// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/downloads/downloads_api.h"

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/current_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time_to_iso8601.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
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
#include "chrome/browser/icon_loader.h"
#include "chrome/browser/icon_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/extensions/api/downloads.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/warning_service.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/base/filename_util.h"
#include "net/base/load_flags.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image_skia.h"

using content::BrowserContext;
using content::BrowserThread;
using content::DownloadManager;
using download::DownloadItem;
using download::DownloadPathReservationTracker;

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
const char kInvisibleContext[] = "Javascript execution context is not visible "
  "(tab, window, popup bubble)";
const char kNotComplete[] = "Download must be complete";
const char kNotDangerous[] = "Download must be dangerous";
const char kNotInProgress[] = "Download must be in progress";
const char kNotResumable[] = "DownloadItem.canResume must be true";
const char kOpenPermission[] = "The \"downloads.open\" permission is required";
const char kShelfDisabled[] = "Another extension has disabled the shelf";
const char kShelfPermission[] = "downloads.setShelfEnabled requires the "
  "\"downloads.shelf\" permission";
const char kTooManyListeners[] = "Each extension may have at most one "
  "onDeterminingFilename listener between all of its renderer execution "
  "contexts.";
const char kUnexpectedDeterminer[] = "Unexpected determineFilename call";
const char kUserGesture[] = "User gesture required";

}  // namespace download_extension_errors


namespace extensions {

namespace {

namespace downloads = api::downloads;

// Default icon size for getFileIcon() in pixels.
const int  kDefaultIconSize = 32;

// Parameter keys
const char kByExtensionIdKey[] = "byExtensionId";
const char kByExtensionNameKey[] = "byExtensionName";
const char kBytesReceivedKey[] = "bytesReceived";
const char kCanResumeKey[] = "canResume";
const char kDangerAccepted[] = "accepted";
const char kDangerContent[] = "content";
const char kDangerFile[] = "file";
const char kDangerHost[] = "host";
const char kDangerKey[] = "danger";
const char kDangerSafe[] = "safe";
const char kDangerUncommon[] = "uncommon";
const char kDangerUnwanted[] = "unwanted";
const char kDangerAllowlistedByPolicy[] = "allowlistedByPolicy";
const char kDangerAsyncScanning[] = "asyncScanning";
const char kDangerPasswordProtected[] = "passwordProtected";
const char kDangerTooLarge[] = "blockedTooLarge";
const char kDangerSensitiveContentWarning[] = "sensitiveContentWarning";
const char kDangerSensitiveContentBlock[] = "sensitiveContentBlock";
const char kDangerUnsupportedFileType[] = "unsupportedFileType";
const char kDangerDeepScannedSafe[] = "deepScannedSafe";
const char kDangerDeepScannedOpenedDangerous[] = "deepScannedOpenedDangerous";
const char kDangerPromptForScanning[] = "promptForScanning";
const char kDangerUrl[] = "url";
const char kEndTimeKey[] = "endTime";
const char kEndedAfterKey[] = "endedAfter";
const char kEndedBeforeKey[] = "endedBefore";
const char kErrorKey[] = "error";
const char kEstimatedEndTimeKey[] = "estimatedEndTime";
const char kExistsKey[] = "exists";
const char kFileSizeKey[] = "fileSize";
const char kFilenameKey[] = "filename";
const char kFilenameRegexKey[] = "filenameRegex";
const char kIdKey[] = "id";
const char kDownloadsApiIncognitoKey[] = "incognito";
const char kMimeKey[] = "mime";
const char kPausedKey[] = "paused";
const char kQueryKey[] = "query";
const char kReferrerUrlKey[] = "referrer";
const char kStartTimeKey[] = "startTime";
const char kStartedAfterKey[] = "startedAfter";
const char kStartedBeforeKey[] = "startedBefore";
const char kStateComplete[] = "complete";
const char kStateInProgress[] = "in_progress";
const char kStateInterrupted[] = "interrupted";
const char kStateKey[] = "state";
const char kTotalBytesGreaterKey[] = "totalBytesGreater";
const char kTotalBytesKey[] = "totalBytes";
const char kTotalBytesLessKey[] = "totalBytesLess";
const char kUrlKey[] = "url";
const char kUrlRegexKey[] = "urlRegex";
const char kFinalUrlKey[] = "finalUrl";
const char kFinalUrlRegexKey[] = "finalUrlRegex";

const char* const kDangerStrings[] = {kDangerSafe,
                                      kDangerFile,
                                      kDangerUrl,
                                      kDangerContent,
                                      kDangerSafe,
                                      kDangerUncommon,
                                      kDangerAccepted,
                                      kDangerHost,
                                      kDangerUnwanted,
                                      kDangerAllowlistedByPolicy,
                                      kDangerAsyncScanning,
                                      kDangerPasswordProtected,
                                      kDangerTooLarge,
                                      kDangerSensitiveContentWarning,
                                      kDangerSensitiveContentBlock,
                                      kDangerDeepScannedSafe,
                                      kDangerDeepScannedOpenedDangerous,
                                      kDangerPromptForScanning,
                                      kDangerUnsupportedFileType};
static_assert(base::size(kDangerStrings) == download::DOWNLOAD_DANGER_TYPE_MAX,
              "kDangerStrings should have DOWNLOAD_DANGER_TYPE_MAX elements");

const char* const kStateStrings[] = {
  kStateInProgress,
  kStateComplete,
  kStateInterrupted,
  kStateInterrupted,
};
static_assert(base::size(kStateStrings) ==
                  download::DownloadItem::MAX_DOWNLOAD_STATE,
              "kStateStrings should have MAX_DOWNLOAD_STATE elements");

const char* DangerString(download::DownloadDangerType danger) {
  DCHECK(danger >= 0);
  DCHECK(danger <
         static_cast<download::DownloadDangerType>(base::size(kDangerStrings)));
  if (danger < 0 || danger >= static_cast<download::DownloadDangerType>(
                                  base::size(kDangerStrings)))
    return "";
  return kDangerStrings[danger];
}

download::DownloadDangerType DangerEnumFromString(const std::string& danger) {
  for (size_t i = 0; i < base::size(kDangerStrings); ++i) {
    if (danger == kDangerStrings[i])
      return static_cast<download::DownloadDangerType>(i);
  }
  return download::DOWNLOAD_DANGER_TYPE_MAX;
}

const char* StateString(download::DownloadItem::DownloadState state) {
  DCHECK(state >= 0);
  DCHECK(state < static_cast<download::DownloadItem::DownloadState>(
                     base::size(kStateStrings)));
  if (state < 0 || state >= static_cast<download::DownloadItem::DownloadState>(
                                base::size(kStateStrings)))
    return "";
  return kStateStrings[state];
}

download::DownloadItem::DownloadState StateEnumFromString(
    const std::string& state) {
  for (size_t i = 0; i < base::size(kStateStrings); ++i) {
    if ((kStateStrings[i] != NULL) && (state == kStateStrings[i]))
      return static_cast<DownloadItem::DownloadState>(i);
  }
  return DownloadItem::MAX_DOWNLOAD_STATE;
}

std::unique_ptr<base::DictionaryValue> DownloadItemToJSON(
    DownloadItem* download_item,
    content::BrowserContext* browser_context) {
  base::DictionaryValue* json = new base::DictionaryValue();
  json->SetBoolean(kExistsKey, !download_item->GetFileExternallyRemoved());
  json->SetInteger(kIdKey, download_item->GetId());
  const GURL& url = download_item->GetOriginalUrl();
  json->SetString(kUrlKey, (url.is_valid() ? url.spec() : std::string()));
  const GURL& finalUrl = download_item->GetURL();
  json->SetString(kFinalUrlKey,
                  (finalUrl.is_valid() ? finalUrl.spec() : std::string()));
  const GURL& referrer = download_item->GetReferrerUrl();
  json->SetString(kReferrerUrlKey, (referrer.is_valid() ? referrer.spec()
                                                        : std::string()));
  json->SetString(kFilenameKey,
                  download_item->GetTargetFilePath().LossyDisplayName());
  json->SetString(kDangerKey, DangerString(download_item->GetDangerType()));
  json->SetString(kStateKey, StateString(download_item->GetState()));
  json->SetBoolean(kCanResumeKey, download_item->CanResume());
  json->SetBoolean(kPausedKey, download_item->IsPaused());
  json->SetString(kMimeKey, download_item->GetMimeType());
  json->SetString(kStartTimeKey,
                  base::TimeToISO8601(download_item->GetStartTime()));
  json->SetDouble(kBytesReceivedKey, download_item->GetReceivedBytes());
  json->SetDouble(kTotalBytesKey, download_item->GetTotalBytes());
  json->SetBoolean(kDownloadsApiIncognitoKey,
                   browser_context->IsOffTheRecord());
  if (download_item->GetState() == DownloadItem::INTERRUPTED) {
    json->SetString(kErrorKey, download::DownloadInterruptReasonToString(
                                   download_item->GetLastReason()));
  } else if (download_item->GetState() == DownloadItem::CANCELLED) {
    json->SetString(kErrorKey,
                    download::DownloadInterruptReasonToString(
                        download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED));
  }
  if (!download_item->GetEndTime().is_null())
    json->SetString(kEndTimeKey,
                    base::TimeToISO8601(download_item->GetEndTime()));
  base::TimeDelta time_remaining;
  if (download_item->TimeRemaining(&time_remaining)) {
    base::Time now = base::Time::Now();
    json->SetString(kEstimatedEndTimeKey,
                    base::TimeToISO8601(now + time_remaining));
  }
  DownloadedByExtension* by_ext = DownloadedByExtension::Get(download_item);
  if (by_ext) {
    json->SetString(kByExtensionIdKey, by_ext->id());
    json->SetString(kByExtensionNameKey, by_ext->name());
    // Lookup the extension's current name() in case the user changed their
    // language. This won't work if the extension was uninstalled, so the name
    // might be the wrong language.
    const Extension* extension =
        ExtensionRegistry::Get(browser_context)
            ->GetExtensionById(by_ext->id(), ExtensionRegistry::EVERYTHING);
    if (extension)
      json->SetString(kByExtensionNameKey, extension->name());
  }
  // TODO(benjhayden): Implement fileSize.
  json->SetDouble(kFileSizeKey, download_item->GetTotalBytes());
  return std::unique_ptr<base::DictionaryValue>(json);
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
                          const IconURLCallback& callback,
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
      path, icon_size,
      base::BindOnce(&DownloadFileIconExtractorImpl::OnIconLoadComplete,
                     base::Unretained(this), scale, callback),
      &cancelable_task_tracker_);
  return true;
}

void DownloadFileIconExtractorImpl::OnIconLoadComplete(
    float scale,
    const IconURLCallback& callback,
    gfx::Image icon) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  callback.Run(
      icon.IsEmpty()
          ? std::string()
          : webui::GetBitmapDataUrl(
                icon.ToImageSkia()->GetRepresentation(scale).GetBitmap()));
}

IconLoader::IconSize IconLoaderSizeFromPixelSize(int pixel_size) {
  switch (pixel_size) {
    case 16: return IconLoader::SMALL;
    case 32: return IconLoader::NORMAL;
    default:
      NOTREACHED();
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
// |include_incognito|. This should work regardless of whether |profile| is
// original or incognito.
void GetManagers(content::BrowserContext* context,
                 bool include_incognito,
                 DownloadManager** manager,
                 DownloadManager** incognito_manager) {
  Profile* profile = Profile::FromBrowserContext(context);
  *manager = BrowserContext::GetDownloadManager(profile->GetOriginalProfile());
  if (profile->HasPrimaryOTRProfile() &&
      (include_incognito || profile->IsOffTheRecord())) {
    *incognito_manager =
        BrowserContext::GetDownloadManager(profile->GetPrimaryOTRProfile());
  } else {
    *incognito_manager = NULL;
  }
}

DownloadItem* GetDownload(content::BrowserContext* context,
                          bool include_incognito,
                          int id) {
  DownloadManager* manager = NULL;
  DownloadManager* incognito_manager = NULL;
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
  // Insert new values here, not at the beginning.
  DOWNLOADS_FUNCTION_LAST
};

void RecordApiFunctions(DownloadsFunctionName function) {
  UMA_HISTOGRAM_ENUMERATION("Download.ApiFunctions",
                            function,
                            DOWNLOADS_FUNCTION_LAST);
}

void CompileDownloadQueryOrderBy(
    const std::vector<std::string>& order_by_strs,
    std::string* error,
    DownloadQuery* query) {
  // TODO(benjhayden): Consider switching from LazyInstance to explicit string
  // comparisons.
  static base::LazyInstance<SortTypeMap>::DestructorAtExit sorter_types =
      LAZY_INSTANCE_INITIALIZER;
  if (sorter_types.Get().empty())
    InitSortTypeMap(sorter_types.Pointer());

  for (auto iter = order_by_strs.cbegin(); iter != order_by_strs.cend();
       ++iter) {
    std::string term_str = *iter;
    if (term_str.empty())
      continue;
    DownloadQuery::SortDirection direction = DownloadQuery::ASCENDING;
    if (term_str[0] == '-') {
      direction = DownloadQuery::DESCENDING;
      term_str = term_str.substr(1);
    }
    SortTypeMap::const_iterator sorter_type =
        sorter_types.Get().find(term_str);
    if (sorter_type == sorter_types.Get().end()) {
      *error = download_extension_errors::kInvalidOrderBy;
      return;
    }
    query->AddSorter(sorter_type->second, direction);
  }
}

void RunDownloadQuery(
    const downloads::DownloadQuery& query_in,
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
  if (query_in.limit.get()) {
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
  std::string danger_string =
      downloads::ToString(query_in.danger);
  if (!danger_string.empty()) {
    download::DownloadDangerType danger_type =
        DangerEnumFromString(danger_string);
    if (danger_type == download::DOWNLOAD_DANGER_TYPE_MAX) {
      *error = download_extension_errors::kInvalidDangerType;
      return;
    }
    query_out.AddFilter(danger_type);
  }
  if (query_in.order_by.get()) {
    CompileDownloadQueryOrderBy(*query_in.order_by, error, &query_out);
    if (!error->empty())
      return;
  }

  std::unique_ptr<base::DictionaryValue> query_in_value(query_in.ToValue());
  for (base::DictionaryValue::Iterator query_json_field(*query_in_value);
       !query_json_field.IsAtEnd(); query_json_field.Advance()) {
    FilterTypeMap::const_iterator filter_type =
        filter_types.Get().find(query_json_field.key());
    if (filter_type != filter_types.Get().end()) {
      if (!query_out.AddFilter(filter_type->second, query_json_field.value())) {
        *error = download_extension_errors::kInvalidFilter;
        return;
      }
    }
  }

  DownloadQuery::DownloadVector all_items;
  if (query_in.id.get()) {
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
  query_out.AddFilter(base::Bind(&ShouldExport));
  query_out.Search(all_items.begin(), all_items.end(), results);
}

download::DownloadPathReservationTracker::FilenameConflictAction
ConvertConflictAction(downloads::FilenameConflictAction action) {
  switch (action) {
    case downloads::FILENAME_CONFLICT_ACTION_NONE:
    case downloads::FILENAME_CONFLICT_ACTION_UNIQUIFY:
      return DownloadPathReservationTracker::UNIQUIFY;
    case downloads::FILENAME_CONFLICT_ACTION_OVERWRITE:
      return DownloadPathReservationTracker::OVERWRITE;
    case downloads::FILENAME_CONFLICT_ACTION_PROMPT:
      return DownloadPathReservationTracker::PROMPT;
  }
  NOTREACHED();
  return download::DownloadPathReservationTracker::UNIQUIFY;
}

class ExtensionDownloadsEventRouterData : public base::SupportsUserData::Data {
 public:
  static ExtensionDownloadsEventRouterData* Get(DownloadItem* download_item) {
    base::SupportsUserData::Data* data = download_item->GetUserData(kKey);
    return (data == NULL) ? NULL :
        static_cast<ExtensionDownloadsEventRouterData*>(data);
  }

  static void Remove(DownloadItem* download_item) {
    download_item->RemoveUserData(kKey);
  }

  explicit ExtensionDownloadsEventRouterData(
      DownloadItem* download_item,
      std::unique_ptr<base::DictionaryValue> json_item)
      : updated_(0),
        changed_fired_(0),
        json_(std::move(json_item)),
        creator_conflict_action_(downloads::FILENAME_CONFLICT_ACTION_UNIQUIFY),
        determined_conflict_action_(
            downloads::FILENAME_CONFLICT_ACTION_UNIQUIFY),
        is_download_completed_(download_item->GetState() ==
                               DownloadItem::COMPLETE),
        is_completed_download_deleted_(
            download_item->GetFileExternallyRemoved()) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    download_item->SetUserData(kKey, base::WrapUnique(this));
  }

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
  const base::DictionaryValue& json() const { return *json_; }
  void set_json(std::unique_ptr<base::DictionaryValue> json_item) {
    json_ = std::move(json_item);
  }

  void OnItemUpdated() { ++updated_; }
  void OnChangedFired() { ++changed_fired_; }

  static void SetDetermineFilenameTimeoutSecondsForTesting(int s) {
    determine_filename_timeout_s_ = s;
  }

  void BeginFilenameDetermination(
      const base::Closure& no_change,
      const ExtensionDownloadsEventRouter::FilenameChangedCallback& change) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    ClearPendingDeterminers();
    filename_no_change_ = no_change;
    filename_change_ = change;
    determined_filename_ = creator_suggested_filename_;
    determined_conflict_action_ = creator_conflict_action_;
    // determiner_.install_time should default to 0 so that creator suggestions
    // should be lower priority than any actual onDeterminingFilename listeners.

    // Ensure that the callback is called within a time limit.
    weak_ptr_factory_.reset(
        new base::WeakPtrFactory<ExtensionDownloadsEventRouterData>(this));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &ExtensionDownloadsEventRouterData::DetermineFilenameTimeout,
            weak_ptr_factory_->GetWeakPtr()),
        base::TimeDelta::FromSeconds(determine_filename_timeout_s_));
  }

  void DetermineFilenameTimeout() {
    CallFilenameCallback();
  }

  void ClearPendingDeterminers() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    determined_filename_.clear();
    determined_conflict_action_ =
      downloads::FILENAME_CONFLICT_ACTION_UNIQUIFY;
    determiner_ = DeterminerInfo();
    filename_no_change_ = base::Closure();
    filename_change_ = ExtensionDownloadsEventRouter::FilenameChangedCallback();
    weak_ptr_factory_.reset();
    determiners_.clear();
  }

  void DeterminerRemoved(const std::string& extension_id) {
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

  void AddPendingDeterminer(const std::string& extension_id,
                            const base::Time& installed) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    for (size_t index = 0; index < determiners_.size(); ++index) {
      if (determiners_[index].extension_id == extension_id) {
        DCHECK(false) << extension_id;
        return;
      }
    }
    determiners_.push_back(DeterminerInfo(extension_id, installed));
  }

  bool DeterminerAlreadyReported(const std::string& extension_id) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    for (size_t index = 0; index < determiners_.size(); ++index) {
      if (determiners_[index].extension_id == extension_id) {
        return determiners_[index].reported;
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

  downloads::FilenameConflictAction
  creator_conflict_action() const {
    return creator_conflict_action_;
  }

  void ResetCreatorSuggestion() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    creator_suggested_filename_.clear();
    creator_conflict_action_ =
      downloads::FILENAME_CONFLICT_ACTION_UNIQUIFY;
  }

  // Returns false if this |extension_id| was not expected or if this
  // |extension_id| has already reported. The caller is responsible for
  // validating |filename|.
  bool DeterminerCallback(content::BrowserContext* browser_context,
                          const std::string& extension_id,
                          const base::FilePath& filename,
                          downloads::FilenameConflictAction conflict_action) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    bool found_info = false;
    for (size_t index = 0; index < determiners_.size(); ++index) {
      if (determiners_[index].extension_id == extension_id) {
        found_info = true;
        if (determiners_[index].reported)
          return false;
        determiners_[index].reported = true;
        // Do not use filename if another determiner has already overridden the
        // filename and they take precedence. Extensions that were installed
        // later take precedence over previous extensions.
        if (!filename.empty() ||
            (conflict_action != downloads::FILENAME_CONFLICT_ACTION_UNIQUIFY)) {
          WarningSet warnings;
          std::string winner_extension_id;
          ExtensionDownloadsEventRouter::DetermineFilenameInternal(
              filename,
              conflict_action,
              determiners_[index].extension_id,
              determiners_[index].install_time,
              determiner_.extension_id,
              determiner_.install_time,
              &winner_extension_id,
              &determined_filename_,
              &determined_conflict_action_,
              &warnings);
          if (!warnings.empty())
            WarningService::NotifyWarningsOnUI(browser_context, warnings);
          if (winner_extension_id == determiners_[index].extension_id)
            determiner_ = determiners_[index];
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
    DeterminerInfo(const std::string& e_id,
                   const base::Time& installed);
    ~DeterminerInfo();

    std::string extension_id;
    base::Time install_time;
    bool reported;
  };
  typedef std::vector<DeterminerInfo> DeterminerInfoVector;

  static const char kKey[];

  // This is safe to call even while not waiting for determiners to call back;
  // in that case, the callbacks will be null so they won't be Run.
  void CheckAllDeterminersCalled() {
    for (auto iter = determiners_.begin(); iter != determiners_.end(); ++iter) {
      if (!iter->reported)
        return;
    }
    CallFilenameCallback();

    // Don't clear determiners_ immediately in case there's a second listener
    // for one of the extensions, so that DetermineFilename can return
    // kTooManyListeners. After a few seconds, DetermineFilename will return
    // kUnexpectedDeterminer instead of kTooManyListeners so that determiners_
    // doesn't keep hogging memory.
    weak_ptr_factory_.reset(
        new base::WeakPtrFactory<ExtensionDownloadsEventRouterData>(this));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &ExtensionDownloadsEventRouterData::ClearPendingDeterminers,
            weak_ptr_factory_->GetWeakPtr()),
        base::TimeDelta::FromSeconds(15));
  }

  void CallFilenameCallback() {
    if (determined_filename_.empty() &&
        (determined_conflict_action_ ==
         downloads::FILENAME_CONFLICT_ACTION_UNIQUIFY)) {
      if (!filename_no_change_.is_null())
        filename_no_change_.Run();
    } else {
      if (!filename_change_.is_null()) {
        filename_change_.Run(determined_filename_, ConvertConflictAction(
            determined_conflict_action_));
      }
    }
    // Clear the callbacks immediately in case they aren't idempotent.
    filename_no_change_ = base::Closure();
    filename_change_ = ExtensionDownloadsEventRouter::FilenameChangedCallback();
  }


  int updated_;
  int changed_fired_;
  // Dictionary representing the current state of the download. It is cleared
  // when download completes.
  std::unique_ptr<base::DictionaryValue> json_;

  base::Closure filename_no_change_;
  ExtensionDownloadsEventRouter::FilenameChangedCallback filename_change_;

  DeterminerInfoVector determiners_;

  base::FilePath creator_suggested_filename_;
  downloads::FilenameConflictAction
    creator_conflict_action_;
  base::FilePath determined_filename_;
  downloads::FilenameConflictAction
    determined_conflict_action_;
  DeterminerInfo determiner_;

  // Whether a download is complete and whether the completed download is
  // deleted.
  bool is_download_completed_;
  bool is_completed_download_deleted_;

  std::unique_ptr<base::WeakPtrFactory<ExtensionDownloadsEventRouterData>>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDownloadsEventRouterData);
};

int ExtensionDownloadsEventRouterData::determine_filename_timeout_s_ = 15;

ExtensionDownloadsEventRouterData::DeterminerInfo::DeterminerInfo(
    const std::string& e_id,
    const base::Time& installed)
    : extension_id(e_id),
      install_time(installed),
      reported(false) {
}

ExtensionDownloadsEventRouterData::DeterminerInfo::DeterminerInfo()
    : reported(false) {
}

ExtensionDownloadsEventRouterData::DeterminerInfo::~DeterminerInfo() {}

const char ExtensionDownloadsEventRouterData::kKey[] =
  "DownloadItem ExtensionDownloadsEventRouterData";

bool OnDeterminingFilenameWillDispatchCallback(
    bool* any_determiners,
    ExtensionDownloadsEventRouterData* data,
    content::BrowserContext* browser_context,
    Feature::Context target_context,
    const Extension* extension,
    Event* event,
    const base::DictionaryValue* listener_filter) {
  *any_determiners = true;
  base::Time installed =
      ExtensionPrefs::Get(browser_context)->GetInstallTime(extension->id());
  data->AddPendingDeterminer(extension->id(), installed);
  return true;
}

bool Fault(bool error,
           const char* message_in,
           std::string* message_out) {
  if (!error)
    return false;
  *message_out = message_in;
  return true;
}

bool InvalidId(DownloadItem* valid_item, std::string* message_out) {
  return Fault(!valid_item, download_extension_errors::kInvalidId, message_out);
}

bool IsDownloadDeltaField(const std::string& field) {
  return ((field == kUrlKey) ||
          (field == kFinalUrlKey) ||
          (field == kFilenameKey) ||
          (field == kDangerKey) ||
          (field == kMimeKey) ||
          (field == kStartTimeKey) ||
          (field == kEndTimeKey) ||
          (field == kStateKey) ||
          (field == kCanResumeKey) ||
          (field == kPausedKey) ||
          (field == kErrorKey) ||
          (field == kTotalBytesKey) ||
          (field == kFileSizeKey) ||
          (field == kExistsKey));
}

}  // namespace

const char DownloadedByExtension::kKey[] =
  "DownloadItem DownloadedByExtension";

DownloadedByExtension* DownloadedByExtension::Get(
    download::DownloadItem* item) {
  base::SupportsUserData::Data* data = item->GetUserData(kKey);
  return (data == NULL) ? NULL :
      static_cast<DownloadedByExtension*>(data);
}

DownloadedByExtension::DownloadedByExtension(download::DownloadItem* item,
                                             const std::string& id,
                                             const std::string& name)
    : id_(id), name_(name) {
  item->SetUserData(kKey, base::WrapUnique(this));
}

DownloadsDownloadFunction::DownloadsDownloadFunction() {}

DownloadsDownloadFunction::~DownloadsDownloadFunction() {}

ExtensionFunction::ResponseAction DownloadsDownloadFunction::Run() {
  std::unique_ptr<downloads::Download::Params> params(
      downloads::Download::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
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
          render_frame_host()->GetRenderViewHost()->GetRoutingID(),
          render_frame_host()->GetRoutingID(), traffic_annotation));

  base::FilePath creator_suggested_filename;
  if (options.filename.get()) {
#if defined(OS_WIN)
    // Can't get filename16 from options.ToValue() because that converts it from
    // std::string.
    base::DictionaryValue* options_value = NULL;
    EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &options_value));
    base::string16 filename16;
    EXTENSION_FUNCTION_VALIDATE(options_value->GetString(
        kFilenameKey, &filename16));
    creator_suggested_filename = base::FilePath(filename16);
#elif defined(OS_POSIX)
    creator_suggested_filename = base::FilePath(*options.filename);
#endif
    if (!net::IsSafePortableRelativePath(creator_suggested_filename)) {
      return RespondNow(Error(download_extension_errors::kInvalidFilename));
    }
  }

  if (options.save_as.get())
    download_params->set_prompt(*options.save_as);

  if (options.headers.get()) {
    for (const downloads::HeaderNameValuePair& name_value : *options.headers) {
      if (!net::HttpUtil::IsValidHeaderName(name_value.name)) {
        return RespondNow(Error(download_extension_errors::kInvalidHeaderName));
      }
      if (!net::HttpUtil::IsSafeHeader(name_value.name)) {
        return RespondNow(
            Error(download_extension_errors::kInvalidHeaderUnsafe));
      }
      if (!net::HttpUtil::IsValidHeaderValue(name_value.value)) {
        return RespondNow(
            Error(download_extension_errors::kInvalidHeaderValue));
      }
      download_params->add_request_header(name_value.name, name_value.value);
    }
  }

  std::string method_string =
      downloads::ToString(options.method);
  if (!method_string.empty())
    download_params->set_method(method_string);
  if (options.body.get()) {
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

  DownloadManager* manager =
      BrowserContext::GetDownloadManager(browser_context());

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
    Respond(OneArgument(
        std::make_unique<base::Value>(static_cast<int>(item->GetId()))));
    if (!creator_suggested_filename.empty() ||
        (creator_conflict_action !=
         downloads::FILENAME_CONFLICT_ACTION_UNIQUIFY)) {
      ExtensionDownloadsEventRouterData* data =
          ExtensionDownloadsEventRouterData::Get(item);
      if (!data) {
        data = new ExtensionDownloadsEventRouterData(
            item, std::unique_ptr<base::DictionaryValue>(
                      new base::DictionaryValue()));
      }
      data->CreatorSuggestedFilename(
          creator_suggested_filename, creator_conflict_action);
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
  std::unique_ptr<downloads::Search::Params> params(
      downloads::Search::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  DownloadManager* manager = NULL;
  DownloadManager* incognito_manager = NULL;
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

  std::unique_ptr<base::ListValue> json_results(new base::ListValue());
  for (DownloadManager::DownloadVector::const_iterator it = results.begin();
       it != results.end(); ++it) {
    DownloadItem* download_item = *it;
    uint32_t download_id = download_item->GetId();
    bool off_record = ((incognito_manager != NULL) &&
                       (incognito_manager->GetDownload(download_id) != NULL));
    Profile* profile = Profile::FromBrowserContext(browser_context());
    std::unique_ptr<base::DictionaryValue> json_item(
        DownloadItemToJSON(*it, off_record ? profile->GetPrimaryOTRProfile()
                                           : profile->GetOriginalProfile()));
    json_results->Append(std::move(json_item));
  }
  RecordApiFunctions(DOWNLOADS_FUNCTION_SEARCH);
  return RespondNow(OneArgument(std::move(json_results)));
}

DownloadsPauseFunction::DownloadsPauseFunction() {}

DownloadsPauseFunction::~DownloadsPauseFunction() {}

ExtensionFunction::ResponseAction DownloadsPauseFunction::Run() {
  std::unique_ptr<downloads::Pause::Params> params(
      downloads::Pause::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
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
  std::unique_ptr<downloads::Resume::Params> params(
      downloads::Resume::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
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
  std::unique_ptr<downloads::Resume::Params> params(
      downloads::Resume::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  DownloadItem* download_item = GetDownload(
      browser_context(), include_incognito_information(), params->download_id);
  if (download_item &&
      (download_item->GetState() == DownloadItem::IN_PROGRESS))
    download_item->Cancel(true);
  // |download_item| can be NULL if the download ID was invalid or if the
  // download is not currently active.  Either way, it's not a failure.
  RecordApiFunctions(DOWNLOADS_FUNCTION_CANCEL);
  return RespondNow(NoArguments());
}

DownloadsEraseFunction::DownloadsEraseFunction() {}

DownloadsEraseFunction::~DownloadsEraseFunction() {}

ExtensionFunction::ResponseAction DownloadsEraseFunction::Run() {
  std::unique_ptr<downloads::Erase::Params> params(
      downloads::Erase::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  DownloadManager* manager = NULL;
  DownloadManager* incognito_manager = NULL;
  GetManagers(browser_context(), include_incognito_information(), &manager,
              &incognito_manager);
  DownloadQuery::DownloadVector results;
  std::string error;
  RunDownloadQuery(params->query, manager, incognito_manager, &error, &results);
  if (!error.empty())
    return RespondNow(Error(std::move(error)));
  std::unique_ptr<base::ListValue> json_results(new base::ListValue());
  for (DownloadManager::DownloadVector::const_iterator it = results.begin();
       it != results.end(); ++it) {
    json_results->AppendInteger(static_cast<int>((*it)->GetId()));
    (*it)->Remove();
  }
  RecordApiFunctions(DOWNLOADS_FUNCTION_ERASE);
  return RespondNow(OneArgument(std::move(json_results)));
}

DownloadsRemoveFileFunction::DownloadsRemoveFileFunction() {
}

DownloadsRemoveFileFunction::~DownloadsRemoveFileFunction() {
}

ExtensionFunction::ResponseAction DownloadsRemoveFileFunction::Run() {
  std::unique_ptr<downloads::RemoveFile::Params> params(
      downloads::RemoveFile::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
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
    DownloadsAcceptDangerFunction::on_prompt_created_ = NULL;

ExtensionFunction::ResponseAction DownloadsAcceptDangerFunction::Run() {
  std::unique_ptr<downloads::AcceptDanger::Params> params(
      downloads::AcceptDanger::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
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
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&DownloadsAcceptDangerFunction::PromptOrWait, this,
                         download_id, retries - 1),
          base::TimeDelta::FromMilliseconds(100));
      return;
    }
    Respond(Error(download_extension_errors::kInvisibleContext));
    return;
  }
  RecordApiFunctions(DOWNLOADS_FUNCTION_ACCEPT_DANGER);
  // DownloadDangerPrompt displays a modal dialog using native widgets that the
  // user must either accept or cancel. It cannot be scripted.
  DownloadDangerPrompt* prompt = DownloadDangerPrompt::Create(
      download_item,
      web_contents,
      true,
      base::Bind(&DownloadsAcceptDangerFunction::DangerPromptCallback,
                 this, download_id));
  // DownloadDangerPrompt deletes itself
  if (on_prompt_created_ && !on_prompt_created_->is_null())
    on_prompt_created_->Run(prompt);
  // Function finishes in DangerPromptCallback().
}

void DownloadsAcceptDangerFunction::DangerPromptCallback(
    int download_id, DownloadDangerPrompt::Action action) {
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
  std::unique_ptr<downloads::Show::Params> params(
      downloads::Show::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
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
  DownloadManager* manager = NULL;
  DownloadManager* incognito_manager = NULL;
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
  std::unique_ptr<downloads::Open::Params> params(
      downloads::Open::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  DownloadItem* download_item = GetDownload(
      browser_context(), include_incognito_information(), params->download_id);
  std::string error;
  if (InvalidId(download_item, &error) ||
      Fault(!user_gesture(), download_extension_errors::kUserGesture, &error) ||
      Fault(download_item->GetState() != DownloadItem::COMPLETE,
            download_extension_errors::kNotComplete, &error) ||
      Fault(!extension()->permissions_data()->HasAPIPermission(
                APIPermission::kDownloadsOpen),
            download_extension_errors::kOpenPermission, &error)) {
    return RespondNow(Error(std::move(error)));
  }
  Browser* browser = ChromeExtensionFunctionDetails(this).GetCurrentBrowser();
  if (Fault(!browser, download_extension_errors::kInvisibleContext, &error))
    return RespondNow(Error(std::move(error)));
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (Fault(!web_contents, download_extension_errors::kInvisibleContext,
            &error))
    return RespondNow(Error(std::move(error)));
  // Extensions with debugger permission could fake user gestures and should
  // not be trusted.
  if (GetSenderWebContents() &&
      GetSenderWebContents()->HasRecentInteractiveInputEvent() &&
      !extension()->permissions_data()->HasAPIPermission(
          APIPermission::kDebugger)) {
    download_item->OpenDownload();
    return RespondNow(NoArguments());
  }
  // Prompt user for ack to open the download.
  // TODO(qinmin): check if user prefers to open all download using the same
  // extension, or check the recent user gesture on the originating webcontents
  // to avoid showing the prompt.
  DownloadOpenPrompt* download_open_prompt =
      DownloadOpenPrompt::CreateDownloadOpenConfirmationDialog(
          web_contents, extension()->name(), download_item->GetFullPath(),
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
  std::unique_ptr<downloads::SetShelfEnabled::Params> params(
      downloads::SetShelfEnabled::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  // TODO(devlin): Solve this with the feature system.
  if (!extension()->permissions_data()->HasAPIPermission(
          APIPermission::kDownloadsShelf)) {
    return RespondNow(Error(download_extension_errors::kShelfPermission));
  }

  RecordApiFunctions(DOWNLOADS_FUNCTION_SET_SHELF_ENABLED);
  DownloadManager* manager = NULL;
  DownloadManager* incognito_manager = NULL;
  GetManagers(browser_context(), include_incognito_information(), &manager,
              &incognito_manager);
  DownloadCoreService* service = NULL;
  DownloadCoreService* incognito_service = NULL;
  if (manager) {
    service = DownloadCoreServiceFactory::GetForBrowserContext(
        manager->GetBrowserContext());
    service->GetExtensionEventRouter()->SetShelfEnabled(extension(),
                                                        params->enabled);
  }
  if (incognito_manager) {
    incognito_service = DownloadCoreServiceFactory::GetForBrowserContext(
        incognito_manager->GetBrowserContext());
    incognito_service->GetExtensionEventRouter()->SetShelfEnabled(
        extension(), params->enabled);
  }

  BrowserList* browsers = BrowserList::GetInstance();
  if (browsers) {
    for (auto iter = browsers->begin(); iter != browsers->end(); ++iter) {
      const Browser* browser = *iter;
      DownloadCoreService* current_service =
          DownloadCoreServiceFactory::GetForBrowserContext(browser->profile());
      if (((current_service == service) ||
           (current_service == incognito_service)) &&
          browser->window()->IsDownloadShelfVisible() &&
          !current_service->IsShelfEnabled())
        browser->window()->GetDownloadShelf()->Close();
    }
  }

  if (params->enabled &&
      ((manager && !service->IsShelfEnabled()) ||
       (incognito_manager && !incognito_service->IsShelfEnabled()))) {
    return RespondNow(Error(download_extension_errors::kShelfDisabled));
  }

  return RespondNow(NoArguments());
}

DownloadsGetFileIconFunction::DownloadsGetFileIconFunction()
    : icon_extractor_(new DownloadFileIconExtractorImpl()) {
}

DownloadsGetFileIconFunction::~DownloadsGetFileIconFunction() {}

void DownloadsGetFileIconFunction::SetIconExtractorForTesting(
    DownloadFileIconExtractor* extractor) {
  DCHECK(extractor);
  icon_extractor_.reset(extractor);
}

ExtensionFunction::ResponseAction DownloadsGetFileIconFunction::Run() {
  std::unique_ptr<downloads::GetFileIcon::Params> params(
      downloads::GetFileIcon::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  const downloads::GetFileIconOptions* options =
      params->options.get();
  int icon_size = kDefaultIconSize;
  if (options && options->size.get())
    icon_size = *options->size;
  DownloadItem* download_item = GetDownload(
      browser_context(), include_incognito_information(), params->download_id);
  std::string error;
  if (InvalidId(download_item, &error) ||
      Fault(download_item->GetTargetFilePath().empty(),
            download_extension_errors::kEmptyFile, &error))
    return RespondNow(Error(std::move(error)));
  // In-progress downloads return the intermediate filename for GetFullPath()
  // which doesn't have the final extension. Therefore a good file icon can't be
  // found, so use GetTargetFilePath() instead.
  DCHECK(icon_extractor_.get());
  DCHECK(icon_size == 16 || icon_size == 32);
  float scale = 1.0;
  content::WebContents* web_contents =
      dispatcher()->GetVisibleWebContents();
  if (web_contents && web_contents->GetRenderWidgetHostView())
    scale = web_contents->GetRenderWidgetHostView()->GetDeviceScaleFactor();
  EXTENSION_FUNCTION_VALIDATE(icon_extractor_->ExtractIconURLForPath(
      download_item->GetTargetFilePath(),
      scale,
      IconLoaderSizeFromPixelSize(icon_size),
      base::Bind(&DownloadsGetFileIconFunction::OnIconURLExtracted, this)));
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
  Respond(OneArgument(std::make_unique<base::Value>(url)));
}

ExtensionDownloadsEventRouter::ExtensionDownloadsEventRouter(
    Profile* profile,
    DownloadManager* manager)
    : profile_(profile), notifier_(manager, this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(profile_);
  extension_registry_observer_.Add(ExtensionRegistry::Get(profile_));
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

void ExtensionDownloadsEventRouter::SetShelfEnabled(const Extension* extension,
                                                    bool enabled) {
  auto iter = shelf_disabling_extensions_.find(extension);
  if (iter == shelf_disabling_extensions_.end()) {
    if (!enabled)
      shelf_disabling_extensions_.insert(extension);
  } else if (enabled) {
    shelf_disabling_extensions_.erase(extension);
  }
}

bool ExtensionDownloadsEventRouter::IsShelfEnabled() const {
  return shelf_disabling_extensions_.empty();
}

// The method by which extensions hook into the filename determination process
// is based on the method by which the omnibox API allows extensions to hook
// into the omnibox autocompletion process. Extensions that wish to play a part
// in the filename determination process call
// chrome.downloads.onDeterminingFilename.addListener, which adds an
// EventListener object to ExtensionEventRouter::listeners().
//
// When a download's filename is being determined, DownloadTargetDeterminer (via
// ChromeDownloadManagerDelegate (CDMD) ::NotifyExtensions()) passes 2 callbacks
// to ExtensionDownloadsEventRouter::OnDeterminingFilename (ODF), which stores
// the callbacks in the item's ExtensionDownloadsEventRouterData (EDERD) along
// with all of the extension IDs that are listening for onDeterminingFilename
// events. ODF dispatches chrome.downloads.onDeterminingFilename.
//
// When the extension's event handler calls |suggestCallback|,
// downloads_custom_bindings.js calls
// DownloadsInternalDetermineFilenameFunction::RunAsync, which calls
// EDER::DetermineFilename, which notifies the item's EDERD.
//
// When the last extension's event handler returns, EDERD calls one of the two
// callbacks that CDMD passed to ODF, allowing DownloadTargetDeterminer to
// continue the filename determination process. If multiple extensions wish to
// override the filename, then the extension that was last installed wins.

void ExtensionDownloadsEventRouter::OnDeterminingFilename(
    DownloadItem* item,
    const base::FilePath& suggested_path,
    const base::Closure& no_change,
    const FilenameChangedCallback& change) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ExtensionDownloadsEventRouterData* data =
      ExtensionDownloadsEventRouterData::Get(item);
  if (!data) {
    no_change.Run();
    return;
  }
  data->BeginFilenameDetermination(no_change, change);
  bool any_determiners = false;
  std::unique_ptr<base::DictionaryValue> json =
      DownloadItemToJSON(item, profile_);
  json->SetString(kFilenameKey, suggested_path.LossyDisplayName());
  DispatchEvent(events::DOWNLOADS_ON_DETERMINING_FILENAME,
                downloads::OnDeterminingFilename::kEventName, false,
                base::Bind(&OnDeterminingFilenameWillDispatchCallback,
                           &any_determiners, data),
                std::move(json));
  if (!any_determiners) {
    data->ClearPendingDeterminers();
    if (!data->creator_suggested_filename().empty() ||
        (data->creator_conflict_action() !=
         downloads::FILENAME_CONFLICT_ACTION_UNIQUIFY)) {
      change.Run(data->creator_suggested_filename(),
                 ConvertConflictAction(data->creator_conflict_action()));
      // If all listeners are removed, don't keep |data| around.
      data->ResetCreatorSuggestion();
    } else {
      no_change.Run();
    }
  }
}

void ExtensionDownloadsEventRouter::DetermineFilenameInternal(
    const base::FilePath& filename,
    downloads::FilenameConflictAction conflict_action,
    const std::string& suggesting_extension_id,
    const base::Time& suggesting_install_time,
    const std::string& incumbent_extension_id,
    const base::Time& incumbent_install_time,
    std::string* winner_extension_id,
    base::FilePath* determined_filename,
    downloads::FilenameConflictAction* determined_conflict_action,
    WarningSet* warnings) {
  DCHECK(!filename.empty() ||
         (conflict_action != downloads::FILENAME_CONFLICT_ACTION_UNIQUIFY));
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
        suggesting_extension_id,
        incumbent_extension_id,
        filename,
        *determined_filename));
    return;
  }

  *winner_extension_id = suggesting_extension_id;
  warnings->insert(Warning::CreateDownloadFilenameConflictWarning(
      incumbent_extension_id,
      suggesting_extension_id,
      *determined_filename,
      filename));
  *determined_filename = filename;
  *determined_conflict_action = conflict_action;
}

bool ExtensionDownloadsEventRouter::DetermineFilename(
    content::BrowserContext* browser_context,
    bool include_incognito,
    const std::string& ext_id,
    int download_id,
    const base::FilePath& const_filename,
    downloads::FilenameConflictAction conflict_action,
    std::string* error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordApiFunctions(DOWNLOADS_FUNCTION_DETERMINE_FILENAME);
  DownloadItem* item =
      GetDownload(browser_context, include_incognito, download_id);
  ExtensionDownloadsEventRouterData* data =
      item ? ExtensionDownloadsEventRouterData::Get(item) : NULL;
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
  filename = (valid_filename ? filename.NormalizePathSeparators() :
              base::FilePath());
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
  bool determiner_removed = (
      details.event_name == downloads::OnDeterminingFilename::kEventName);
  EventRouter* router = EventRouter::Get(profile_);
  bool any_listeners =
    router->HasEventListener(downloads::OnChanged::kEventName) ||
    router->HasEventListener(downloads::OnDeterminingFilename::kEventName);
  if (!determiner_removed && any_listeners)
    return;
  DownloadManager::DownloadVector items;
  manager->GetAllDownloads(&items);
  for (DownloadManager::DownloadVector::const_iterator iter =
       items.begin();
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
    if (!any_listeners &&
        data->creator_suggested_filename().empty()) {
      ExtensionDownloadsEventRouterData::Remove(*iter);
    }
  }
}

// That's all the methods that have to do with filename determination. The rest
// have to do with the other, less special events.

void ExtensionDownloadsEventRouter::OnDownloadCreated(
    DownloadManager* manager, DownloadItem* download_item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!ShouldExport(*download_item))
    return;

  EventRouter* router = EventRouter::Get(profile_);
  // Avoid allocating a bunch of memory in DownloadItemToJSON if it isn't going
  // to be used.
  if (!router ||
      (!router->HasEventListener(downloads::OnCreated::kEventName) &&
       !router->HasEventListener(downloads::OnChanged::kEventName) &&
       !router->HasEventListener(
            downloads::OnDeterminingFilename::kEventName))) {
    return;
  }

  // download_item->GetFileExternallyRemoved() should always return false for
  // unfinished download.
  std::unique_ptr<base::DictionaryValue> json_item(
      DownloadItemToJSON(download_item, profile_));
  DispatchEvent(events::DOWNLOADS_ON_CREATED, downloads::OnCreated::kEventName,
                true, Event::WillDispatchCallback(),
                json_item->CreateDeepCopy());
  if (!ExtensionDownloadsEventRouterData::Get(download_item) &&
      (router->HasEventListener(downloads::OnChanged::kEventName) ||
       router->HasEventListener(
           downloads::OnDeterminingFilename::kEventName))) {
    new ExtensionDownloadsEventRouterData(
        download_item, download_item->GetState() == DownloadItem::COMPLETE
                           ? nullptr
                           : std::move(json_item));
  }
}

void ExtensionDownloadsEventRouter::OnDownloadUpdated(
    DownloadManager* manager, DownloadItem* download_item) {
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
    data = new ExtensionDownloadsEventRouterData(
        download_item,
        std::unique_ptr<base::DictionaryValue>(new base::DictionaryValue()));
  }
  std::unique_ptr<base::DictionaryValue> new_json;
  std::unique_ptr<base::DictionaryValue> delta(new base::DictionaryValue());
  delta->SetInteger(kIdKey, download_item->GetId());
  bool changed = false;
  // For completed downloads, update can only happen when file is removed.
  if (data->is_download_completed()) {
    if (data->is_completed_download_deleted() !=
        download_item->GetFileExternallyRemoved()) {
      DCHECK(!data->is_completed_download_deleted());
      DCHECK(download_item->GetFileExternallyRemoved());
      std::string exists = kExistsKey;
      delta->SetBoolean(exists + ".current", false);
      delta->SetBoolean(exists + ".previous", true);
      changed = true;
    }
  } else {
    new_json = DownloadItemToJSON(download_item, profile_);
    std::set<std::string> new_fields;
    // For each field in the new json representation of the download_item except
    // the bytesReceived field, if the field has changed from the previous old
    // json, set the differences in the |delta| object and remember that
    // something significant changed.
    for (base::DictionaryValue::Iterator iter(*new_json); !iter.IsAtEnd();
         iter.Advance()) {
      new_fields.insert(iter.key());
      if (IsDownloadDeltaField(iter.key())) {
        const base::Value* old_value = NULL;
        if (!data->json().HasKey(iter.key()) ||
            (data->json().Get(iter.key(), &old_value) &&
             !iter.value().Equals(old_value))) {
          delta->Set(iter.key() + ".current", iter.value().CreateDeepCopy());
          if (old_value)
            delta->Set(iter.key() + ".previous", old_value->CreateDeepCopy());
          changed = true;
        }
      }
    }

    // If a field was in the previous json but is not in the new json, set the
    // difference in |delta|.
    for (base::DictionaryValue::Iterator iter(data->json()); !iter.IsAtEnd();
         iter.Advance()) {
      if ((new_fields.find(iter.key()) == new_fields.end()) &&
          IsDownloadDeltaField(iter.key())) {
        // estimatedEndTime disappears after completion, but bytesReceived
        // stays.
        delta->Set(iter.key() + ".previous", iter.value().CreateDeepCopy());
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
                  Event::WillDispatchCallback(), std::move(delta));
    data->OnChangedFired();
  }
}

void ExtensionDownloadsEventRouter::OnDownloadRemoved(
    DownloadManager* manager, DownloadItem* download_item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!ShouldExport(*download_item))
    return;
  DispatchEvent(
      events::DOWNLOADS_ON_ERASED, downloads::OnErased::kEventName, true,
      Event::WillDispatchCallback(),
      std::make_unique<base::Value>(static_cast<int>(download_item->GetId())));
}

void ExtensionDownloadsEventRouter::DispatchEvent(
    events::HistogramValue histogram_value,
    const std::string& event_name,
    bool include_incognito,
    Event::WillDispatchCallback will_dispatch_callback,
    std::unique_ptr<base::Value> arg) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!EventRouter::Get(profile_))
    return;
  std::unique_ptr<base::ListValue> args(new base::ListValue());
  args->Append(std::move(arg));
  std::string json_args;
  base::JSONWriter::Write(*args, &json_args);
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
      (include_incognito && !profile_->IsOffTheRecord()) ? nullptr : profile_;
  auto event =
      std::make_unique<Event>(histogram_value, event_name, std::move(args),
                              restrict_to_browser_context);
  event->will_dispatch_callback = std::move(will_dispatch_callback);
  EventRouter::Get(profile_)->BroadcastEvent(std::move(event));
  DownloadsNotificationSource notification_source;
  notification_source.event_name = event_name;
  notification_source.profile = profile_;
  content::Source<DownloadsNotificationSource> content_source(
      &notification_source);
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_DOWNLOADS_EVENT,
      content_source,
      content::Details<std::string>(&json_args));
}

void ExtensionDownloadsEventRouter::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto iter = shelf_disabling_extensions_.find(extension);
  if (iter != shelf_disabling_extensions_.end())
    shelf_disabling_extensions_.erase(iter);
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
