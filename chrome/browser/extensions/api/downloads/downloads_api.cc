// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/downloads/downloads_api.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_danger_prompt.h"
#include "chrome/browser/download/download_file_icon_extractor.h"
#include "chrome/browser/download/download_open_prompt.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_query.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/icon_loader.h"
#include "chrome/browser/icon_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/extensions/api/downloads.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/warning_service.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/event_dispatcher.mojom-forward.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/base/filename_util.h"
#include "net/base/load_flags.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

using content::BrowserContext;
using content::BrowserThread;
using content::DownloadManager;
using download::DownloadItem;
using download::DownloadPathReservationTracker;
using extensions::mojom::APIPermissionID;

namespace download_extension_errors {

const char kEmptyFile[] = "Filename not yet determined";
const char kFileAlreadyDeleted[] = "Download file already deleted";
const char kFileNotRemoved[] = "Unable to remove file";
const char kIconNotFound[] = "Icon not found";
const char kInvalidDangerType[] = "Invalid danger type";
const char kInvalidFilename[] = "Invalid filename";
const char kInvalidFilter[] = "Invalid query filter";
const char kInvalidHeaderName[] = "Invalid request header name";
const char kInvalidHeaderUnsafe[] = "Unsafe request header name";
const char kInvalidHeaderValue[] = "Invalid request header value";
const char kInvalidId[] = "Invalid downloadId";
const char kInvalidOrderBy[] = "Invalid orderBy field";
const char kInvalidQueryLimit[] = "Invalid query limit";
const char kInvalidState[] = "Invalid state";
const char kInvalidURL[] = "Invalid URL";
const char kInvisibleContext[] =
    "Javascript execution context is not visible "
    "(tab, window, popup bubble)";
const char kNotComplete[] = "Download must be complete";
const char kNotDangerous[] = "Download must be dangerous";
const char kNotInProgress[] = "Download must be in progress";
const char kNotResumable[] = "DownloadItem.canResume must be true";
const char kOpenPermission[] = "The \"downloads.open\" permission is required";
const char kShelfDisabled[] = "Another extension has disabled the shelf";
const char kShelfPermission[] =
    "downloads.setShelfEnabled requires the "
    "\"downloads.shelf\" permission";
const char kTooManyListeners[] =
    "Each extension may have at most one "
    "onDeterminingFilename listener between all of its renderer execution "
    "contexts.";
const char kUiDisabled[] = "Another extension has disabled the download UI";
const char kUiPermission[] =
    "downloads.setUiOptions requires the "
    "\"downloads.ui\" permission";
const char kUnexpectedDeterminer[] = "Unexpected determineFilename call";
const char kUserGesture[] = "User gesture required";

}  // namespace download_extension_errors

namespace extensions {

namespace {

namespace downloads = api::downloads;

// Default icon size for getFileIcon() in pixels.
const int kDefaultIconSize = 32;

// Parameter keys
const char kBytesReceivedKey[] = "bytesReceived";
const char kCanResumeKey[] = "canResume";
const char kDangerKey[] = "danger";
const char kEndTimeKey[] = "endTime";
const char kEndedAfterKey[] = "endedAfter";
const char kEndedBeforeKey[] = "endedBefore";
const char kErrorKey[] = "error";
const char kExistsKey[] = "exists";
const char kFileSizeKey[] = "fileSize";
const char kFilenameKey[] = "filename";
const char kFilenameRegexKey[] = "filenameRegex";
const char kIdKey[] = "id";
const char kMimeKey[] = "mime";
const char kPausedKey[] = "paused";
const char kQueryKey[] = "query";
const char kStartTimeKey[] = "startTime";
const char kStartedAfterKey[] = "startedAfter";
const char kStartedBeforeKey[] = "startedBefore";
const char kStateKey[] = "state";
const char kTotalBytesGreaterKey[] = "totalBytesGreater";
const char kTotalBytesKey[] = "totalBytes";
const char kTotalBytesLessKey[] = "totalBytesLess";
const char kUrlKey[] = "url";
const char kUrlRegexKey[] = "urlRegex";
const char kFinalUrlKey[] = "finalUrl";
const char kFinalUrlRegexKey[] = "finalUrlRegex";

extensions::api::downloads::DangerType ConvertDangerType(
    download::DownloadDangerType danger) {
  switch (danger) {
    case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
      return extensions::api::downloads::DangerType::kSafe;
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      return extensions::api::downloads::DangerType::kFile;
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
      return extensions::api::downloads::DangerType::kUrl;
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
      return extensions::api::downloads::DangerType::kContent;
    case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
      return extensions::api::downloads::DangerType::kSafe;
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
      return extensions::api::downloads::DangerType::kUncommon;
    case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
      return extensions::api::downloads::DangerType::kAccepted;
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
      return extensions::api::downloads::DangerType::kHost;
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      return extensions::api::downloads::DangerType::kUnwanted;
    case download::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY:
      return extensions::api::downloads::DangerType::kAllowlistedByPolicy;
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
      return extensions::api::downloads::DangerType::kAsyncScanning;
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING:
      return extensions::api::downloads::DangerType::
          kAsyncLocalPasswordScanning;
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
      return extensions::api::downloads::DangerType::kPasswordProtected;
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
      return extensions::api::downloads::DangerType::kBlockedTooLarge;
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
      return extensions::api::downloads::DangerType::kSensitiveContentWarning;
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK:
      return extensions::api::downloads::DangerType::kSensitiveContentBlock;
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
      return extensions::api::downloads::DangerType::kDeepScannedSafe;
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
      return extensions::api::downloads::DangerType::
          kDeepScannedOpenedDangerous;
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING:
      return extensions::api::downloads::DangerType::kPromptForScanning;
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
      return extensions::api::downloads::DangerType::kAccountCompromise;
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED:
      return extensions::api::downloads::DangerType::kDeepScannedFailed;
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING:
      return extensions::api::downloads::DangerType::
          kPromptForLocalPasswordScanning;
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_SCAN_FAILED:
      return extensions::api::downloads::DangerType::kBlockedScanFailed;
    case download::DOWNLOAD_DANGER_TYPE_MAX:
      NOTREACHED_IN_MIGRATION();
      return extensions::api::downloads::DangerType::kMaxValue;
  }
}

download::DownloadDangerType DangerEnumFromString(const std::string& danger) {
  extensions::api::downloads::DangerType danger_type =
      api::downloads::ParseDangerType(danger);
  for (size_t i = 0; i < download::DOWNLOAD_DANGER_TYPE_MAX; i++) {
    if (ConvertDangerType(static_cast<download::DownloadDangerType>(i)) ==
        danger_type) {
      return static_cast<download::DownloadDangerType>(i);
    }
  }
  return download::DOWNLOAD_DANGER_TYPE_MAX;
}

extensions::api::downloads::State ConvertState(
    download::DownloadItem::DownloadState state) {
  switch (state) {
    case download::DownloadItem::IN_PROGRESS:
      return extensions::api::downloads::State::kInProgress;
    case download::DownloadItem::COMPLETE:
      return extensions::api::downloads::State::kComplete;
    case download::DownloadItem::CANCELLED:
      return extensions::api::downloads::State::kInterrupted;
    case download::DownloadItem::INTERRUPTED:
      return extensions::api::downloads::State::kInterrupted;
    case download::DownloadItem::MAX_DOWNLOAD_STATE:
      return extensions::api::downloads::State::kMaxValue;
  }
}

download::DownloadItem::DownloadState StateEnumFromString(
    const std::string& state) {
  extensions::api::downloads::State extension_state =
      extensions::api::downloads::ParseState(state);
  for (size_t i = 0; i < download::DownloadItem::MAX_DOWNLOAD_STATE; i++) {
    if (ConvertState(static_cast<download::DownloadItem::DownloadState>(i)) ==
        extension_state) {
      return static_cast<download::DownloadItem::DownloadState>(i);
    }
  }

  return download::DownloadItem::MAX_DOWNLOAD_STATE;
}

extensions::api::downloads::InterruptReason ConvertInterruptReason(
    download::DownloadInterruptReason reason) {
  // Note: Any new entries to this switch, as a result of a new keys to
  // DownloadInterruptReason must be follow with a corresponding entry in
  // api::downloads::InterruptReason, at
  // chrome/common/extensions/api/downloads.idl.
  switch (reason) {
    case download::DOWNLOAD_INTERRUPT_REASON_NONE:
      return extensions::api::downloads::InterruptReason::kNone;
    case download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED:
      return extensions::api::downloads::InterruptReason::kFileFailed;
    case download::DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED:
      return extensions::api::downloads::InterruptReason::kFileAccessDenied;
    case download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE:
      return extensions::api::downloads::InterruptReason::kFileNoSpace;
    case download::DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG:
      return extensions::api::downloads::InterruptReason::kFileNameTooLong;
    case download::DOWNLOAD_INTERRUPT_REASON_FILE_TOO_LARGE:
      return extensions::api::downloads::InterruptReason::kFileTooLarge;
    case download::DOWNLOAD_INTERRUPT_REASON_FILE_VIRUS_INFECTED:
      return extensions::api::downloads::InterruptReason::kFileVirusInfected;
    case download::DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR:
      return extensions::api::downloads::InterruptReason::kFileTransientError;
    case download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED:
      return extensions::api::downloads::InterruptReason::kFileBlocked;
    case download::DOWNLOAD_INTERRUPT_REASON_FILE_SECURITY_CHECK_FAILED:
      return extensions::api::downloads::InterruptReason::
          kFileSecurityCheckFailed;
    case download::DOWNLOAD_INTERRUPT_REASON_FILE_TOO_SHORT:
      return extensions::api::downloads::InterruptReason::kFileTooShort;
    case download::DOWNLOAD_INTERRUPT_REASON_FILE_HASH_MISMATCH:
      return extensions::api::downloads::InterruptReason::kFileHashMismatch;
    case download::DOWNLOAD_INTERRUPT_REASON_FILE_SAME_AS_SOURCE:
      return extensions::api::downloads::InterruptReason::kFileSameAsSource;
    case download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED:
      return extensions::api::downloads::InterruptReason::kNetworkFailed;
    case download::DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT:
      return extensions::api::downloads::InterruptReason::kNetworkTimeout;
    case download::DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED:
      return extensions::api::downloads::InterruptReason::kNetworkDisconnected;
    case download::DOWNLOAD_INTERRUPT_REASON_NETWORK_SERVER_DOWN:
      return extensions::api::downloads::InterruptReason::kNetworkServerDown;
    case download::DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST:
      return extensions::api::downloads::InterruptReason::
          kNetworkInvalidRequest;
    case download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED:
      return extensions::api::downloads::InterruptReason::kServerFailed;
    case download::DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE:
      return extensions::api::downloads::InterruptReason::kServerNoRange;
    case download::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT:
      return extensions::api::downloads::InterruptReason::kServerBadContent;
    case download::DOWNLOAD_INTERRUPT_REASON_SERVER_UNAUTHORIZED:
      return extensions::api::downloads::InterruptReason::kServerUnauthorized;
    case download::DOWNLOAD_INTERRUPT_REASON_SERVER_CERT_PROBLEM:
      return extensions::api::downloads::InterruptReason::kServerCertProblem;
    case download::DOWNLOAD_INTERRUPT_REASON_SERVER_FORBIDDEN:
      return extensions::api::downloads::InterruptReason::kServerForbidden;
    case download::DOWNLOAD_INTERRUPT_REASON_SERVER_UNREACHABLE:
      return extensions::api::downloads::InterruptReason::kServerUnreachable;
    case download::DOWNLOAD_INTERRUPT_REASON_SERVER_CONTENT_LENGTH_MISMATCH:
      return extensions::api::downloads::InterruptReason::
          kServerContentLengthMismatch;
    case download::DOWNLOAD_INTERRUPT_REASON_SERVER_CROSS_ORIGIN_REDIRECT:
      return extensions::api::downloads::InterruptReason::
          kServerCrossOriginRedirect;
    case download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED:
      return extensions::api::downloads::InterruptReason::kUserCanceled;
    case download::DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN:
      return extensions::api::downloads::InterruptReason::kUserShutdown;
    case download::DOWNLOAD_INTERRUPT_REASON_CRASH:
      return extensions::api::downloads::InterruptReason::kCrash;
  }
}

base::Value::Dict DownloadItemToJSON(DownloadItem* download_item,
                                     content::BrowserContext* browser_context) {
  extensions::api::downloads::DownloadItem item;
  item.exists = !download_item->GetFileExternallyRemoved();
  item.id = static_cast<int>(download_item->GetId());
  const GURL& url = download_item->GetOriginalUrl();
  item.url = url.is_valid() ? url.spec() : std::string();
  const GURL& finalUrl = download_item->GetURL();
  item.final_url = finalUrl.is_valid() ? finalUrl.spec() : std::string();
  const GURL& referrer = download_item->GetReferrerUrl();
  item.referrer = referrer.is_valid() ? referrer.spec() : std::string();
  item.filename =
      base::UTF16ToUTF8(download_item->GetTargetFilePath().LossyDisplayName());
  item.danger = ConvertDangerType(download_item->GetDangerType());
  item.state = ConvertState(download_item->GetState());
  item.can_resume = download_item->CanResume();
  item.paused = download_item->IsPaused();
  item.mime = download_item->GetMimeType();
  item.start_time = base::TimeFormatAsIso8601(download_item->GetStartTime());
  item.bytes_received = static_cast<double>(download_item->GetReceivedBytes());
  item.total_bytes = static_cast<double>(download_item->GetTotalBytes());
  item.incognito = browser_context->IsOffTheRecord();
  if (download_item->GetState() == DownloadItem::INTERRUPTED) {
    item.error = ConvertInterruptReason(download_item->GetLastReason());
  } else if (download_item->GetState() == DownloadItem::CANCELLED) {
    item.error = ConvertInterruptReason(
        download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED);
  }
  if (!download_item->GetEndTime().is_null()) {
    item.end_time = base::TimeFormatAsIso8601(download_item->GetEndTime());
  }
  base::TimeDelta time_remaining;
  if (download_item->TimeRemaining(&time_remaining)) {
    base::Time now = base::Time::Now();
    item.estimated_end_time = base::TimeFormatAsIso8601(now + time_remaining);
  }
  DownloadedByExtension* by_ext = DownloadedByExtension::Get(download_item);
  if (by_ext) {
    item.by_extension_id = by_ext->id();
    item.by_extension_name = by_ext->name();
    // Lookup the extension's current name() in case the user changed their
    // language. This won't work if the extension was uninstalled, so the name
    // might be the wrong language.
    const Extension* extension =
        ExtensionRegistry::Get(browser_context)
            ->GetExtensionById(by_ext->id(), ExtensionRegistry::EVERYTHING);
    if (extension)
      item.by_extension_name = extension->name();
  }
  // TODO(benjhayden): Implement fileSize.
  item.file_size = static_cast<double>(download_item->GetTotalBytes());
  return item.ToValue();
}

class DownloadFileIconExtractorImpl : public DownloadFileIconExtractor {
 public:
  DownloadFileIconExtractorImpl() {}

  ~DownloadFileIconExtractorImpl() override {}

  bool ExtractIconURLForPath(const base::FilePath& path,
                             float scale,
                             IconLoader::IconSize icon_size,
                             IconURLCallback callback) override;

 private:
  void OnIconLoadComplete(float scale,
                          IconURLCallback callback,
                          gfx::Image icon);

  base::CancelableTaskTracker cancelable_task_tracker_;
};

bool DownloadFileIconExtractorImpl::ExtractIconURLForPath(
    const base::FilePath& path,
    float scale,
    IconLoader::IconSize icon_size,
    IconURLCallback callback) {
  IconManager* im = g_browser_process->icon_manager();
  // The contents of the file at |path| may have changed since a previous
  // request, in which case the associated icon may also have changed.
  // Therefore, always call LoadIcon instead of attempting a LookupIcon.
  im->LoadIcon(
      path, icon_size, scale,
      base::BindOnce(&DownloadFileIconExtractorImpl::OnIconLoadComplete,
                     base::Unretained(this), scale, std::move(callback)),
      &cancelable_task_tracker_);
  return true;
}

void DownloadFileIconExtractorImpl::OnIconLoadComplete(float scale,
                                                       IconURLCallback callback,
                                                       gfx::Image icon) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run(
      icon.IsEmpty()
          ? std::string()
          : webui::GetBitmapDataUrl(
                icon.ToImageSkia()->GetRepresentation(scale).GetBitmap()));
}

IconLoader::IconSize IconLoaderSizeFromPixelSize(int pixel_size) {
  switch (pixel_size) {
    case 16:
      return IconLoader::SMALL;
    case 32:
      return IconLoader::NORMAL;
    default:
      NOTREACHED_IN_MIGRATION();
      return IconLoader::NORMAL;
  }
}

using FilterTypeMap = base::flat_map<std::string, DownloadQuery::FilterType>;
void AppendFilter(const char* name,
                  DownloadQuery::FilterType type,
                  std::vector<FilterTypeMap::value_type>* v) {
  v->emplace_back(name, type);
}

void InitFilterTypeMap(FilterTypeMap* filter_types_ptr) {
  // Initialize the map in one shot by storing to a vector and assigning.
  std::vector<FilterTypeMap::value_type> v;

  AppendFilter(kBytesReceivedKey, DownloadQuery::FILTER_BYTES_RECEIVED, &v);

  AppendFilter(kBytesReceivedKey, DownloadQuery::FILTER_BYTES_RECEIVED, &v);
  AppendFilter(kExistsKey, DownloadQuery::FILTER_EXISTS, &v);
  AppendFilter(kFilenameKey, DownloadQuery::FILTER_FILENAME, &v);
  AppendFilter(kFilenameRegexKey, DownloadQuery::FILTER_FILENAME_REGEX, &v);
  AppendFilter(kMimeKey, DownloadQuery::FILTER_MIME, &v);
  AppendFilter(kPausedKey, DownloadQuery::FILTER_PAUSED, &v);
  AppendFilter(kQueryKey, DownloadQuery::FILTER_QUERY, &v);
  AppendFilter(kEndedAfterKey, DownloadQuery::FILTER_ENDED_AFTER, &v);
  AppendFilter(kEndedBeforeKey, DownloadQuery::FILTER_ENDED_BEFORE, &v);
  AppendFilter(kEndTimeKey, DownloadQuery::FILTER_END_TIME, &v);
  AppendFilter(kStartedAfterKey, DownloadQuery::FILTER_STARTED_AFTER, &v);
  AppendFilter(kStartedBeforeKey, DownloadQuery::FILTER_STARTED_BEFORE, &v);
  AppendFilter(kStartTimeKey, DownloadQuery::FILTER_START_TIME, &v);
  AppendFilter(kTotalBytesKey, DownloadQuery::FILTER_TOTAL_BYTES, &v);
  AppendFilter(kTotalBytesGreaterKey, DownloadQuery::FILTER_TOTAL_BYTES_GREATER,
               &v);
  AppendFilter(kTotalBytesLessKey, DownloadQuery::FILTER_TOTAL_BYTES_LESS, &v);
  AppendFilter(kUrlKey, DownloadQuery::FILTER_ORIGINAL_URL, &v);
  AppendFilter(kUrlRegexKey, DownloadQuery::FILTER_ORIGINAL_URL_REGEX, &v);
  AppendFilter(kFinalUrlKey, DownloadQuery::FILTER_URL, &v);
  AppendFilter(kFinalUrlRegexKey, DownloadQuery::FILTER_URL_REGEX, &v);

  *filter_types_ptr = FilterTypeMap(std::move(v));
}

using SortTypeMap = base::flat_map<std::string, DownloadQuery::SortType>;
void AppendFilter(const char* name,
                  DownloadQuery::SortType type,
                  std::vector<SortTypeMap::value_type>* v) {
  v->emplace_back(name, type);
}

void InitSortTypeMap(SortTypeMap* sorter_types_ptr) {
  // Initialize the map in one shot by storing to a vector and assigning.
  std::vector<SortTypeMap::value_type> v;

  AppendFilter(kBytesReceivedKey, DownloadQuery::SORT_BYTES_RECEIVED, &v);
  AppendFilter(kDangerKey, DownloadQuery::SORT_DANGER, &v);
  AppendFilter(kEndTimeKey, DownloadQuery::SORT_END_TIME, &v);
  AppendFilter(kExistsKey, DownloadQuery::SORT_EXISTS, &v);
  AppendFilter(kFilenameKey, DownloadQuery::SORT_FILENAME, &v);
  AppendFilter(kMimeKey, DownloadQuery::SORT_MIME, &v);
  AppendFilter(kPausedKey, DownloadQuery::SORT_PAUSED, &v);
  AppendFilter(kStartTimeKey, DownloadQuery::SORT_START_TIME, &v);
  AppendFilter(kStateKey, DownloadQuery::SORT_STATE, &v);
  AppendFilter(kTotalBytesKey, DownloadQuery::SORT_TOTAL_BYTES, &v);
  AppendFilter(kUrlKey, DownloadQuery::SORT_ORIGINAL_URL, &v);
  AppendFilter(kFinalUrlKey, DownloadQuery::SORT_URL, &v);

  *sorter_types_ptr = SortTypeMap(std::move(v));
}

bool ShouldExport(const DownloadItem& download_item) {
  return !download_item.IsTemporary() &&
         download_item.GetDownloadSource() !=
             download::DownloadSource::INTERNAL_API;
}

// Set |manager| to the on-record DownloadManager, and |incognito_manager| to
// the off-record DownloadManager if one exists and is requested via
// |include_incognito|. This should work regardless of whether |context| is
// original or incognito.
void GetManagers(content::BrowserContext* context,
                 bool include_incognito,
                 DownloadManager** manager,
                 DownloadManager** incognito_manager) {
  Profile* profile = Profile::FromBrowserContext(context);
  *manager = profile->GetOriginalProfile()->GetDownloadManager();
  if (profile->HasPrimaryOTRProfile() &&
      (include_incognito || profile->IsOffTheRecord())) {
    *incognito_manager =
        profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)
            ->GetDownloadManager();
  } else {
    *incognito_manager = nullptr;
  }
}

// Set |service| to the on-record DownloadCoreService, |incognito_service| to
// the off-record DownloadCoreService if one exists and is requested via
// |include_incognito|. This should work regardless of whether |context| is
// original or incognito.
void GetDownloadCoreServices(content::BrowserContext* context,
                             bool include_incognito,
                             DownloadCoreService** service,
                             DownloadCoreService** incognito_service) {
  DownloadManager* manager = nullptr;
  DownloadManager* incognito_manager = nullptr;
  GetManagers(context, include_incognito, &manager, &incognito_manager);
  if (manager) {
    *service = DownloadCoreServiceFactory::GetForBrowserContext(
        manager->GetBrowserContext());
  }
  if (incognito_manager) {
    *incognito_service = DownloadCoreServiceFactory::GetForBrowserContext(
        incognito_manager->GetBrowserContext());
  }
}

void MaybeSetUiEnabled(DownloadCoreService* service,
                       DownloadCoreService* incognito_service,
                       const Extension* extension,
                       bool enabled) {
  if (service) {
    service->GetExtensionEventRouter()->SetUiEnabled(extension, enabled);
  }
  if (incognito_service) {
    incognito_service->GetExtensionEventRouter()->SetUiEnabled(extension,
                                                               enabled);
  }
}

DownloadItem* GetDownload(content::BrowserContext* context,
                          bool include_incognito,
                          int id) {
  DownloadManager* manager = nullptr;
  DownloadManager* incognito_manager = nullptr;
  GetManagers(context, include_incognito, &manager, &incognito_manager);
  DownloadItem* download_item = manager->GetDownload(id);
  if (!download_item && incognito_manager)
    download_item = incognito_manager->GetDownload(id);
  return download_item;
}

// Corresponds to |DownloadFunctions| enumeration in histograms.xml. Please
// keep these in sync.
enum DownloadsFunctionName {
  DOWNLOADS_FUNCTION_DOWNLOAD = 0,
  DOWNLOADS_FUNCTION_SEARCH = 1,
  DOWNLOADS_FUNCTION_PAUSE = 2,
  DOWNLOADS_FUNCTION_RESUME = 3,
  DOWNLOADS_FUNCTION_CANCEL = 4,
  DOWNLOADS_FUNCTION_ERASE = 5,
  // 6 unused
  DOWNLOADS_FUNCTION_ACCEPT_DANGER = 7,
  DOWNLOADS_FUNCTION_SHOW = 8,
  DOWNLOADS_FUNCTION_DRAG = 9,
  DOWNLOADS_FUNCTION_GET_FILE_ICON = 10,
  DOWNLOADS_FUNCTION_OPEN = 11,
  DOWNLOADS_FUNCTION_REMOVE_FILE = 12,
  DOWNLOADS_FUNCTION_SHOW_DEFAULT_FOLDER = 13,
  DOWNLOADS_FUNCTION_SET_SHELF_ENABLED = 14,
  DOWNLOADS_FUNCTION_DETERMINE_FILENAME = 15,
  DOWNLOADS_FUNCTION_SET_UI_OPTIONS = 16,
  // Insert new values here, not at the beginning.
  DOWNLOADS_FUNCTION_LAST
};

void RecordApiFunctions(DownloadsFunctionName function) {
  UMA_HISTOGRAM_ENUMERATION("Download.ApiFunctions", function,
                            DOWNLOADS_FUNCTION_LAST);
}

void CompileDownloadQueryOrderBy(const std::vector<std::string>& order_by_strs,
                                 std::string* error,
                                 DownloadQuery* query) {
  // TODO(benjhayden): Consider switching from LazyInstance to explicit string
  // comparisons.
  static base::LazyInstance<SortTypeMap>::DestructorAtExit sorter_types =
      LAZY_INSTANCE_INITIALIZER;
  if (sorter_types.Get().empty())
    InitSortTypeMap(sorter_types.Pointer());

  for (auto term_str : order_by_strs) {
    if (term_str.empty())
      continue;
    DownloadQuery::SortDirection direction = DownloadQuery::ASCENDING;
    if (term_str[0] == '-') {
      direction = DownloadQuery::DESCENDING;
      term_str = term_str.substr(1);
    }
    SortTypeMap::const_iterator sorter_type = sorter_types.Get().find(term_str);
    if (sorter_type == sorter_types.Get().end()) {
      *error = download_extension_errors::kInvalidOrderBy;
      return;
    }
    query->AddSorter(sorter_type->second, direction);
  }
}

void RunDownloadQuery(const downloads::DownloadQuery& query_in,
                      DownloadManager* manager,
                      DownloadManager* incognito_manager,
                      std::string* error,
                      DownloadQuery::DownloadVector* results) {
  // TODO(benjhayden): Consider switching from LazyInstance to explicit string
  // comparisons.
  static base::LazyInstance<FilterTypeMap>::DestructorAtExit filter_types =
      LAZY_INSTANCE_INITIALIZER;
  if (filter_types.Get().empty())
    InitFilterTypeMap(filter_types.Pointer());

  DownloadQuery query_out;

  size_t limit = 1000;
  if (query_in.limit) {
    if (*query_in.limit < 0) {
      *error = download_extension_errors::kInvalidQueryLimit;
      return;
    }
    limit = *query_in.limit;
  }
  if (limit > 0) {
    query_out.Limit(limit);
  }

  std::string state_string = downloads::ToString(query_in.state);
  if (!state_string.empty()) {
    DownloadItem::DownloadState state = StateEnumFromString(state_string);
    if (state == DownloadItem::MAX_DOWNLOAD_STATE) {
      *error = download_extension_errors::kInvalidState;
      return;
    }
    query_out.AddFilter(state);
  }
  std::string danger_string = downloads::ToString(query_in.danger);
  if (!danger_string.empty()) {
    download::DownloadDangerType danger_type =
        DangerEnumFromString(danger_string);
    if (danger_type == download::DOWNLOAD_DANGER_TYPE_MAX) {
      *error = download_extension_errors::kInvalidDangerType;
      return;
    }
    query_out.AddFilter(danger_type);
  }
  if (query_in.order_by) {
    CompileDownloadQueryOrderBy(*query_in.order_by, error, &query_out);
    if (!error->empty())
      return;
  }

  for (const auto query_json_field : query_in.ToValue()) {
    FilterTypeMap::const_iterator filter_type =
        filter_types.Get().find(query_json_field.first);
    if (filter_type != filter_types.Get().end()) {
      if (!query_out.AddFilter(filter_type->second, query_json_field.second)) {
        *error = download_extension_errors::kInvalidFilter;
        return;
      }
    }
  }

  DownloadQuery::DownloadVector all_items;
  if (query_in.id) {
    DownloadItem* download_item = manager->GetDownload(*query_in.id);
    if (!download_item && incognito_manager)
      download_item = incognito_manager->GetDownload(*query_in.id);
    if (download_item)
      all_items.push_back(download_item);
  } else {
    manager->GetAllDownloads(&all_items);
    if (incognito_manager)
      incognito_manager->GetAllDownloads(&all_items);
  }
  query_out.AddFilter(base::BindRepeating(&ShouldExport));
  query_out.Search(all_items.begin(), all_items.end(), results);
}

download::DownloadPathReservationTracker::FilenameConflictAction
ConvertConflictAction(downloads::FilenameConflictAction action) {
  switch (action) {
    case downloads::FilenameConflictAction::kNone:
    case downloads::FilenameConflictAction::kUniquify:
      return DownloadPathReservationTracker::UNIQUIFY;
    case downloads::FilenameConflictAction::kOverwrite:
      return DownloadPathReservationTracker::OVERWRITE;
    case downloads::FilenameConflictAction::kPrompt:
      return DownloadPathReservationTracker::PROMPT;
  }
  NOTREACHED_IN_MIGRATION();
  return download::DownloadPathReservationTracker::UNIQUIFY;
}

class ExtensionDownloadsEventRouterData : public base::SupportsUserData::Data {
 public:
  static ExtensionDownloadsEventRouterData* Get(DownloadItem* download_item) {
    base::SupportsUserData::Data* data = download_item->GetUserData(kKey);
    return (data == nullptr)
               ? nullptr
               : static_cast<ExtensionDownloadsEventRouterData*>(data);
  }

  static void Remove(DownloadItem* download_item) {
    download_item->RemoveUserData(kKey);
  }

  explicit ExtensionDownloadsEventRouterData(DownloadItem* download_item,
                                             base::Value::Dict json_item)
      : updated_(0),
        changed_fired_(0),
        json_(std::move(json_item)),
        creator_conflict_action_(downloads::FilenameConflictAction::kUniquify),
        determined_conflict_action_(
            downloads::FilenameConflictAction::kUniquify),
        is_download_completed_(download_item->GetState() ==
                               DownloadItem::COMPLETE),
        is_completed_download_deleted_(
            download_item->GetFileExternallyRemoved()) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    download_item->SetUserData(kKey, base::WrapUnique(this));
  }

  ExtensionDownloadsEventRouterData(const ExtensionDownloadsEventRouterData&) =
      delete;
  ExtensionDownloadsEventRouterData& operator=(
      const ExtensionDownloadsEventRouterData&) = delete;

  ~ExtensionDownloadsEventRouterData() override = default;

  void set_is_download_completed(bool is_download_completed) {
    is_download_completed_ = is_download_completed;
  }
  void set_is_completed_download_deleted(bool is_completed_download_deleted) {
    is_completed_download_deleted_ = is_completed_download_deleted;
  }
  bool is_download_completed() { return is_download_completed_; }
  bool is_completed_download_deleted() {
    return is_completed_download_deleted_;
  }
  const base::Value::Dict& json() const { return json_; }
  void set_json(base::Value::Dict json_item) { json_ = std::move(json_item); }

  void OnItemUpdated() { ++updated_; }
  void OnChangedFired() { ++changed_fired_; }

  static void SetDetermineFilenameTimeoutSecondsForTesting(int s) {
    determine_filename_timeout_s_ = s;
  }

  void BeginFilenameDetermination(
      ExtensionDownloadsEventRouter::FilenameChangedCallback filename_changed) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    ClearPendingDeterminers();
    filename_changed_ = std::move(filename_changed);
    determined_filename_ = creator_suggested_filename_;
    determined_conflict_action_ = creator_conflict_action_;
    // determiner_.install_time should default to 0 so that creator suggestions
    // should be lower priority than any actual onDeterminingFilename listeners.

    // Ensure that the callback is called within a time limit.
    weak_ptr_factory_ = std::make_unique<
        base::WeakPtrFactory<ExtensionDownloadsEventRouterData>>(this);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &ExtensionDownloadsEventRouterData::DetermineFilenameTimeout,
            weak_ptr_factory_->GetWeakPtr()),
        base::Seconds(determine_filename_timeout_s_));
  }

  void DetermineFilenameTimeout() { CallFilenameCallback(); }

  void CallFilenameCallback() {
    if (!filename_changed_)
      return;

    std::move(filename_changed_)
        .Run(determined_filename_,
             ConvertConflictAction(determined_conflict_action_));
  }

  void ClearPendingDeterminers() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    determined_filename_.clear();
    determined_conflict_action_ = downloads::FilenameConflictAction::kUniquify;
    determiner_ = DeterminerInfo();
    filename_changed_.Reset();
    weak_ptr_factory_.reset();
    determiners_.clear();
  }

  void DeterminerRemoved(const ExtensionId& extension_id) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    for (auto iter = determiners_.begin(); iter != determiners_.end();) {
      if (iter->extension_id == extension_id) {
        iter = determiners_.erase(iter);
      } else {
        ++iter;
      }
    }
    // If we just removed the last unreported determiner, then we need to call a
    // callback.
    CheckAllDeterminersCalled();
  }

  void AddPendingDeterminer(const ExtensionId& extension_id,
                            const base::Time& installed) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    for (auto& determiner : determiners_) {
      if (determiner.extension_id == extension_id) {
        DCHECK(false) << extension_id;
        return;
      }
    }
    determiners_.push_back(DeterminerInfo(extension_id, installed));
  }

  bool DeterminerAlreadyReported(const ExtensionId& extension_id) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    for (auto& determiner : determiners_) {
      if (determiner.extension_id == extension_id) {
        return determiner.reported;
      }
    }
    return false;
  }

  void CreatorSuggestedFilename(
      const base::FilePath& filename,
      downloads::FilenameConflictAction conflict_action) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    creator_suggested_filename_ = filename;
    creator_conflict_action_ = conflict_action;
  }

  base::FilePath creator_suggested_filename() const {
    return creator_suggested_filename_;
  }

  downloads::FilenameConflictAction creator_conflict_action() const {
    return creator_conflict_action_;
  }

  void ResetCreatorSuggestion() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    creator_suggested_filename_.clear();
    creator_conflict_action_ = downloads::FilenameConflictAction::kUniquify;
  }

  // Returns false if this |extension_id| was not expected or if this
  // |extension_id| has already reported. The caller is responsible for
  // validating |filename|.
  bool DeterminerCallback(content::BrowserContext* browser_context,
                          const ExtensionId& extension_id,
                          const base::FilePath& filename,
                          downloads::FilenameConflictAction conflict_action) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    bool found_info = false;
    for (auto& determiner : determiners_) {
      if (determiner.extension_id == extension_id) {
        found_info = true;
        if (determiner.reported)
          return false;
        determiner.reported = true;
        // Do not use filename if another determiner has already overridden the
        // filename and they take precedence. Extensions that were installed
        // later take precedence over previous extensions.
        if (!filename.empty() ||
            (conflict_action != downloads::FilenameConflictAction::kUniquify)) {
          WarningSet warnings;
          ExtensionId winner_extension_id;
          ExtensionDownloadsEventRouter::DetermineFilenameInternal(
              filename, conflict_action, determiner.extension_id,
              determiner.install_time, determiner_.extension_id,
              determiner_.install_time, &winner_extension_id,
              &determined_filename_, &determined_conflict_action_, &warnings);
          if (!warnings.empty())
            WarningService::NotifyWarningsOnUI(browser_context, warnings);
          if (winner_extension_id == determiner.extension_id)
            determiner_ = determiner;
        }
        break;
      }
    }
    if (!found_info)
      return false;
    CheckAllDeterminersCalled();
    return true;
  }

 private:
  static int determine_filename_timeout_s_;

  struct DeterminerInfo {
    DeterminerInfo();
    DeterminerInfo(const ExtensionId& e_id, const base::Time& installed);
    ~DeterminerInfo();

    ExtensionId extension_id;
    base::Time install_time;
    bool reported;
  };
  typedef std::vector<DeterminerInfo> DeterminerInfoVector;

  static const char kKey[];

  // This is safe to call even while not waiting for determiners to call back;
  // in that case, the callbacks will be null so they won't be Run.
  void CheckAllDeterminersCalled() {
    for (auto& determiner : determiners_) {
      if (!determiner.reported)
        return;
    }
    CallFilenameCallback();

    // Don't clear determiners_ immediately in case there's a second listener
    // for one of the extensions, so that DetermineFilename can return
    // kTooManyListeners. After a few seconds, DetermineFilename will return
    // kUnexpectedDeterminer instead of kTooManyListeners so that determiners_
    // doesn't keep hogging memory.
    weak_ptr_factory_ = std::make_unique<
        base::WeakPtrFactory<ExtensionDownloadsEventRouterData>>(this);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &ExtensionDownloadsEventRouterData::ClearPendingDeterminers,
            weak_ptr_factory_->GetWeakPtr()),
        base::Seconds(15));
  }

  int updated_;
  int changed_fired_;
  // Dictionary representing the current state of the download. It is cleared
  // when download completes.
  base::Value::Dict json_;

  ExtensionDownloadsEventRouter::FilenameChangedCallback filename_changed_;

  DeterminerInfoVector determiners_;

  base::FilePath creator_suggested_filename_;
  downloads::FilenameConflictAction creator_conflict_action_;
  base::FilePath determined_filename_;
  downloads::FilenameConflictAction determined_conflict_action_;
  DeterminerInfo determiner_;

  // Whether a download is complete and whether the completed download is
  // deleted.
  bool is_download_completed_;
  bool is_completed_download_deleted_;

  std::unique_ptr<base::WeakPtrFactory<ExtensionDownloadsEventRouterData>>
      weak_ptr_factory_;
};

int ExtensionDownloadsEventRouterData::determine_filename_timeout_s_ = 15;

ExtensionDownloadsEventRouterData::DeterminerInfo::DeterminerInfo(
    const ExtensionId& e_id,
    const base::Time& installed)
    : extension_id(e_id), install_time(installed), reported(false) {}

ExtensionDownloadsEventRouterData::DeterminerInfo::DeterminerInfo()
    : reported(false) {}

ExtensionDownloadsEventRouterData::DeterminerInfo::~DeterminerInfo() {}

const char ExtensionDownloadsEventRouterData::kKey[] =
    "DownloadItem ExtensionDownloadsEventRouterData";

bool OnDeterminingFilenameWillDispatchCallback(
    bool* any_determiners,
    ExtensionDownloadsEventRouterData* data,
    content::BrowserContext* browser_context,
    mojom::ContextType target_context,
    const Extension* extension,
    const base::Value::Dict* listener_filter,
    std::optional<base::Value::List>& event_args_out,
    mojom::EventFilteringInfoPtr& event_filtering_info_out) {
  *any_determiners = true;
  base::Time installed =
      ExtensionPrefs::Get(browser_context)->GetLastUpdateTime(extension->id());
  data->AddPendingDeterminer(extension->id(), installed);
  return true;
}

bool Fault(bool error, const char* message_in, std::string* message_out) {
  if (!error)
    return false;
  *message_out = message_in;
  return true;
}

bool InvalidId(DownloadItem* valid_item, std::string* message_out) {
  return Fault(!valid_item, download_extension_errors::kInvalidId, message_out);
}

bool IsDownloadDeltaField(const std::string& field) {
  return ((field == kUrlKey) || (field == kFinalUrlKey) ||
          (field == kFilenameKey) || (field == kDangerKey) ||
          (field == kMimeKey) || (field == kStartTimeKey) ||
          (field == kEndTimeKey) || (field == kStateKey) ||
          (field == kCanResumeKey) || (field == kPausedKey) ||
          (field == kErrorKey) || (field == kTotalBytesKey) ||
          (field == kFileSizeKey) || (field == kExistsKey));
}

}  // namespace

const char DownloadedByExtension::kKey[] = "DownloadItem DownloadedByExtension";

DownloadedByExtension* DownloadedByExtension::Get(
    download::DownloadItem* item) {
  base::SupportsUserData::Data* data = item->GetUserData(kKey);
  return (data == nullptr) ? nullptr
                           : static_cast<DownloadedByExtension*>(data);
}

DownloadedByExtension::DownloadedByExtension(download::DownloadItem* item,
                                             const ExtensionId& id,
                                             const std::string& name)
    : id_(id), name_(name) {
  item->SetUserData(kKey, base::WrapUnique(this));
}

DownloadsDownloadFunction::DownloadsDownloadFunction() {}

DownloadsDownloadFunction::~DownloadsDownloadFunction() {}

ExtensionFunction::ResponseAction DownloadsDownloadFunction::Run() {
  std::optional<downloads::Download::Params> params =
      downloads::Download::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  const downloads::DownloadOptions& options = params->options;
  GURL download_url(options.url);
  std::string error;
  if (Fault(!download_url.is_valid(), download_extension_errors::kInvalidURL,
            &error))
    return RespondNow(Error(std::move(error)));

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("downloads_api_run_async", R"(
        semantics {
          sender: "Downloads API"
          description:
            "This request is made when an extension makes an API call to "
            "download a file."
          trigger:
            "An API call from an extension, can be in response to user input "
            "or autonomously."
          data:
            "The extension may provide any data that it has permission to "
            "access, or is provided to it by the user."
          destination: OTHER
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "This feature cannot be disabled in settings, but disabling all "
            "extensions will prevent it."
          chrome_policy {
            ExtensionInstallBlocklist {
              ExtensionInstallBlocklist: {
                entries: '*'
              }
            }
          }
        })");
  std::unique_ptr<download::DownloadUrlParameters> download_params(
      new download::DownloadUrlParameters(
          download_url, source_process_id(),
          render_frame_host() ? render_frame_host()->GetRoutingID() : -1,
          traffic_annotation));
  base::FilePath creator_suggested_filename;
  if (options.filename) {
    // Strip "%" character as it affects environment variables.
    std::string filename;
    base::ReplaceChars(*options.filename, "%", "_", &filename);
    creator_suggested_filename = base::FilePath::FromUTF8Unsafe(filename);
    if (!net::IsSafePortableRelativePath(creator_suggested_filename)) {
      return RespondNow(Error(download_extension_errors::kInvalidFilename));
    }
  }

  if (options.save_as)
    download_params->set_prompt(*options.save_as);

  if (options.headers) {
    for (const downloads::HeaderNameValuePair& header : *options.headers) {
      if (!net::HttpUtil::IsValidHeaderName(header.name)) {
        return RespondNow(Error(download_extension_errors::kInvalidHeaderName));
      }
      if (!net::HttpUtil::IsSafeHeader(header.name, header.value)) {
        return RespondNow(
            Error(download_extension_errors::kInvalidHeaderUnsafe));
      }
      if (!net::HttpUtil::IsValidHeaderValue(header.value)) {
        return RespondNow(
            Error(download_extension_errors::kInvalidHeaderValue));
      }
      download_params->add_request_header(header.name, header.value);
    }
  }

  std::string method_string = downloads::ToString(options.method);
  if (!method_string.empty())
    download_params->set_method(method_string);
  if (options.body) {
    download_params->set_post_body(
        network::ResourceRequestBody::CreateFromBytes(options.body->data(),
                                                      options.body->size()));
  }

  download_params->set_callback(
      base::BindOnce(&DownloadsDownloadFunction::OnStarted, this,
                     creator_suggested_filename, options.conflict_action));
  // Prevent login prompts for 401/407 responses.
  download_params->set_do_not_prompt_for_login(true);
  download_params->set_download_source(download::DownloadSource::EXTENSION_API);

  DownloadManager* manager = browser_context()->GetDownloadManager();
  manager->DownloadUrl(std::move(download_params));
  RecordApiFunctions(DOWNLOADS_FUNCTION_DOWNLOAD);
  return RespondLater();
}

void DownloadsDownloadFunction::OnStarted(
    const base::FilePath& creator_suggested_filename,
    downloads::FilenameConflictAction creator_conflict_action,
    DownloadItem* item,
    download::DownloadInterruptReason interrupt_reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  VLOG(1) << __func__ << " " << item << " " << interrupt_reason;
  if (item) {
    DCHECK_EQ(download::DOWNLOAD_INTERRUPT_REASON_NONE, interrupt_reason);
    Respond(WithArguments(static_cast<int>(item->GetId())));
    if (!creator_suggested_filename.empty() ||
        (creator_conflict_action !=
         downloads::FilenameConflictAction::kUniquify)) {
      ExtensionDownloadsEventRouterData* data =
          ExtensionDownloadsEventRouterData::Get(item);
      if (!data) {
        data = new ExtensionDownloadsEventRouterData(item, base::Value::Dict());
      }
      data->CreatorSuggestedFilename(creator_suggested_filename,
                                     creator_conflict_action);
    }
    new DownloadedByExtension(item, extension()->id(), extension()->name());
    item->UpdateObservers();
  } else {
    DCHECK_NE(download::DOWNLOAD_INTERRUPT_REASON_NONE, interrupt_reason);
    Respond(Error(download::DownloadInterruptReasonToString(interrupt_reason)));
  }
}

DownloadsSearchFunction::DownloadsSearchFunction() {}

DownloadsSearchFunction::~DownloadsSearchFunction() {}

ExtensionFunction::ResponseAction DownloadsSearchFunction::Run() {
  std::optional<downloads::Search::Params> params =
      downloads::Search::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DownloadManager* manager = nullptr;
  DownloadManager* incognito_manager = nullptr;
  GetManagers(browser_context(), include_incognito_information(), &manager,
              &incognito_manager);
  ExtensionDownloadsEventRouter* router =
      DownloadCoreServiceFactory::GetForBrowserContext(
          manager->GetBrowserContext())
          ->GetExtensionEventRouter();
  router->CheckForHistoryFilesRemoval();
  if (incognito_manager) {
    ExtensionDownloadsEventRouter* incognito_router =
        DownloadCoreServiceFactory::GetForBrowserContext(
            incognito_manager->GetBrowserContext())
            ->GetExtensionEventRouter();
    incognito_router->CheckForHistoryFilesRemoval();
  }
  DownloadQuery::DownloadVector results;
  std::string error;
  RunDownloadQuery(params->query, manager, incognito_manager, &error, &results);
  if (!error.empty())
    return RespondNow(Error(std::move(error)));

  base::Value::List json_results;
  for (DownloadManager::DownloadVector::const_iterator it = results.begin();
       it != results.end(); ++it) {
    DownloadItem* download_item = *it;
    uint32_t download_id = download_item->GetId();
    bool off_record =
        ((incognito_manager != nullptr) &&
         (incognito_manager->GetDownload(download_id) != nullptr));
    Profile* profile = Profile::FromBrowserContext(browser_context());
    base::Value::Dict json_item = DownloadItemToJSON(
        *it, off_record
                 ? profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)
                 : profile->GetOriginalProfile());
    json_results.Append(std::move(json_item));
  }
  RecordApiFunctions(DOWNLOADS_FUNCTION_SEARCH);
  return RespondNow(WithArguments(std::move(json_results)));
}

DownloadsPauseFunction::DownloadsPauseFunction() {}

DownloadsPauseFunction::~DownloadsPauseFunction() {}

ExtensionFunction::ResponseAction DownloadsPauseFunction::Run() {
  std::optional<downloads::Pause::Params> params =
      downloads::Pause::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DownloadItem* download_item = GetDownload(
      browser_context(), include_incognito_information(), params->download_id);
  std::string error;
  if (InvalidId(download_item, &error) ||
      Fault(download_item->GetState() != DownloadItem::IN_PROGRESS,
            download_extension_errors::kNotInProgress, &error)) {
    return RespondNow(Error(std::move(error)));
  }
  // If the item is already paused, this is a no-op and the operation will
  // silently succeed.
  download_item->Pause();
  RecordApiFunctions(DOWNLOADS_FUNCTION_PAUSE);
  return RespondNow(NoArguments());
}

DownloadsResumeFunction::DownloadsResumeFunction() {}

DownloadsResumeFunction::~DownloadsResumeFunction() {}

ExtensionFunction::ResponseAction DownloadsResumeFunction::Run() {
  std::optional<downloads::Resume::Params> params =
      downloads::Resume::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DownloadItem* download_item = GetDownload(
      browser_context(), include_incognito_information(), params->download_id);
  std::string error;
  if (InvalidId(download_item, &error) ||
      Fault(download_item->IsPaused() && !download_item->CanResume(),
            download_extension_errors::kNotResumable, &error)) {
    return RespondNow(Error(std::move(error)));
  }
  // Note that if the item isn't paused, this will be a no-op, and the extension
  // call will seem successful.
  download_item->Resume(user_gesture());
  RecordApiFunctions(DOWNLOADS_FUNCTION_RESUME);
  return RespondNow(NoArguments());
}

DownloadsCancelFunction::DownloadsCancelFunction() {}

DownloadsCancelFunction::~DownloadsCancelFunction() {}

ExtensionFunction::ResponseAction DownloadsCancelFunction::Run() {
  std::optional<downloads::Resume::Params> params =
      downloads::Resume::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DownloadItem* download_item = GetDownload(
      browser_context(), include_incognito_information(), params->download_id);
  if (download_item && (download_item->GetState() == DownloadItem::IN_PROGRESS))
    download_item->Cancel(true);
  // |download_item| can be NULL if the download ID was invalid or if the
  // download is not currently active.  Either way, it's not a failure.
  RecordApiFunctions(DOWNLOADS_FUNCTION_CANCEL);
  return RespondNow(NoArguments());
}

DownloadsEraseFunction::DownloadsEraseFunction() {}

DownloadsEraseFunction::~DownloadsEraseFunction() {}

ExtensionFunction::ResponseAction DownloadsEraseFunction::Run() {
  std::optional<downloads::Erase::Params> params =
      downloads::Erase::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DownloadManager* manager = nullptr;
  DownloadManager* incognito_manager = nullptr;
  GetManagers(browser_context(), include_incognito_information(), &manager,
              &incognito_manager);
  DownloadQuery::DownloadVector results;
  std::string error;
  RunDownloadQuery(params->query, manager, incognito_manager, &error, &results);
  if (!error.empty())
    return RespondNow(Error(std::move(error)));
  base::Value::List json_results;
  for (download::DownloadItem* result : results) {
    json_results.Append(static_cast<int>(result->GetId()));
    result->Remove();
  }
  RecordApiFunctions(DOWNLOADS_FUNCTION_ERASE);
  return RespondNow(WithArguments(std::move(json_results)));
}

DownloadsRemoveFileFunction::DownloadsRemoveFileFunction() {}

DownloadsRemoveFileFunction::~DownloadsRemoveFileFunction() {}

ExtensionFunction::ResponseAction DownloadsRemoveFileFunction::Run() {
  std::optional<downloads::RemoveFile::Params> params =
      downloads::RemoveFile::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DownloadItem* download_item = GetDownload(
      browser_context(), include_incognito_information(), params->download_id);
  std::string error;
  if (InvalidId(download_item, &error) ||
      Fault((download_item->GetState() != DownloadItem::COMPLETE),
            download_extension_errors::kNotComplete, &error) ||
      Fault(download_item->GetFileExternallyRemoved(),
            download_extension_errors::kFileAlreadyDeleted, &error))
    return RespondNow(Error(std::move(error)));
  RecordApiFunctions(DOWNLOADS_FUNCTION_REMOVE_FILE);
  download_item->DeleteFile(
      base::BindOnce(&DownloadsRemoveFileFunction::Done, this));
  return RespondLater();
}

void DownloadsRemoveFileFunction::Done(bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!success) {
    Respond(Error(download_extension_errors::kFileNotRemoved));
  } else {
    Respond(NoArguments());
  }
}

DownloadsAcceptDangerFunction::DownloadsAcceptDangerFunction() {}

DownloadsAcceptDangerFunction::~DownloadsAcceptDangerFunction() {}

DownloadsAcceptDangerFunction::OnPromptCreatedCallback*
    DownloadsAcceptDangerFunction::on_prompt_created_ = nullptr;

ExtensionFunction::ResponseAction DownloadsAcceptDangerFunction::Run() {
  std::optional<downloads::AcceptDanger::Params> params =
      downloads::AcceptDanger::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  PromptOrWait(params->download_id, 10);
  return RespondLater();
}

void DownloadsAcceptDangerFunction::PromptOrWait(int download_id, int retries) {
  DownloadItem* download_item = GetDownload(
      browser_context(), include_incognito_information(), download_id);
  content::WebContents* web_contents = dispatcher()->GetVisibleWebContents();
  std::string error;
  if (InvalidId(download_item, &error) ||
      Fault(download_item->GetState() != DownloadItem::IN_PROGRESS,
            download_extension_errors::kNotInProgress, &error) ||
      Fault(!download_item->IsDangerous(),
            download_extension_errors::kNotDangerous, &error) ||
      Fault(!web_contents, download_extension_errors::kInvisibleContext,
            &error)) {
    Respond(Error(std::move(error)));
    return;
  }
  bool visible = platform_util::IsVisible(web_contents->GetNativeView());
  if (!visible) {
    if (retries > 0) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&DownloadsAcceptDangerFunction::PromptOrWait, this,
                         download_id, retries - 1),
          base::Milliseconds(100));
      return;
    }
    Respond(Error(download_extension_errors::kInvisibleContext));
    return;
  }
  RecordApiFunctions(DOWNLOADS_FUNCTION_ACCEPT_DANGER);
  // DownloadDangerPrompt displays a modal dialog using native widgets that the
  // user must either accept or cancel. It cannot be scripted.
  DownloadDangerPrompt* prompt = DownloadDangerPrompt::Create(
      download_item, web_contents,
      base::BindOnce(&DownloadsAcceptDangerFunction::DangerPromptCallback, this,
                     download_id));
  // DownloadDangerPrompt deletes itself
  if (on_prompt_created_ && !on_prompt_created_->is_null())
    std::move(*on_prompt_created_).Run(prompt);
  // Function finishes in DangerPromptCallback().
}

void DownloadsAcceptDangerFunction::DangerPromptCallback(
    int download_id,
    DownloadDangerPrompt::Action action) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DownloadItem* download_item = GetDownload(
      browser_context(), include_incognito_information(), download_id);
  std::string error;
  if (InvalidId(download_item, &error) ||
      Fault(download_item->GetState() != DownloadItem::IN_PROGRESS,
            download_extension_errors::kNotInProgress, &error)) {
    Respond(Error(std::move(error)));
    return;
  }
  switch (action) {
    case DownloadDangerPrompt::ACCEPT:
      download_item->ValidateDangerousDownload();
      break;
    case DownloadDangerPrompt::CANCEL:
      download_item->Remove();
      break;
    case DownloadDangerPrompt::DISMISS:
      break;
  }
  Respond(NoArguments());
}

DownloadsShowFunction::DownloadsShowFunction() {}

DownloadsShowFunction::~DownloadsShowFunction() {}

ExtensionFunction::ResponseAction DownloadsShowFunction::Run() {
  std::optional<downloads::Show::Params> params =
      downloads::Show::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DownloadItem* download_item = GetDownload(
      browser_context(), include_incognito_information(), params->download_id);
  std::string error;
  if (InvalidId(download_item, &error))
    return RespondNow(Error(std::move(error)));
  download_item->ShowDownloadInShell();
  RecordApiFunctions(DOWNLOADS_FUNCTION_SHOW);
  return RespondNow(NoArguments());
}

DownloadsShowDefaultFolderFunction::DownloadsShowDefaultFolderFunction() {}

DownloadsShowDefaultFolderFunction::~DownloadsShowDefaultFolderFunction() {}

ExtensionFunction::ResponseAction DownloadsShowDefaultFolderFunction::Run() {
  DownloadManager* manager = nullptr;
  DownloadManager* incognito_manager = nullptr;
  GetManagers(browser_context(), include_incognito_information(), &manager,
              &incognito_manager);
  platform_util::OpenItem(
      Profile::FromBrowserContext(browser_context()),
      DownloadPrefs::FromDownloadManager(manager)->DownloadPath(),
      platform_util::OPEN_FOLDER, platform_util::OpenOperationCallback());
  RecordApiFunctions(DOWNLOADS_FUNCTION_SHOW_DEFAULT_FOLDER);
  return RespondNow(NoArguments());
}

DownloadsOpenFunction::OnPromptCreatedCallback*
    DownloadsOpenFunction::on_prompt_created_cb_ = nullptr;

DownloadsOpenFunction::DownloadsOpenFunction() {}

DownloadsOpenFunction::~DownloadsOpenFunction() {}

ExtensionFunction::ResponseAction DownloadsOpenFunction::Run() {
  std::optional<downloads::Open::Params> params =
      downloads::Open::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DownloadItem* download_item = GetDownload(
      browser_context(), include_incognito_information(), params->download_id);
  std::string error;
  if (InvalidId(download_item, &error) ||
      Fault(!user_gesture(), download_extension_errors::kUserGesture, &error) ||
      Fault(download_item->GetState() != DownloadItem::COMPLETE,
            download_extension_errors::kNotComplete, &error) ||
      Fault(download_item->GetFileExternallyRemoved(),
            download_extension_errors::kFileAlreadyDeleted, &error) ||
      Fault(!extension()->permissions_data()->HasAPIPermission(
                APIPermissionID::kDownloadsOpen),
            download_extension_errors::kOpenPermission, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  WindowController* window_controller =
      ChromeExtensionFunctionDetails(this).GetCurrentWindowController();
  if (!window_controller) {
    return RespondNow(Error(download_extension_errors::kInvisibleContext));
  }
  content::WebContents* active_contents = window_controller->GetActiveTab();
  if (!active_contents) {
    return RespondNow(Error(download_extension_errors::kInvisibleContext));
  }

  // Extensions with debugger permission could fake user gestures and should
  // not be trusted.
  if (GetSenderWebContents() &&
      GetSenderWebContents()->HasRecentInteraction() &&
      !extension()->permissions_data()->HasAPIPermission(
          APIPermissionID::kDebugger)) {
    download_item->OpenDownload();
    return RespondNow(NoArguments());
  }
  // Prompt user for ack to open the download.
  // TODO(qinmin): check if user prefers to open all download using the same
  // extension, or check the recent user gesture on the originating webcontents
  // to avoid showing the prompt.
  DownloadOpenPrompt* download_open_prompt =
      DownloadOpenPrompt::CreateDownloadOpenConfirmationDialog(
          active_contents, extension()->name(), download_item->GetFullPath(),
          base::BindOnce(&DownloadsOpenFunction::OpenPromptDone, this,
                         params->download_id));
  if (on_prompt_created_cb_)
    std::move(*on_prompt_created_cb_).Run(download_open_prompt);
  RecordApiFunctions(DOWNLOADS_FUNCTION_OPEN);
  return RespondLater();
}

void DownloadsOpenFunction::OpenPromptDone(int download_id, bool accept) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string error;
  if (Fault(!accept, download_extension_errors::kOpenPermission, &error)) {
    Respond(Error(std::move(error)));
    return;
  }
  DownloadItem* download_item = GetDownload(
      browser_context(), include_incognito_information(), download_id);
  if (Fault(!download_item, download_extension_errors::kFileAlreadyDeleted,
            &error)) {
    Respond(Error(std::move(error)));
    return;
  }
  download_item->OpenDownload();
  Respond(NoArguments());
}

DownloadsSetShelfEnabledFunction::DownloadsSetShelfEnabledFunction() {}

DownloadsSetShelfEnabledFunction::~DownloadsSetShelfEnabledFunction() {}

ExtensionFunction::ResponseAction DownloadsSetShelfEnabledFunction::Run() {
  std::optional<downloads::SetShelfEnabled::Params> params =
      downloads::SetShelfEnabled::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  // TODO(devlin): Solve this with the feature system.
  if (!extension()->permissions_data()->HasAPIPermission(
          APIPermissionID::kDownloadsShelf)) {
    return RespondNow(Error(download_extension_errors::kShelfPermission));
  }

  RecordApiFunctions(DOWNLOADS_FUNCTION_SET_SHELF_ENABLED);
  DownloadCoreService* service = nullptr;
  DownloadCoreService* incognito_service = nullptr;
  GetDownloadCoreServices(browser_context(), include_incognito_information(),
                          &service, &incognito_service);

  MaybeSetUiEnabled(service, incognito_service, extension(), params->enabled);

  for (WindowController* window : *WindowControllerList::GetInstance()) {
    DownloadCoreService* current_service =
        DownloadCoreServiceFactory::GetForBrowserContext(window->profile());
    // The following code is to hide the download UI explicitly if the UI is
    // set to disabled.
    bool match_current_service =
        (current_service == service) || (current_service == incognito_service);
    if (!match_current_service || current_service->IsDownloadUiEnabled()) {
      continue;
    }
    // Calling this API affects the download bubble as well, so extensions
    // using this API is still compatible with the new download bubble. This
    // API will eventually be deprecated (replaced by the SetUiOptions API
    // below).
    Browser* browser = window->GetBrowser();
    if (download::IsDownloadBubbleEnabled() &&
        browser->window()->GetDownloadBubbleUIController()) {
      browser->window()->GetDownloadBubbleUIController()->HideDownloadUi();
    } else if (browser->window()->IsDownloadShelfVisible()) {
      browser->window()->GetDownloadShelf()->Close();
    }
  }

  if (params->enabled &&
      ((service && !service->IsDownloadUiEnabled()) ||
       (incognito_service && !incognito_service->IsDownloadUiEnabled()))) {
    return RespondNow(Error(download_extension_errors::kShelfDisabled));
  }

  return RespondNow(NoArguments());
}

DownloadsSetUiOptionsFunction::DownloadsSetUiOptionsFunction() = default;

DownloadsSetUiOptionsFunction::~DownloadsSetUiOptionsFunction() = default;

ExtensionFunction::ResponseAction DownloadsSetUiOptionsFunction::Run() {
  std::optional<downloads::SetUiOptions::Params> params =
      downloads::SetUiOptions::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  const downloads::UiOptions& options = params->options;
  if (!extension()->permissions_data()->HasAPIPermission(
          APIPermissionID::kDownloadsUi)) {
    return RespondNow(Error(download_extension_errors::kUiPermission));
  }

  RecordApiFunctions(DOWNLOADS_FUNCTION_SET_UI_OPTIONS);
  DownloadCoreService* service = nullptr;
  DownloadCoreService* incognito_service = nullptr;
  GetDownloadCoreServices(browser_context(), include_incognito_information(),
                          &service, &incognito_service);

  MaybeSetUiEnabled(service, incognito_service, extension(), options.enabled);

  for (WindowController* window : *WindowControllerList::GetInstance()) {
    DownloadCoreService* current_service =
        DownloadCoreServiceFactory::GetForBrowserContext(window->profile());
    // The following code is to hide the download UI explicitly if the UI is
    // set to disabled.
    bool match_current_service =
        (current_service == service) || (current_service == incognito_service);
    if (!match_current_service || current_service->IsDownloadUiEnabled()) {
      continue;
    }

    Browser* browser = window->GetBrowser();
    if (download::IsDownloadBubbleEnabled() &&
        browser->window()->GetDownloadBubbleUIController()) {
      browser->window()->GetDownloadBubbleUIController()->HideDownloadUi();
    } else if (browser->window()->IsDownloadShelfVisible()) {
      browser->window()->GetDownloadShelf()->Close();
    }
  }

  if (options.enabled &&
      ((service && !service->IsDownloadUiEnabled()) ||
       (incognito_service && !incognito_service->IsDownloadUiEnabled()))) {
    return RespondNow(Error(download_extension_errors::kUiDisabled));
  }

  return RespondNow(NoArguments());
}

DownloadsGetFileIconFunction::DownloadsGetFileIconFunction()
    : icon_extractor_(new DownloadFileIconExtractorImpl()) {}

DownloadsGetFileIconFunction::~DownloadsGetFileIconFunction() {}

void DownloadsGetFileIconFunction::SetIconExtractorForTesting(
    DownloadFileIconExtractor* extractor) {
  DCHECK(extractor);
  icon_extractor_.reset(extractor);
}

ExtensionFunction::ResponseAction DownloadsGetFileIconFunction::Run() {
  std::optional<downloads::GetFileIcon::Params> params =
      downloads::GetFileIcon::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  const std::optional<downloads::GetFileIconOptions>& options = params->options;
  DownloadItem* download_item = GetDownload(
      browser_context(), include_incognito_information(), params->download_id);
  std::string error;
  if (InvalidId(download_item, &error) ||
      Fault(download_item->GetTargetFilePath().empty(),
            download_extension_errors::kEmptyFile, &error))
    return RespondNow(Error(std::move(error)));

  int icon_size = kDefaultIconSize;
  if (options && options->size) {
    icon_size = *options->size;
    if (icon_size != 16 && icon_size != 32) {
      return RespondNow(Error("Invalid `size`. Must be either `16` or `32`."));
    }
  }

  // In-progress downloads return the intermediate filename for GetFullPath()
  // which doesn't have the final extension. Therefore a good file icon can't be
  // found, so use GetTargetFilePath() instead.
  DCHECK(icon_extractor_.get());
  DCHECK(icon_size == 16 || icon_size == 32);
  float scale = 1.0;
  content::WebContents* web_contents = dispatcher()->GetVisibleWebContents();
  if (web_contents && web_contents->GetRenderWidgetHostView())
    scale = web_contents->GetRenderWidgetHostView()->GetDeviceScaleFactor();
  EXTENSION_FUNCTION_VALIDATE(icon_extractor_->ExtractIconURLForPath(
      download_item->GetTargetFilePath(), scale,
      IconLoaderSizeFromPixelSize(icon_size),
      base::BindOnce(&DownloadsGetFileIconFunction::OnIconURLExtracted, this)));
  return RespondLater();
}

void DownloadsGetFileIconFunction::OnIconURLExtracted(const std::string& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string error;
  if (Fault(url.empty(), download_extension_errors::kIconNotFound, &error)) {
    Respond(Error(std::move(error)));
    return;
  }
  RecordApiFunctions(DOWNLOADS_FUNCTION_GET_FILE_ICON);
  Respond(WithArguments(url));
}

ExtensionDownloadsEventRouter::ExtensionDownloadsEventRouter(
    Profile* profile,
    DownloadManager* manager)
    : profile_(profile), notifier_(manager, this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(profile_);
  extension_registry_observation_.Observe(ExtensionRegistry::Get(profile_));
  EventRouter* router = EventRouter::Get(profile_);
  if (router)
    router->RegisterObserver(this,
                             downloads::OnDeterminingFilename::kEventName);
}

ExtensionDownloadsEventRouter::~ExtensionDownloadsEventRouter() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  EventRouter* router = EventRouter::Get(profile_);
  if (router)
    router->UnregisterObserver(this);
}

void ExtensionDownloadsEventRouter::
    SetDetermineFilenameTimeoutSecondsForTesting(int s) {
  ExtensionDownloadsEventRouterData::
      SetDetermineFilenameTimeoutSecondsForTesting(s);
}

void ExtensionDownloadsEventRouter::SetUiEnabled(const Extension* extension,
                                                 bool enabled) {
  auto iter = ui_disabling_extensions_.find(extension);
  if (iter == ui_disabling_extensions_.end()) {
    if (!enabled)
      ui_disabling_extensions_.insert(extension);
  } else if (enabled) {
    ui_disabling_extensions_.erase(extension);
  }
}

bool ExtensionDownloadsEventRouter::IsUiEnabled() const {
  return ui_disabling_extensions_.empty();
}

// The method by which extensions hook into the filename determination process
// is based on the method by which the omnibox API allows extensions to hook
// into the omnibox autocompletion process. Extensions that wish to play a part
// in the filename determination process call
// chrome.downloads.onDeterminingFilename.addListener, which adds an
// EventListener object to ExtensionEventRouter::listeners().
//
// When a download's filename is being determined, DownloadTargetDeterminer (via
// ChromeDownloadManagerDelegate (CDMD) ::NotifyExtensions()) passes a callback
// to ExtensionDownloadsEventRouter::OnDeterminingFilename (ODF), which stores
// the callback in the item's ExtensionDownloadsEventRouterData (EDERD) along
// with all of the extension IDs that are listening for onDeterminingFilename
// events. ODF dispatches chrome.downloads.onDeterminingFilename.
//
// When the extension's event handler calls |suggestCallback|,
// downloads_custom_bindings.js calls
// DownloadsInternalDetermineFilenameFunction::RunAsync, which calls
// EDER::DetermineFilename, which notifies the item's EDERD.
//
// When the last extension's event handler returns, EDERD invokes the callback
// that CDMD passed to ODF, allowing DownloadTargetDeterminer to continue the
// filename determination process. If multiple extensions wish to override the
// filename, then the extension that was last installed wins.

void ExtensionDownloadsEventRouter::OnDeterminingFilename(
    DownloadItem* item,
    const base::FilePath& suggested_path,
    FilenameChangedCallback filename_changed_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ExtensionDownloadsEventRouterData* data =
      ExtensionDownloadsEventRouterData::Get(item);
  if (!data) {
    std::move(filename_changed_callback)
        .Run({}, DownloadPathReservationTracker::UNIQUIFY);
    return;
  }
  data->BeginFilenameDetermination(std::move(filename_changed_callback));
  bool any_determiners = false;
  base::Value::Dict json = DownloadItemToJSON(item, profile_);
  json.Set(kFilenameKey, suggested_path.LossyDisplayName());
  DispatchEvent(events::DOWNLOADS_ON_DETERMINING_FILENAME,
                downloads::OnDeterminingFilename::kEventName, false,
                base::BindRepeating(&OnDeterminingFilenameWillDispatchCallback,
                                    &any_determiners, data),
                base::Value(std::move(json)));
  if (!any_determiners) {
    data->CallFilenameCallback();
    data->ClearPendingDeterminers();
    data->ResetCreatorSuggestion();
  }
}

void ExtensionDownloadsEventRouter::DetermineFilenameInternal(
    const base::FilePath& filename,
    downloads::FilenameConflictAction conflict_action,
    const ExtensionId& suggesting_extension_id,
    const base::Time& suggesting_install_time,
    const ExtensionId& incumbent_extension_id,
    const base::Time& incumbent_install_time,
    ExtensionId* winner_extension_id,
    base::FilePath* determined_filename,
    downloads::FilenameConflictAction* determined_conflict_action,
    WarningSet* warnings) {
  DCHECK(!filename.empty() ||
         (conflict_action != downloads::FilenameConflictAction::kUniquify));
  DCHECK(!suggesting_extension_id.empty());

  if (incumbent_extension_id.empty()) {
    *winner_extension_id = suggesting_extension_id;
    *determined_filename = filename;
    *determined_conflict_action = conflict_action;
    return;
  }

  if (suggesting_install_time < incumbent_install_time) {
    *winner_extension_id = incumbent_extension_id;
    warnings->insert(Warning::CreateDownloadFilenameConflictWarning(
        suggesting_extension_id, incumbent_extension_id, filename,
        *determined_filename));
    return;
  }

  *winner_extension_id = suggesting_extension_id;
  warnings->insert(Warning::CreateDownloadFilenameConflictWarning(
      incumbent_extension_id, suggesting_extension_id, *determined_filename,
      filename));
  *determined_filename = filename;
  *determined_conflict_action = conflict_action;
}

bool ExtensionDownloadsEventRouter::DetermineFilename(
    content::BrowserContext* browser_context,
    bool include_incognito,
    const ExtensionId& ext_id,
    int download_id,
    const base::FilePath& const_filename,
    downloads::FilenameConflictAction conflict_action,
    std::string* error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordApiFunctions(DOWNLOADS_FUNCTION_DETERMINE_FILENAME);
  DownloadItem* item =
      GetDownload(browser_context, include_incognito, download_id);
  ExtensionDownloadsEventRouterData* data =
      item ? ExtensionDownloadsEventRouterData::Get(item) : nullptr;
  // maxListeners=1 in downloads.idl and suggestCallback in
  // downloads_custom_bindings.js should prevent duplicate DeterminerCallback
  // calls from the same renderer, but an extension may have more than one
  // renderer, so don't DCHECK(!reported).
  if (InvalidId(item, error) ||
      Fault(item->GetState() != DownloadItem::IN_PROGRESS,
            download_extension_errors::kNotInProgress, error) ||
      Fault(!data, download_extension_errors::kUnexpectedDeterminer, error) ||
      Fault(data->DeterminerAlreadyReported(ext_id),
            download_extension_errors::kTooManyListeners, error))
    return false;
  base::FilePath::StringType filename_str(const_filename.value());
  // Allow windows-style directory separators on all platforms.
  std::replace(filename_str.begin(), filename_str.end(),
               FILE_PATH_LITERAL('\\'), FILE_PATH_LITERAL('/'));
  base::FilePath filename(filename_str);
  bool valid_filename = net::IsSafePortableRelativePath(filename);
  filename =
      (valid_filename ? filename.NormalizePathSeparators() : base::FilePath());
  // If the invalid filename check is moved to before DeterminerCallback(), then
  // it will block forever waiting for this ext_id to report.
  if (Fault(!data->DeterminerCallback(browser_context, ext_id, filename,
                                      conflict_action),
            download_extension_errors::kUnexpectedDeterminer, error) ||
      Fault((!const_filename.empty() && !valid_filename),
            download_extension_errors::kInvalidFilename, error))
    return false;
  return true;
}

void ExtensionDownloadsEventRouter::OnListenerRemoved(
    const EventListenerInfo& details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DownloadManager* manager = notifier_.GetManager();
  if (!manager)
    return;
  bool determiner_removed =
      (details.event_name == downloads::OnDeterminingFilename::kEventName);
  EventRouter* router = EventRouter::Get(profile_);
  bool any_listeners =
      router->HasEventListener(downloads::OnChanged::kEventName) ||
      router->HasEventListener(downloads::OnDeterminingFilename::kEventName);
  if (!determiner_removed && any_listeners)
    return;
  DownloadManager::DownloadVector items;
  manager->GetAllDownloads(&items);
  for (DownloadManager::DownloadVector::const_iterator iter = items.begin();
       iter != items.end(); ++iter) {
    ExtensionDownloadsEventRouterData* data =
        ExtensionDownloadsEventRouterData::Get(*iter);
    if (!data)
      continue;
    if (determiner_removed) {
      // Notify any items that may be waiting for callbacks from this
      // extension/determiner.  This will almost always be a no-op, however, it
      // is possible for an extension renderer to be unloaded while a download
      // item is waiting for a determiner. In that case, the download item
      // should proceed.
      data->DeterminerRemoved(details.extension_id);
    }
    if (!any_listeners && data->creator_suggested_filename().empty()) {
      ExtensionDownloadsEventRouterData::Remove(*iter);
    }
  }
}

// That's all the methods that have to do with filename determination. The rest
// have to do with the other, less special events.

void ExtensionDownloadsEventRouter::OnDownloadCreated(
    DownloadManager* manager,
    DownloadItem* download_item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!ShouldExport(*download_item))
    return;

  EventRouter* router = EventRouter::Get(profile_);
  // Avoid allocating a bunch of memory in DownloadItemToJSON if it isn't going
  // to be used.
  if (!router || (!router->HasEventListener(downloads::OnCreated::kEventName) &&
                  !router->HasEventListener(downloads::OnChanged::kEventName) &&
                  !router->HasEventListener(
                      downloads::OnDeterminingFilename::kEventName))) {
    return;
  }

  // download_item->GetFileExternallyRemoved() should always return false for
  // unfinished download.
  base::Value::Dict json_item = DownloadItemToJSON(download_item, profile_);
  DispatchEvent(events::DOWNLOADS_ON_CREATED, downloads::OnCreated::kEventName,
                true, Event::WillDispatchCallback(),
                base::Value(json_item.Clone()));
  if (!ExtensionDownloadsEventRouterData::Get(download_item) &&
      (router->HasEventListener(downloads::OnChanged::kEventName) ||
       router->HasEventListener(
           downloads::OnDeterminingFilename::kEventName))) {
    new ExtensionDownloadsEventRouterData(
        download_item, download_item->GetState() == DownloadItem::COMPLETE
                           ? base::Value::Dict()
                           : std::move(json_item));
  }
}

void ExtensionDownloadsEventRouter::OnDownloadUpdated(
    DownloadManager* manager,
    DownloadItem* download_item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  EventRouter* router = EventRouter::Get(profile_);
  ExtensionDownloadsEventRouterData* data =
      ExtensionDownloadsEventRouterData::Get(download_item);
  if (!ShouldExport(*download_item) ||
      !router->HasEventListener(downloads::OnChanged::kEventName)) {
    return;
  }
  if (!data) {
    // The download_item probably transitioned from temporary to not temporary,
    // or else an event listener was added.
    data = new ExtensionDownloadsEventRouterData(download_item,
                                                 base::Value::Dict());
  }
  base::Value::Dict new_json;
  base::Value::Dict delta;
  delta.Set(kIdKey, static_cast<int>(download_item->GetId()));
  bool changed = false;
  // For completed downloads, update can only happen when file is removed.
  if (data->is_download_completed()) {
    if (data->is_completed_download_deleted() !=
        download_item->GetFileExternallyRemoved()) {
      DCHECK(!data->is_completed_download_deleted());
      DCHECK(download_item->GetFileExternallyRemoved());
      std::string exists = kExistsKey;
      delta.SetByDottedPath(exists + ".current", false);
      delta.SetByDottedPath(exists + ".previous", true);
      changed = true;
    }
  } else {
    new_json = DownloadItemToJSON(download_item, profile_);
    std::set<std::string> new_fields;
    // For each field in the new json representation of the download_item except
    // the bytesReceived field, if the field has changed from the previous old
    // json, set the differences in the |delta| object and remember that
    // something significant changed.
    for (auto kv : new_json) {
      new_fields.insert(kv.first);
      if (IsDownloadDeltaField(kv.first)) {
        const base::Value* old_value = data->json().Find(kv.first);
        if (!old_value || kv.second != *old_value) {
          delta.SetByDottedPath(kv.first + ".current", kv.second.Clone());
          if (old_value) {
            delta.SetByDottedPath(kv.first + ".previous", old_value->Clone());
          }
          changed = true;
        }
      }
    }

    // If a field was in the previous json but is not in the new json, set the
    // difference in |delta|.
    for (auto kv : data->json()) {
      if ((new_fields.find(kv.first) == new_fields.end()) &&
          IsDownloadDeltaField(kv.first)) {
        // estimatedEndTime disappears after completion, but bytesReceived
        // stays.
        delta.SetByDottedPath(kv.first + ".previous", kv.second.Clone());
        changed = true;
      }
    }
  }

  data->set_is_download_completed(download_item->GetState() ==
                                  DownloadItem::COMPLETE);
  // download_item->GetFileExternallyRemoved() should always return false for
  // unfinished download.
  data->set_is_completed_download_deleted(
      download_item->GetFileExternallyRemoved());
  data->set_json(std::move(new_json));

  // Update the OnChangedStat and dispatch the event if something significant
  // changed. Replace the stored json with the new json.
  data->OnItemUpdated();
  if (changed) {
    DispatchEvent(events::DOWNLOADS_ON_CHANGED,
                  downloads::OnChanged::kEventName, true,
                  Event::WillDispatchCallback(), base::Value(std::move(delta)));
    data->OnChangedFired();
  }
}

void ExtensionDownloadsEventRouter::OnDownloadRemoved(
    DownloadManager* manager,
    DownloadItem* download_item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!ShouldExport(*download_item))
    return;
  DispatchEvent(events::DOWNLOADS_ON_ERASED, downloads::OnErased::kEventName,
                true, Event::WillDispatchCallback(),
                base::Value(static_cast<int>(download_item->GetId())));
}

void ExtensionDownloadsEventRouter::DispatchEvent(
    events::HistogramValue histogram_value,
    const std::string& event_name,
    bool include_incognito,
    Event::WillDispatchCallback will_dispatch_callback,
    base::Value arg) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!EventRouter::Get(profile_))
    return;
  base::Value::List args;
  args.Append(std::move(arg));
  // The downloads system wants to share on-record events with off-record
  // extension renderers even in incognito_split_mode because that's how
  // chrome://downloads works. The "restrict_to_profile" mechanism does not
  // anticipate this, so it does not automatically prevent sharing off-record
  // events with on-record extension renderers.
  // TODO(lazyboy): When |restrict_to_browser_context| is nullptr, this will
  // broadcast events to unrelated profiles, not just incognito. Fix this
  // by introducing "include incognito" option to Event constructor.
  // https://crbug.com/726022.
  Profile* restrict_to_browser_context =
      (include_incognito && !profile_->IsOffTheRecord()) ? nullptr
                                                         : profile_.get();
  auto event =
      std::make_unique<Event>(histogram_value, event_name, std::move(args),
                              restrict_to_browser_context);
  event->will_dispatch_callback = std::move(will_dispatch_callback);
  EventRouter::Get(profile_)->BroadcastEvent(std::move(event));
}

void ExtensionDownloadsEventRouter::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto iter = ui_disabling_extensions_.find(extension);
  if (iter != ui_disabling_extensions_.end())
    ui_disabling_extensions_.erase(iter);
}

void ExtensionDownloadsEventRouter::CheckForHistoryFilesRemoval() {
  static const int kFileExistenceRateLimitSeconds = 10;
  DownloadManager* manager = notifier_.GetManager();
  if (!manager)
    return;
  base::Time now(base::Time::Now());
  int delta = now.ToTimeT() - last_checked_removal_.ToTimeT();
  if (delta <= kFileExistenceRateLimitSeconds)
    return;
  last_checked_removal_ = now;
  manager->CheckForHistoryFilesRemoval();
}

}  // namespace extensions
