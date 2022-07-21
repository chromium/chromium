// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/chrome_download_manager_delegate.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_dialog_types.h"
#include "chrome/browser/download/download_file_picker.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/download/download_target_determiner.h"
#include "chrome/browser/download/mixed_content_download_blocking.h"
#include "chrome/browser/download/save_package_file_picker.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/file_system/rename_handler.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/download_protection/deep_scanning_request.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/net/safe_search_util.h"
#include "chrome/common/pdf_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_stats.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/download/download_stats.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "components/services/quarantine/public/mojom/quarantine.mojom.h"
#include "components/services/quarantine/quarantine_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/common/origin_util.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/filename_util.h"
#include "net/base/mime_util.h"
#include "net/base/network_change_notifier.h"
#include "ppapi/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/content_uri_utils.h"
#include "base/android/path_utils.h"
#include "chrome/browser/download/android/chrome_duplicate_download_infobar_delegate.h"
#include "chrome/browser/download/android/download_controller.h"
#include "chrome/browser/download/android/download_dialog_bridge.h"
#include "chrome/browser/download/android/download_manager_service.h"
#include "chrome/browser/download/android/download_message_bridge.h"
#include "chrome/browser/download/android/download_open_source.h"
#include "chrome/browser/download/android/download_utils.h"
#include "chrome/browser/download/android/duplicate_download_dialog_bridge_delegate.h"
#include "chrome/browser/download/android/mixed_content_download_dialog_bridge.h"
#include "chrome/browser/download/android/mixed_content_download_infobar_delegate.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "net/http/http_content_disposition.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/downloads/downloads_api.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/constants.h"
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/offline_page_utils.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#endif

using content::BrowserThread;
using content::DownloadManager;
using download::DownloadItem;
using download::DownloadPathReservationTracker;
using download::PathValidationResult;
using safe_browsing::DownloadFileType;
using safe_browsing::DownloadProtectionService;
using ConnectionType = net::NetworkChangeNotifier::ConnectionType;

namespace {

#if !BUILDFLAG(IS_ANDROID)
// How long an ephemeral warning lasts before being automatically canceled (if
// there is no user interaction).
constexpr base::TimeDelta kEphemeralWarningLifetimeBeforeCancel =
    base::Hours(1);
#endif

// Used with GetPlatformDownloadPath() to indicate which platform path to
// return.
enum PlatformDownloadPathType {
  // Return the platform specific target path.
  PLATFORM_TARGET_PATH,

  // Return the platform specific current path. If the download is in-progress
  // and the download location is a local filesystem path, then
  // GetPlatformDownloadPath will return the path to the intermediate file.
  PLATFORM_CURRENT_PATH
};

// Returns a path in the form that that is expected by platform_util::OpenItem /
// platform_util::ShowItemInFolder / DownloadTargetDeterminer.
//
// How the platform path is determined is based on PlatformDownloadPathType.
base::FilePath GetPlatformDownloadPath(const DownloadItem* download,
                                       PlatformDownloadPathType path_type) {
  if (path_type == PLATFORM_TARGET_PATH)
    return download->GetTargetFilePath();
  return download->GetFullPath();
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
// Callback invoked by DownloadProtectionService::CheckClientDownload.
// |is_content_check_supported| is true if the SB service supports scanning the
// download for malicious content.
// |callback| is invoked with a danger type determined as follows:
//
// Danger type is (in order of preference):
//   * DANGEROUS_URL, if the URL is a known malware site.
//   * MAYBE_DANGEROUS_CONTENT, if the content will be scanned for
//         malware. I.e. |is_content_check_supported| is true.
//   * ALLOWLISTED_BY_POLICY, if the download matches enterprise whitelist.
//   * NOT_DANGEROUS.
void CheckDownloadUrlDone(
    DownloadTargetDeterminerDelegate::CheckDownloadUrlCallback callback,
    const std::vector<GURL>& download_urls,
    bool is_content_check_supported,
    safe_browsing::DownloadCheckResult result) {
  safe_browsing::WebUIInfoSingleton::GetInstance()->AddToDownloadUrlsChecked(
      download_urls, result);
  download::DownloadDangerType danger_type;
  if (result == safe_browsing::DownloadCheckResult::SAFE ||
      result == safe_browsing::DownloadCheckResult::UNKNOWN) {
    // If this type of files is handled by the enhanced SafeBrowsing download
    // protection, mark it as potentially dangerous content until we are done
    // with scanning it.
    if (is_content_check_supported)
      danger_type = download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT;
    else
      danger_type = download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;
  } else if (result ==
             safe_browsing::DownloadCheckResult::ALLOWLISTED_BY_POLICY) {
    danger_type = download::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY;
  } else {
    // If the URL is malicious, we'll use that as the danger type. The results
    // of the content check, if one is performed, will be ignored.
    danger_type = download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL;
  }
  std::move(callback).Run(danger_type);
}

#endif  // FULL_SAFE_BROWSING

// Called asynchronously to determine the MIME type for |path|.
std::string GetMimeType(const base::FilePath& path) {
#if BUILDFLAG(IS_ANDROID)
  if (path.IsContentUri()) {
    return base::GetContentUriMimeType(path);
  }
#endif
  std::string mime_type;
  net::GetMimeTypeFromFile(path, &mime_type);
  return mime_type;
}

// On Android, Chrome wants to warn the user of file overwrites rather than
// uniquify.
#if BUILDFLAG(IS_ANDROID)
const DownloadPathReservationTracker::FilenameConflictAction
    kDefaultPlatformConflictAction = DownloadPathReservationTracker::PROMPT;
#else
const DownloadPathReservationTracker::FilenameConflictAction
    kDefaultPlatformConflictAction = DownloadPathReservationTracker::UNIQUIFY;
#endif

// Invoked when whether download can proceed is determined.
// Args: whether storage permission is granted and whether the download is
// allowed.
using CanDownloadCallback =
    base::OnceCallback<void(bool /* storage permission granted */,
                            bool /*allow*/)>;

void CheckCanDownload(const content::WebContents::Getter& web_contents_getter,
                      const GURL& url,
                      const std::string& request_method,
                      absl::optional<url::Origin> request_initiator,
                      bool from_download_cross_origin_redirect,
                      CanDownloadCallback can_download_cb) {
  DownloadRequestLimiter* limiter =
      g_browser_process->download_request_limiter();
  if (limiter) {
    limiter->CanDownload(web_contents_getter, url, request_method,
                         std::move(request_initiator),
                         from_download_cross_origin_redirect,
                         base::BindOnce(std::move(can_download_cb), true));
  }
}

#if BUILDFLAG(IS_ANDROID)
// TODO(qinmin): reuse the similar function defined in
// DownloadResourceThrottle.
void OnDownloadAcquireFileAccessPermissionDone(
    const content::WebContents::Getter& web_contents_getter,
    const GURL& url,
    const std::string& request_method,
    absl::optional<url::Origin> request_initiator,
    CanDownloadCallback can_download_cb,
    bool granted) {
  if (granted) {
    CheckCanDownload(web_contents_getter, url, request_method,
                     std::move(request_initiator),
                     false /* from_download_cross_origin_redirect */,
                     std::move(can_download_cb));
  } else {
    std::move(can_download_cb).Run(false, false);
  }
}

// Overlays download location dialog result to target determiner.
void OnDownloadDialogClosed(
    DownloadTargetDeterminerDelegate::ConfirmationCallback callback,
    DownloadDialogResult result) {
  switch (result.location_result) {
    case DownloadLocationDialogResult::USER_CONFIRMED:
      std::move(callback).Run(DownloadConfirmationResult::CONFIRMED_WITH_DIALOG,
                              result.file_path,
                              std::move(result.download_schedule));
      break;
    case DownloadLocationDialogResult::USER_CANCELED:
      std::move(callback).Run(DownloadConfirmationResult::CANCELED,
                              base::FilePath(), absl::nullopt);
      break;
    case DownloadLocationDialogResult::DUPLICATE_DIALOG:
      // TODO(xingliu): Figure out the dialog behavior on multiple downloads.
      // Currently we just let other downloads continue, which doesn't make
      // sense.
      std::move(callback).Run(
          DownloadConfirmationResult::CONTINUE_WITHOUT_CONFIRMATION,
          result.file_path, std::move(result.download_schedule));
      break;
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

void OnCheckExistingDownloadPathDone(
    std::unique_ptr<DownloadTargetInfo> target_info,
    content::DownloadTargetCallback callback,
    bool file_exists) {
  if (file_exists) {
    RecordDownloadCancelReason(DownloadCancelReason::kExistingDownloadPath);
    target_info->result = download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED;
  }

  std::move(callback).Run(
      target_info->target_path, target_info->target_disposition,
      target_info->danger_type, target_info->mixed_content_status,
      target_info->intermediate_path, target_info->display_name,
      target_info->mime_type, target_info->result);
}

#if BUILDFLAG(IS_ANDROID)
// Callback used by Mixed Download infobar on Android. Unlike on Desktop, this
// infobar's entire life occurs prior to download start.
void HandleMixedDownloadInfoBarResult(
    download::DownloadItem* download_item,
    std::unique_ptr<DownloadTargetInfo> target_info,
    content::DownloadTargetCallback callback,
    bool should_download) {
  // If the download should be blocked, we can call the callback directly.
  if (!should_download) {
    std::move(callback).Run(target_info->target_path,
                            target_info->target_disposition,
                            download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
                            DownloadItem::MixedContentStatus::SILENT_BLOCK,
                            target_info->intermediate_path,
                            target_info->display_name, target_info->mime_type,
                            download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED);
    return;
  }
  target_info->mixed_content_status =
      download::DownloadItem::MixedContentStatus::VALIDATED;

  // Otherwise, proceed as normal and check for a separate reservation with the
  // same target path. If such a reservation exists, cancel this reservation.
  const base::FilePath target_path = target_info->target_path;
  DownloadPathReservationTracker::CheckDownloadPathForExistingDownload(
      target_path, download_item,
      base::BindOnce(&OnCheckExistingDownloadPathDone, std::move(target_info),
                     std::move(callback)));
}
#endif

void MaybeReportDangerousDownloadBlocked(
    DownloadPrefs::DownloadRestriction download_restriction,
    std::string danger_type,
    std::string download_path,
    download::DownloadItem* download) {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  if (download_restriction !=
          DownloadPrefs::DownloadRestriction::POTENTIALLY_DANGEROUS_FILES &&
      download_restriction !=
          DownloadPrefs::DownloadRestriction::DANGEROUS_FILES &&
      download_restriction !=
          DownloadPrefs::DownloadRestriction::MALICIOUS_FILES) {
    return;
  }

  content::BrowserContext* browser_context =
      content::DownloadItemUtils::GetBrowserContext(download);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile)
    return;

  // If |download| has a deep scanning malware verdict, then it means the
  // dangerous file has already been reported.
  auto* scan_result = static_cast<enterprise_connectors::ScanResult*>(
      download->GetUserData(enterprise_connectors::ScanResult::kKey));
  if (scan_result) {
    for (const auto& metadata : scan_result->file_metadata) {
      if (enterprise_connectors::ContainsMalwareVerdict(metadata.scan_response))
        return;
    }
  }

  auto* router =
      extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile);
  if (router) {
    std::string raw_digest_sha256 = download->GetHash();
    router->OnDangerousDownloadEvent(
        download->GetURL(), download_path,
        base::HexEncode(raw_digest_sha256.data(), raw_digest_sha256.size()),
        danger_type, download->GetMimeType(), /*scan_id*/ "",
        download->GetTotalBytes(), safe_browsing::EventResult::BLOCKED);
  }
#endif
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
download::DownloadDangerType SavePackageDangerType(
    safe_browsing::DownloadCheckResult result) {
  switch (result) {
    case safe_browsing::DownloadCheckResult::ASYNC_SCANNING:
      return download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING;
    case safe_browsing::DownloadCheckResult::SENSITIVE_CONTENT_WARNING:
      return download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING;
    case safe_browsing::DownloadCheckResult::UNKNOWN:
      // Failed scans with an unknown result should fail-open, so treat them as
      // if they're not dangerous.
      return download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;
    case safe_browsing::DownloadCheckResult::DEEP_SCANNED_SAFE:
      return download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE;
    case safe_browsing::DownloadCheckResult::BLOCKED_UNSUPPORTED_FILE_TYPE:
      return download::DOWNLOAD_DANGER_TYPE_BLOCKED_UNSUPPORTED_FILETYPE;
    case safe_browsing::DownloadCheckResult::BLOCKED_PASSWORD_PROTECTED:
      return download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED;
    case safe_browsing::DownloadCheckResult::BLOCKED_TOO_LARGE:
      return download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE;
    case safe_browsing::DownloadCheckResult::SENSITIVE_CONTENT_BLOCK:
      return download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK;

    default:
      NOTREACHED();
      return download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;
  }
}
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)

}  // namespace

ChromeDownloadManagerDelegate::ChromeDownloadManagerDelegate(Profile* profile)
    : profile_(profile),
      next_download_id_(download::DownloadItem::kInvalidId),
      next_id_retrieved_(false),
      download_prefs_(new DownloadPrefs(profile)),
      is_file_picker_showing_(false) {
#if BUILDFLAG(IS_ANDROID)
  download_dialog_bridge_ = std::make_unique<DownloadDialogBridge>();
  download_message_bridge_ = std::make_unique<DownloadMessageBridge>();
#endif
}

ChromeDownloadManagerDelegate::~ChromeDownloadManagerDelegate() {
  // If a DownloadManager was set for this, Shutdown() must be called.
  DCHECK(!download_manager_);
}

void ChromeDownloadManagerDelegate::SetDownloadManager(DownloadManager* dm) {
  if (download_manager_) {
    download_manager_->RemoveObserver(this);
  }

  download_manager_ = dm;

  safe_browsing::SafeBrowsingService* sb_service =
      g_browser_process->safe_browsing_service();
  if (sb_service && !profile_->IsOffTheRecord()) {
    // Include this download manager in the set monitored by safe browsing.
    sb_service->AddDownloadManager(dm);
  }

  if (download_manager_) {
    download_manager_->AddObserver(this);
  }
}

#if BUILDFLAG(IS_ANDROID)
void ChromeDownloadManagerDelegate::ShowDownloadDialog(
    gfx::NativeWindow native_window,
    int64_t total_bytes,
    DownloadLocationDialogType dialog_type,
    const base::FilePath& suggested_path,
    DownloadDialogBridge::DialogCallback callback) {
  DCHECK(download_dialog_bridge_);
  auto connection_type = net::NetworkChangeNotifier::GetConnectionType();
  bool is_incognito = profile_->IsOffTheRecord();

  download_dialog_bridge_->ShowDialog(
      native_window, total_bytes, connection_type, dialog_type, suggested_path,
      is_incognito, std::move(callback));
}

void ChromeDownloadManagerDelegate::SetDownloadDialogBridgeForTesting(
    DownloadDialogBridge* bridge) {
  download_dialog_bridge_.reset(bridge);
}
#endif  // BUILDFLAG(IS_ANDROID)

void ChromeDownloadManagerDelegate::Shutdown() {
  download_prefs_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (download_manager_) {
    download_manager_->RemoveObserver(this);
    download_manager_ = nullptr;
  }
}

content::DownloadIdCallback
ChromeDownloadManagerDelegate::GetDownloadIdReceiverCallback() {
  return base::BindOnce(&ChromeDownloadManagerDelegate::SetNextId,
                        weak_ptr_factory_.GetWeakPtr());
}

void ChromeDownloadManagerDelegate::SetNextId(uint32_t next_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!profile_->IsOffTheRecord());

  // |download::DownloadItem::kInvalidId| will be returned only when history
  // database failed to initialize.
  bool history_db_available = (next_id != download::DownloadItem::kInvalidId);
  RecordDatabaseAvailability(history_db_available);
  if (history_db_available)
    next_download_id_ = next_id;
  next_id_retrieved_ = true;

  IdCallbackVector callbacks;
  id_callbacks_.swap(callbacks);
  for (auto& callback : callbacks)
    ReturnNextId(std::move(callback));
}

void ChromeDownloadManagerDelegate::GetNextId(
    content::DownloadIdCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (profile_->IsOffTheRecord()) {
    profile_->GetOriginalProfile()->GetDownloadManager()->GetNextId(
        std::move(callback));
    return;
  }
  if (!next_id_retrieved_) {
    id_callbacks_.push_back(std::move(callback));
    return;
  }
  ReturnNextId(std::move(callback));
}

void ChromeDownloadManagerDelegate::ReturnNextId(
    content::DownloadIdCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!profile_->IsOffTheRecord());
  // kInvalidId is returned to indicate the error.
  std::move(callback).Run(next_download_id_);
  if (next_download_id_ != download::DownloadItem::kInvalidId)
    ++next_download_id_;
}

bool ChromeDownloadManagerDelegate::DetermineDownloadTarget(
    DownloadItem* download,
    content::DownloadTargetCallback* callback) {
  if (download->GetTargetFilePath().empty() &&
      download->GetMimeType() == kPDFMimeType && !download->HasUserGesture()) {
    ReportPDFLoadStatus(PDFLoadStatus::kTriggeredNoGestureDriveByDownload);
  }

  DownloadTargetDeterminer::CompletionCallback target_determined_callback =
      base::BindOnce(&ChromeDownloadManagerDelegate::OnDownloadTargetDetermined,
                     weak_ptr_factory_.GetWeakPtr(), download->GetId(),
                     std::move(*callback));
  base::FilePath download_path =
      GetPlatformDownloadPath(download, PLATFORM_TARGET_PATH);
  DownloadPathReservationTracker::FilenameConflictAction action =
      kDefaultPlatformConflictAction;
#if BUILDFLAG(IS_ANDROID)
  if (!download_path.empty())
    action = DownloadPathReservationTracker::UNIQUIFY;
#endif
  DownloadTargetDeterminer::Start(download, download_path, action,
                                  download_prefs_.get(), this,
                                  std::move(target_determined_callback));
  return true;
}

bool ChromeDownloadManagerDelegate::ShouldAutomaticallyOpenFile(
    const GURL& url,
    const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (path.Extension().empty())
    return false;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // TODO(crbug.com/1077929): This determination is done based on |path|, while
  // ShouldOpenDownload() detects extension downloads based on the
  // characteristics of the download. Reconcile this.
  if (path.MatchesExtension(extensions::kExtensionFileExtension))
    return false;
#endif
  return download_prefs_->IsAutoOpenEnabled(url, path);
}

bool ChromeDownloadManagerDelegate::ShouldAutomaticallyOpenFileByPolicy(
    const GURL& url,
    const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (path.Extension().empty())
    return false;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // TODO(crbug.com/1077929): This determination is done based on |path|, while
  // ShouldOpenDownload() detects extension downloads based on the
  // characteristics of the download. Reconcile this.
  if (path.MatchesExtension(extensions::kExtensionFileExtension))
    return false;
#endif
  return download_prefs_->IsAutoOpenByPolicy(url, path);
}

// static
void ChromeDownloadManagerDelegate::DisableSafeBrowsing(DownloadItem* item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if BUILDFLAG(FULL_SAFE_BROWSING)
  SafeBrowsingState* state = static_cast<SafeBrowsingState*>(
      item->GetUserData(&SafeBrowsingState::kSafeBrowsingUserDataKey));
  if (!state) {
    auto new_state = std::make_unique<SafeBrowsingState>();
    state = new_state.get();
    item->SetUserData(&SafeBrowsingState::kSafeBrowsingUserDataKey,
                      std::move(new_state));
  }
  state->CompleteDownload();
#endif
}

// static
bool ChromeDownloadManagerDelegate::IsDangerTypeBlocked(
    download::DownloadDangerType danger_type) {
  return danger_type == download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE ||
         danger_type ==
             download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED ||
         danger_type == download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK;
}

bool ChromeDownloadManagerDelegate::IsDownloadReadyForCompletion(
    DownloadItem* item,
    base::OnceClosure internal_complete_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if BUILDFLAG(FULL_SAFE_BROWSING)
  // If this is a chrome triggered download, return true;
  if (!item->RequireSafetyChecks())
    return true;

  if (!download_prefs_->safebrowsing_for_trusted_sources_enabled() &&
      download_prefs_->IsFromTrustedSource(*item)) {
    return true;
  }

  SafeBrowsingState* state = static_cast<SafeBrowsingState*>(
      item->GetUserData(&SafeBrowsingState::kSafeBrowsingUserDataKey));
  if (!state) {
    // Begin the safe browsing download protection check.
    state = new SafeBrowsingState();
    state->set_callback(std::move(internal_complete_callback));
    item->SetUserData(&SafeBrowsingState::kSafeBrowsingUserDataKey,
                      base::WrapUnique(state));
    DownloadProtectionService* service = GetDownloadProtectionService();
    if (service) {
      DVLOG(2) << __func__ << "() Start SB download check for download = "
               << item->DebugString(false);
      if (service->MaybeCheckClientDownload(
              item, base::BindRepeating(
                        &ChromeDownloadManagerDelegate::CheckClientDownloadDone,
                        weak_ptr_factory_.GetWeakPtr(), item->GetId()))) {
        return false;
      }
    }

    // In case the service was disabled between the download starting and now,
    // we need to restore the danger state.
    download::DownloadDangerType danger_type = item->GetDangerType();
    if (DownloadItemModel(item).GetDangerLevel() !=
            DownloadFileType::NOT_DANGEROUS &&
        (danger_type == download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS ||
         danger_type ==
             download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT)) {
      DVLOG(2) << __func__
               << "() SB service disabled. Marking download as DANGEROUS FILE";
      if (ShouldBlockFile(item,
                          download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE)) {
        MaybeReportDangerousDownloadBlocked(
            download_prefs_->download_restriction(), "DANGEROUS_FILE_TYPE",
            item->GetTargetFilePath().AsUTF8Unsafe(), item);

        item->OnContentCheckCompleted(
            // Specifying a dangerous type here would take precedence over the
            // blocking of the file.
            download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
            download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED);
      } else {
        item->OnContentCheckCompleted(
            download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
            download::DOWNLOAD_INTERRUPT_REASON_NONE);
      }
      state->CompleteDownload();
      return false;
    }
  } else if (!state->is_complete() &&
             item->GetDangerType() !=
                 download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED) {
    // Don't complete the download until we have an answer.
    state->set_callback(std::move(internal_complete_callback));
    return false;
  }

#endif
  return true;
}

void ChromeDownloadManagerDelegate::ShouldCompleteDownloadInternal(
    uint32_t download_id,
    base::OnceClosure user_complete_callback) {
  DownloadItem* item = download_manager_->GetDownload(download_id);
  if (!item)
    return;
  // This should be called only once.
  base::RepeatingClosure callback = base::BindRepeating(
      [](base::OnceClosure callback) { std::move(callback).Run(); },
      base::Passed(&user_complete_callback));
  if (ShouldCompleteDownload(item, callback)) {
    // |callback| should not have run when ShouldCompleteDownload() returns
    // true.
    std::move(callback).Run();
  }
}

bool ChromeDownloadManagerDelegate::ShouldCompleteDownload(
    DownloadItem* item,
    base::OnceClosure user_complete_callback) {
  return IsDownloadReadyForCompletion(
      item, base::BindOnce(
                &ChromeDownloadManagerDelegate::ShouldCompleteDownloadInternal,
                weak_ptr_factory_.GetWeakPtr(), item->GetId(),
                std::move(user_complete_callback)));
}

bool ChromeDownloadManagerDelegate::ShouldOpenDownload(
    DownloadItem* item,
    content::DownloadOpenDelayedCallback callback) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (download_crx_util::IsExtensionDownload(*item) &&
      !extensions::WebstoreInstaller::GetAssociatedApproval(*item)) {
    scoped_refptr<extensions::CrxInstaller> crx_installer =
        download_crx_util::OpenChromeExtension(profile_, *item);

    // CRX_INSTALLER_DONE will fire when the install completes.  At that
    // time, Observe() will call the passed callback.
    registrar_.Add(
        this, extensions::NOTIFICATION_CRX_INSTALLER_DONE,
        content::Source<extensions::CrxInstaller>(crx_installer.get()));

    crx_installers_[crx_installer.get()] = std::move(callback);
    // The status text and percent complete indicator will change now
    // that we are installing a CRX.  Update observers so that they pick
    // up the change.
    item->UpdateObservers();
    return false;
  }
#endif

  return true;
}

bool ChromeDownloadManagerDelegate::InterceptDownloadIfApplicable(
    const GURL& url,
    const std::string& user_agent,
    const std::string& content_disposition,
    const std::string& mime_type,
    const std::string& request_origin,
    int64_t content_length,
    bool is_transient,
    content::WebContents* web_contents) {
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // For background service downloads we don't want offline pages backend to
  // intercept the download. |is_transient| flag is used to determine whether
  // the download corresponds to background service. Additionally we don't want
  // offline pages backend to intercept html files explicitly marked as
  // attachments.
  if (!is_transient &&
      !net::HttpContentDisposition(content_disposition, std::string())
           .is_attachment() &&
      offline_pages::OfflinePageUtils::CanDownloadAsOfflinePage(url,
                                                                mime_type)) {
    offline_pages::OfflinePageUtils::ScheduleDownload(
        web_contents, offline_pages::kDownloadNamespace, url,
        offline_pages::OfflinePageUtils::DownloadUIActionFlags::ALL,
        request_origin);
    return true;
  }
#endif
  return false;
}

void ChromeDownloadManagerDelegate::GetSaveDir(
    content::BrowserContext* browser_context,
    base::FilePath* website_save_dir,
    base::FilePath* download_save_dir) {
  *website_save_dir = download_prefs_->SaveFilePath();
  DCHECK(!website_save_dir->empty());
  *download_save_dir = download_prefs_->DownloadPath();
}

void ChromeDownloadManagerDelegate::ChooseSavePath(
    content::WebContents* web_contents,
    const base::FilePath& suggested_path,
    const base::FilePath::StringType& default_extension,
    bool can_save_as_complete,
    content::SavePackagePathPickedCallback callback) {
  // Deletes itself.
  new SavePackageFilePicker(web_contents, suggested_path, default_extension,
                            can_save_as_complete, download_prefs_.get(),
                            std::move(callback));
}

void ChromeDownloadManagerDelegate::SanitizeSavePackageResourceName(
    base::FilePath* filename,
    const GURL& source_url) {
  safe_browsing::FileTypePolicies* file_type_policies =
      safe_browsing::FileTypePolicies::GetInstance();

  const PrefService* prefs = profile_->GetPrefs();
  if (file_type_policies->GetFileDangerLevel(*filename, source_url, prefs) ==
      safe_browsing::DownloadFileType::NOT_DANGEROUS)
    return;

  base::FilePath default_filename = base::FilePath::FromUTF8Unsafe(
      l10n_util::GetStringUTF8(IDS_DEFAULT_DOWNLOAD_FILENAME));
  *filename = filename->AddExtension(default_filename.BaseName().value());
}

void ChromeDownloadManagerDelegate::SanitizeDownloadParameters(
    download::DownloadUrlParameters* params) {
  if (profile_->GetPrefs()->GetBoolean(prefs::kForceGoogleSafeSearch)) {
    GURL safe_url;
    safe_search_util::ForceGoogleSafeSearch(params->url(), &safe_url);
    if (!safe_url.is_empty())
      params->set_url(std::move(safe_url));
  }
}

void ChromeDownloadManagerDelegate::OpenDownloadUsingPlatformHandler(
    DownloadItem* download) {
  base::FilePath platform_path(
      GetPlatformDownloadPath(download, PLATFORM_TARGET_PATH));
  DCHECK(!platform_path.empty());
  platform_util::OpenItem(profile_, platform_path, platform_util::OPEN_FILE,
                          platform_util::OpenOperationCallback());
}

void ChromeDownloadManagerDelegate::OpenDownload(DownloadItem* download) {
  DCHECK_EQ(DownloadItem::COMPLETE, download->GetState());
  DCHECK(!download->GetTargetFilePath().empty());
  if (!download->CanOpenDownload())
    return;

  if (!IsMostRecentDownloadItemAtFilePath(download))
    return;
  MaybeSendDangerousDownloadOpenedReport(download,
                                         false /* show_download_in_folder */);

#if BUILDFLAG(IS_ANDROID)
  DownloadUtils::OpenDownload(download, DownloadOpenSource::kUnknown);
#else

  download::DownloadItemRenameHandler* handler = download->GetRenameHandler();
  if (handler) {
    handler->OpenDownload();
    RecordDownloadOpenMethod(DOWNLOAD_OPEN_METHOD_RENAME_HANDLER);
    return;
  }

  if (!DownloadItemModel(download).ShouldPreferOpeningInBrowser()) {
    RecordDownloadOpenMethod(DOWNLOAD_OPEN_METHOD_DEFAULT_PLATFORM);
    OpenDownloadUsingPlatformHandler(download);
    return;
  }

  content::WebContents* web_contents =
      content::DownloadItemUtils::GetWebContents(download);
  Browser* browser =
      web_contents ? chrome::FindBrowserWithWebContents(web_contents) : nullptr;
  std::unique_ptr<chrome::ScopedTabbedBrowserDisplayer> browser_displayer;
  if (!browser ||
      !browser->CanSupportWindowFeature(Browser::FEATURE_TABSTRIP)) {
    browser_displayer =
        std::make_unique<chrome::ScopedTabbedBrowserDisplayer>(profile_);
    browser = browser_displayer->browser();
  }
  content::OpenURLParams params(
      net::FilePathToFileURL(download->GetTargetFilePath()),
      content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false);

  if (download->GetMimeType() == "application/x-x509-user-cert")
    chrome::ShowSettingsSubPage(browser, "certificates");
  else
    browser->OpenURL(params);

  RecordDownloadOpenMethod(DOWNLOAD_OPEN_METHOD_DEFAULT_BROWSER);
#endif  // BUILDFLAG(IS_ANDROID)
}

bool ChromeDownloadManagerDelegate::IsMostRecentDownloadItemAtFilePath(
    DownloadItem* download) {
  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(download));
  std::vector<Profile*> profiles_to_check =
      profile->GetOriginalProfile()->GetAllOffTheRecordProfiles();
  profiles_to_check.push_back(profile->GetOriginalProfile());

  std::vector<DownloadItem*> all_downloads;
  for (auto* profile_to_check : profiles_to_check) {
    content::DownloadManager* manager = profile_to_check->GetDownloadManager();
    if (manager)
      manager->GetAllDownloads(&all_downloads);
  }

  for (const auto* item : all_downloads) {
    if (item->GetGuid() == download->GetGuid() ||
        item->GetTargetFilePath() != download->GetTargetFilePath())
      continue;

    if (item->GetState() == DownloadItem::IN_PROGRESS)
      return false;
  }

  return true;
}

void ChromeDownloadManagerDelegate::ShowDownloadInShell(
    DownloadItem* download) {
  if (!download->CanShowInFolder())
    return;

  MaybeSendDangerousDownloadOpenedReport(download,
                                         true /* show_download_in_folder */);

  download::DownloadItemRenameHandler* handler = download->GetRenameHandler();
  if (handler) {
    handler->ShowDownloadInContext();
    return;
  }

  base::FilePath platform_path(
      GetPlatformDownloadPath(download, PLATFORM_CURRENT_PATH));
  DCHECK(!platform_path.empty());
  platform_util::ShowItemInFolder(profile_, platform_path);
}

std::string
ChromeDownloadManagerDelegate::ApplicationClientIdForFileScanning() {
  return std::string(chrome::kApplicationClientIDStringForAVScanning);
}

DownloadProtectionService*
ChromeDownloadManagerDelegate::GetDownloadProtectionService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if BUILDFLAG(FULL_SAFE_BROWSING)
  safe_browsing::SafeBrowsingService* sb_service =
      g_browser_process->safe_browsing_service();
  if (sb_service && sb_service->download_protection_service()) {
    return sb_service->download_protection_service();
  }
#endif
  return nullptr;
}

void ChromeDownloadManagerDelegate::GetMixedContentStatus(
    download::DownloadItem* download,
    const base::FilePath& virtual_path,
    GetMixedContentStatusCallback callback) {
  DCHECK(download);
  std::move(callback).Run(
      GetMixedContentStatusForDownload(profile_, virtual_path, download));
}

void ChromeDownloadManagerDelegate::NotifyExtensions(
    DownloadItem* download,
    const base::FilePath& virtual_path,
    NotifyExtensionsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!download->IsTransient());

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::ExtensionDownloadsEventRouter* router =
      DownloadCoreServiceFactory::GetForBrowserContext(profile_)
          ->GetExtensionEventRouter();
  if (router) {
    router->OnDeterminingFilename(download, virtual_path.BaseName(),
                                  std::move(callback));
    return;
  }
#endif
  std::move(callback).Run(base::FilePath(),
                          DownloadPathReservationTracker::UNIQUIFY);
}

void ChromeDownloadManagerDelegate::ReserveVirtualPath(
    download::DownloadItem* download,
    const base::FilePath& virtual_path,
    bool create_directory,
    DownloadPathReservationTracker::FilenameConflictAction conflict_action,
    DownloadTargetDeterminerDelegate::ReservedPathCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!virtual_path.empty());

  base::FilePath document_dir;
  base::PathService::Get(chrome::DIR_USER_DOCUMENTS, &document_dir);
  DownloadPathReservationTracker::GetReservedPath(
      download, virtual_path, download_prefs_->DownloadPath(), document_dir,
      create_directory, conflict_action, std::move(callback));
}

#if BUILDFLAG(IS_ANDROID)
void ChromeDownloadManagerDelegate::RequestIncognitoWarningConfirmation(
    IncognitoWarningConfirmationCallback callback) {
  download_message_bridge_->ShowIncognitoDownloadMessage(std::move(callback));
}
#endif

void ChromeDownloadManagerDelegate::RequestConfirmation(
    DownloadItem* download,
    const base::FilePath& suggested_path,
    DownloadConfirmationReason reason,
    DownloadTargetDeterminerDelegate::ConfirmationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!download->IsTransient());

// TODO(xingliu): We should abstract a DownloadFilePicker interface and make all
// platform use it.
#if BUILDFLAG(IS_ANDROID)
  content::WebContents* web_contents =
      content::DownloadItemUtils::GetWebContents(download);
    if (reason == DownloadConfirmationReason::SAVE_AS) {
      // If this is a 'Save As' download, just run without confirmation.
      std::move(callback).Run(
          DownloadConfirmationResult::CONTINUE_WITHOUT_CONFIRMATION,
          suggested_path, absl::nullopt /*download_schedule*/);
      return;
    }

    if (!web_contents || reason == DownloadConfirmationReason::UNEXPECTED) {
      // If there are no web_contents and there are no errors (ie. location
      // dialog is only being requested because of a user preference),
      // continue.
      if (reason == DownloadConfirmationReason::PREFERENCE) {
        std::move(callback).Run(
            DownloadConfirmationResult::CONTINUE_WITHOUT_CONFIRMATION,
            suggested_path, absl::nullopt /*download_schedule*/);
        return;
      }

      if (reason == DownloadConfirmationReason::TARGET_PATH_NOT_WRITEABLE) {
        OnDownloadCanceled(download, true /* has_no_external_storage */);
        std::move(callback).Run(DownloadConfirmationResult::CANCELED,
                                base::FilePath(),
                                absl::nullopt /*download_schedule*/);
        return;
      }

      // If we cannot reserve the path and the WebContents is already gone,
      // there is no way to prompt user for a dialog. This could happen after
      // chrome gets killed, and user tries to resume a download while another
      // app has created the target file (not the temporary .crdownload file).
      OnDownloadCanceled(download, false /* has_no_external_storage */);
      std::move(callback).Run(DownloadConfirmationResult::CANCELED,
                              base::FilePath(),
                              absl::nullopt /*download_schedule*/);
      return;
    }

    if (reason == DownloadConfirmationReason::TARGET_CONFLICT) {
      // If there is a file that already has the same name, try to generate a
      // unique name for the new download (ie. "image (1).png" vs
      // "image.png").
      base::FilePath download_dir;
      if (!base::android::GetDownloadsDirectory(&download_dir)) {
        std::move(callback).Run(DownloadConfirmationResult::CANCELED,
                                base::FilePath(),
                                absl::nullopt /*download_schedule*/);
        return;
      }

      if (!download_prefs_->PromptForDownload()) {
        DuplicateDownloadDialogBridgeDelegate::GetInstance()->CreateDialog(
            download, suggested_path, web_contents, std::move(callback));
        return;
      }

      gfx::NativeWindow native_window = web_contents->GetTopLevelNativeWindow();
      DownloadPathReservationTracker::GetReservedPath(
          download, suggested_path, download_dir,
          base::FilePath() /* fallback_directory */, true,
          DownloadPathReservationTracker::UNIQUIFY,
          base::BindOnce(
              &ChromeDownloadManagerDelegate::GenerateUniqueFileNameDone,
              weak_ptr_factory_.GetWeakPtr(), native_window,
              std::move(callback)));
      return;
    }

    // Figure out type of dialog and display.
    DownloadLocationDialogType dialog_type =
        DownloadLocationDialogType::DEFAULT;

    switch (reason) {
      case DownloadConfirmationReason::TARGET_NO_SPACE:
        dialog_type = DownloadLocationDialogType::LOCATION_FULL;
        break;

      case DownloadConfirmationReason::TARGET_PATH_NOT_WRITEABLE:
        dialog_type = DownloadLocationDialogType::LOCATION_NOT_FOUND;
        break;

      case DownloadConfirmationReason::NAME_TOO_LONG:
        dialog_type = DownloadLocationDialogType::NAME_TOO_LONG;
        break;

      case DownloadConfirmationReason::PREFERENCE:
      default:
        break;
    }

    gfx::NativeWindow native_window = web_contents->GetTopLevelNativeWindow();
    ShowDownloadDialog(
        native_window, download->GetTotalBytes(), dialog_type, suggested_path,
        base::BindOnce(&OnDownloadDialogClosed, std::move(callback)));
    return;

#else   // BUILDFLAG(IS_ANDROID)
  // Desktop Chrome displays a file picker for all confirmation needs. We can do
  // better.
  if (is_file_picker_showing_) {
    file_picker_callbacks_.emplace_back(
        base::BindOnce(&ChromeDownloadManagerDelegate::ShowFilePicker,
                       weak_ptr_factory_.GetWeakPtr(), download->GetGuid(),
                       suggested_path, std::move(callback)));
  } else {
    is_file_picker_showing_ = true;
    ShowFilePicker(download->GetGuid(), suggested_path, std::move(callback));
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

void ChromeDownloadManagerDelegate::OnConfirmationCallbackComplete(
    DownloadTargetDeterminerDelegate::ConfirmationCallback callback,
    DownloadConfirmationResult result,
    const base::FilePath& virtual_path) {
  std::move(callback).Run(result, virtual_path,
                          absl::nullopt /*download_schedule*/);
  if (!file_picker_callbacks_.empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, std::move(file_picker_callbacks_.front()));
    file_picker_callbacks_.pop_front();
  } else {
    is_file_picker_showing_ = false;
  }
}

void ChromeDownloadManagerDelegate::ShowFilePicker(
    const std::string& guid,
    const base::FilePath& suggested_path,
    DownloadTargetDeterminerDelegate::ConfirmationCallback callback) {
  DownloadItem* download = download_manager_->GetDownloadByGuid(guid);
  if (download) {
    ShowFilePickerForDownload(download, suggested_path, std::move(callback));
  } else {
    OnConfirmationCallbackComplete(std::move(callback),
                                   DownloadConfirmationResult::CANCELED,
                                   base::FilePath());
  }
}

void ChromeDownloadManagerDelegate::ShowFilePickerForDownload(
    DownloadItem* download,
    const base::FilePath& suggested_path,
    DownloadTargetDeterminerDelegate::ConfirmationCallback callback) {
  DCHECK(download);
  DownloadFilePicker::ShowFilePicker(
      download, suggested_path,
      base::BindOnce(
          &ChromeDownloadManagerDelegate::OnConfirmationCallbackComplete,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

#if BUILDFLAG(IS_ANDROID)
void ChromeDownloadManagerDelegate::GenerateUniqueFileNameDone(
    gfx::NativeWindow native_window,
    DownloadTargetDeterminerDelegate::ConfirmationCallback callback,
    PathValidationResult result,
    const base::FilePath& target_path) {
  // After a new, unique filename has been generated, display the error dialog
  // with the filename automatically set to be the unique filename.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (result == PathValidationResult::SUCCESS) {
    if (download_prefs_->PromptForDownload()) {
      ShowDownloadDialog(
          native_window, 0 /* total_bytes */,
          DownloadLocationDialogType::NAME_CONFLICT, target_path,
          base::BindOnce(&OnDownloadDialogClosed, std::move(callback)));
      return;
    }

    // If user chose not to show download location dialog, uses current unique
    // target path.
    std::move(callback).Run(
        DownloadConfirmationResult::CONTINUE_WITHOUT_CONFIRMATION, target_path,
        absl::nullopt /*download_schedule*/);
  } else {
    // If the name generation failed, fail the download.
    std::move(callback).Run(DownloadConfirmationResult::FAILED,
                            base::FilePath(),
                            absl::nullopt /*download_schedule*/);
  }
}

void ChromeDownloadManagerDelegate::OnDownloadCanceled(
    download::DownloadItem* download,
    bool has_no_external_storage) {
  DownloadManagerService::OnDownloadCanceled(download, has_no_external_storage);
}
#endif  // BUILDFLAG(IS_ANDROID)

void ChromeDownloadManagerDelegate::DetermineLocalPath(
    DownloadItem* download,
    const base::FilePath& virtual_path,
    download::LocalPathCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  download::DetermineLocalPath(download, virtual_path, std::move(callback));
}

void ChromeDownloadManagerDelegate::CheckDownloadUrl(
    DownloadItem* download,
    const base::FilePath& suggested_path,
    CheckDownloadUrlCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if BUILDFLAG(FULL_SAFE_BROWSING)
  safe_browsing::DownloadProtectionService* service =
      GetDownloadProtectionService();
  if (service) {
    bool is_content_check_supported =
        service->IsSupportedDownload(*download, suggested_path);
    DVLOG(2) << __func__ << "() Start SB URL check for download = "
             << download->DebugString(false);
    if (service->ShouldCheckDownloadUrl(download)) {
      service->CheckDownloadUrl(
          download,
          base::BindOnce(&CheckDownloadUrlDone, std::move(callback),
                         download->GetUrlChain(), is_content_check_supported));
      return;
    }
  }
#endif
  std::move(callback).Run(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
}

void ChromeDownloadManagerDelegate::GetFileMimeType(
    const base::FilePath& path,
    GetFileMimeTypeCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&GetMimeType, path),
      std::move(callback));
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
void ChromeDownloadManagerDelegate::CheckClientDownloadDone(
    uint32_t download_id,
    safe_browsing::DownloadCheckResult result) {
  if (!download_manager_)
    return;
  DownloadItem* item = download_manager_->GetDownload(download_id);
  if (!item || (item->GetState() != DownloadItem::IN_PROGRESS &&
                item->GetDangerType() !=
                    download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING)) {
    return;
  }

  DVLOG(2) << __func__ << "() download = " << item->DebugString(false)
           << " verdict = " << static_cast<int>(result);

  // Indicates whether we expect future verdicts on this download. For example,
  // if Safe Browsing is performing deep scanning, we will receive a more
  // specific verdict later.
  bool is_pending_scanning = false;

  // We only mark the content as being dangerous if the download's safety state
  // has not been set to DANGEROUS yet.  We don't want to show two warnings.
  if (item->GetDangerType() == download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS ||
      item->GetDangerType() ==
          download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT ||
      item->GetDangerType() == download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING ||
      item->GetDangerType() ==
          download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING) {
    download::DownloadDangerType danger_type =
        download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;
    switch (result) {
      case safe_browsing::DownloadCheckResult::UNKNOWN:
      case safe_browsing::DownloadCheckResult::SAFE:
        // For DANGEROUS file types, we still want to warng the user, even if
        // Safe Browsing is unsure about the file.
        if (DownloadItemModel(item).GetDangerLevel() ==
            DownloadFileType::DANGEROUS) {
          danger_type = download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE;
        }
        break;
      case safe_browsing::DownloadCheckResult::DANGEROUS:
        danger_type = download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT;
        break;
      case safe_browsing::DownloadCheckResult::UNCOMMON:
        danger_type = download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT;
        break;
      case safe_browsing::DownloadCheckResult::DANGEROUS_HOST:
        danger_type = download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST;
        break;
      case safe_browsing::DownloadCheckResult::POTENTIALLY_UNWANTED:
        danger_type = download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED;
        break;
      case safe_browsing::DownloadCheckResult::ALLOWLISTED_BY_POLICY:
        danger_type = download::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY;
        break;
      case safe_browsing::DownloadCheckResult::ASYNC_SCANNING:
        is_pending_scanning = true;
        danger_type = download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING;
        break;
      case safe_browsing::DownloadCheckResult::BLOCKED_PASSWORD_PROTECTED:
        danger_type = download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED;
        break;
      case safe_browsing::DownloadCheckResult::BLOCKED_TOO_LARGE:
        danger_type = download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE;
        break;
      case safe_browsing::DownloadCheckResult::SENSITIVE_CONTENT_WARNING:
        danger_type = download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING;
        break;
      case safe_browsing::DownloadCheckResult::SENSITIVE_CONTENT_BLOCK:
        danger_type = download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK;
        break;
      case safe_browsing::DownloadCheckResult::DEEP_SCANNED_SAFE:
        danger_type = download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE;
        break;
      case safe_browsing::DownloadCheckResult::PROMPT_FOR_SCANNING:
        danger_type = download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING;
        is_pending_scanning = true;
        break;
      case safe_browsing::DownloadCheckResult::BLOCKED_UNSUPPORTED_FILE_TYPE:
        danger_type =
            download::DOWNLOAD_DANGER_TYPE_BLOCKED_UNSUPPORTED_FILETYPE;
        break;
      case safe_browsing::DownloadCheckResult::DANGEROUS_ACCOUNT_COMPROMISE:
        danger_type =
            download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE;
        break;
    }
    DCHECK_NE(danger_type,
              download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT);

    if (item->GetState() == DownloadItem::COMPLETE &&
        item->GetDangerType() ==
            download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING) {
      // If the file was opened during async scanning, we override the danger
      // type, since the user can no longer discard the download.
      if (danger_type != download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS) {
        item->OnAsyncScanningCompleted(
            download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS);

        // Because the file has been opened before the verdict was available,
        // the reporter must be manually notified that it needs to record the
        // bypass. This is because the bypass wasn't reported on open to avoid
        // sending a bypass event for a non-dangerous/sensitive file.
        GetDownloadProtectionService()->ReportDelayedBypassEvent(item,
                                                                 danger_type);
      } else {
        item->OnAsyncScanningCompleted(danger_type);
      }
    } else if (ShouldBlockFile(item, danger_type)) {
      // Specifying a dangerous type here would take precedence over the
      // blocking of the file. For BLOCKED_TOO_LARGE and
      // BLOCKED_PASSWORD_PROTECTED, we want to display more clear UX, so
      // allow those danger types.
      if (!IsDangerTypeBlocked(danger_type)) {
        danger_type = download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;
        MaybeReportDangerousDownloadBlocked(
            download_prefs_->download_restriction(), "DANGEROUS_FILE_TYPE",
            item->GetTargetFilePath().AsUTF8Unsafe(), item);
      }
      item->OnContentCheckCompleted(
          danger_type, download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED);
    } else {
      item->OnContentCheckCompleted(danger_type,
                                    download::DOWNLOAD_INTERRUPT_REASON_NONE);
    }
  }

  if (!is_pending_scanning) {
    SafeBrowsingState* state = static_cast<SafeBrowsingState*>(
        item->GetUserData(&SafeBrowsingState::kSafeBrowsingUserDataKey));
    state->CompleteDownload();
  }
}

void ChromeDownloadManagerDelegate::CheckSavePackageScanningDone(
    uint32_t download_id,
    safe_browsing::DownloadCheckResult result) {
  if (!download_manager_)
    return;
  DownloadItem* item = download_manager_->GetDownload(download_id);
  if (!item || (item->GetState() != DownloadItem::IN_PROGRESS &&
                item->GetDangerType() !=
                    download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING)) {
    return;
  }

  // We only mark the content as being sensitive if the download's danger state
  // has not been set yet.  We don't want to show two warnings.
  if (item->GetDangerType() == download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS ||
      item->GetDangerType() ==
          download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT ||
      item->GetDangerType() == download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING ||
      item->GetDangerType() ==
          download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING) {
    download::DownloadDangerType danger_type = SavePackageDangerType(result);
    if (item->GetState() == DownloadItem::COMPLETE &&
        item->GetDangerType() ==
            download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING) {
      // If the save package was opened during async scanning, we override the
      // danger type, since the user can no longer discard the download.
      if (danger_type != download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS) {
        item->OnAsyncScanningCompleted(
            download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS);

        // Because the file has been opened before the verdict was available,
        // the reporter must be manually notified that it needs to record the
        // bypass. This is because the bypass wasn't reported on open to avoid
        // sending a bypass event for a non-dangerous/sensitive file.
        GetDownloadProtectionService()->ReportDelayedBypassEvent(item,
                                                                 danger_type);
      } else {
        item->OnAsyncScanningCompleted(danger_type);
      }
    } else if (IsDangerTypeBlocked(danger_type)) {
      item->OnContentCheckCompleted(
          danger_type, download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED);
    } else {
      item->OnContentCheckCompleted(danger_type,
                                    download::DOWNLOAD_INTERRUPT_REASON_NONE);
    }
  }

  // RunSavePackageScanningCallback is called after OnAsyncScanningCompleted or
  // OnContentCheckCompleted so that the package completes correctly after a
  // scanning-specific UI has been applie to `item`.
  switch (result) {
    // These results imply the scanning is either not done or that the Save
    // Package being allowed/blocked depends on user action following a
    // warning, so the callback doesn't need to run.
    case safe_browsing::DownloadCheckResult::ASYNC_SCANNING:
    case safe_browsing::DownloadCheckResult::SENSITIVE_CONTENT_WARNING:
      break;

    case safe_browsing::DownloadCheckResult::UNKNOWN:
    case safe_browsing::DownloadCheckResult::DEEP_SCANNED_SAFE:
      enterprise_connectors::RunSavePackageScanningCallback(item,
                                                            /*allowed*/ true);
      break;

    case safe_browsing::DownloadCheckResult::BLOCKED_UNSUPPORTED_FILE_TYPE:
    case safe_browsing::DownloadCheckResult::BLOCKED_PASSWORD_PROTECTED:
    case safe_browsing::DownloadCheckResult::BLOCKED_TOO_LARGE:
    case safe_browsing::DownloadCheckResult::SENSITIVE_CONTENT_BLOCK:
      enterprise_connectors::RunSavePackageScanningCallback(item,
                                                            /*allowed*/ false);
      break;

    default:
      // These other results should never be returned, but if they are somehow
      // then scanning policies are fail-open, so the save package should be
      // allowed to complete.
      NOTREACHED();
      enterprise_connectors::RunSavePackageScanningCallback(item,
                                                            /*allowed*/ true);
      break;
  }

}
#endif  // FULL_SAFE_BROWSING

// content::NotificationObserver implementation.
void ChromeDownloadManagerDelegate::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  DCHECK_EQ(extensions::NOTIFICATION_CRX_INSTALLER_DONE, type);

  registrar_.Remove(this, extensions::NOTIFICATION_CRX_INSTALLER_DONE, source);

  scoped_refptr<extensions::CrxInstaller> installer =
      content::Source<extensions::CrxInstaller>(source).ptr();
  content::DownloadOpenDelayedCallback callback =
      std::move(crx_installers_[installer.get()]);
  crx_installers_.erase(installer.get());
  std::move(callback).Run(installer->did_handle_successfully());
#endif
}

void ChromeDownloadManagerDelegate::OnDownloadTargetDetermined(
    uint32_t download_id,
    content::DownloadTargetCallback callback,
    std::unique_ptr<DownloadTargetInfo> target_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DownloadItem* item = download_manager_->GetDownload(download_id);
  if (item) {
    if (!target_info->target_path.empty() &&
        IsOpenInBrowserPreferreredForFile(target_info->target_path) &&
        target_info->is_filetype_handled_safely)
      DownloadItemModel(item).SetShouldPreferOpeningInBrowser(true);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    if (item->GetOriginalMimeType() == "application/x-x509-user-cert")
      DownloadItemModel(item).SetShouldPreferOpeningInBrowser(true);
#endif

    DownloadItemModel(item).SetDangerLevel(target_info->danger_level);
  }
  if (ShouldBlockFile(item, target_info->danger_type)) {
    MaybeReportDangerousDownloadBlocked(
        download_prefs_->download_restriction(), "DANGEROUS_FILE_TYPE",
        target_info->target_path.AsUTF8Unsafe(), item);
    target_info->result = download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED;
    // A dangerous type would take precedence over the blocking of the file.
    target_info->danger_type = download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;
  }

  base::FilePath target_path = target_info->target_path;

#if BUILDFLAG(IS_ANDROID)
  // Present a mixed content download infobar when needed, and wait to initiate
  // the download until the user decides what to do.
  // On Desktop, this is handled using the unsafe-download warnings that are
  // shown in parallel with the download. Those warnings don't exist for
  // Android, so for simplicity we prompt before starting the download instead.
  auto mcs = target_info->mixed_content_status;
  if (target_info->result == download::DOWNLOAD_INTERRUPT_REASON_NONE &&
      (mcs == download::DownloadItem::MixedContentStatus::BLOCK ||
       mcs == download::DownloadItem::MixedContentStatus::WARN)) {
    auto* web_contents = content::DownloadItemUtils::GetWebContents(item);
    gfx::NativeWindow native_window =
        web_contents ? web_contents->GetTopLevelNativeWindow() : nullptr;
    if (native_window && item) {
      MixedContentDownloadDialogBridge::GetInstance()->CreateDialog(
          item, target_path.BaseName(), native_window,
          base::BindOnce(HandleMixedDownloadInfoBarResult, item,
                         std::move(target_info), std::move(callback)));
      return;
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)

  // A separate reservation with the same target path may exist.
  // If so, cancel the current reservation.
  DownloadPathReservationTracker::CheckDownloadPathForExistingDownload(
      target_path, item,
      base::BindOnce(&OnCheckExistingDownloadPathDone, std::move(target_info),
                     std::move(callback)));
}

bool ChromeDownloadManagerDelegate::IsOpenInBrowserPreferreredForFile(
    const base::FilePath& path) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
  if (path.MatchesExtension(FILE_PATH_LITERAL(".pdf"))) {
    return !download_prefs_->ShouldOpenPdfInSystemReader();
  }
#endif

  // On Android, always prefer opening with an external app. On ChromeOS, there
  // are no external apps so just allow all opens to be handled by the "System."
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH) && \
    BUILDFLAG(ENABLE_PLUGINS)
  // TODO(asanka): Consider other file types and MIME types.
  // http://crbug.com/323561
  if (path.MatchesExtension(FILE_PATH_LITERAL(".pdf")) ||
      path.MatchesExtension(FILE_PATH_LITERAL(".htm")) ||
      path.MatchesExtension(FILE_PATH_LITERAL(".html")) ||
      path.MatchesExtension(FILE_PATH_LITERAL(".shtm")) ||
      path.MatchesExtension(FILE_PATH_LITERAL(".shtml")) ||
      path.MatchesExtension(FILE_PATH_LITERAL(".svg")) ||
      path.MatchesExtension(FILE_PATH_LITERAL(".xht")) ||
      path.MatchesExtension(FILE_PATH_LITERAL(".xhtm")) ||
      path.MatchesExtension(FILE_PATH_LITERAL(".xhtml")) ||
      path.MatchesExtension(FILE_PATH_LITERAL(".xsl")) ||
      path.MatchesExtension(FILE_PATH_LITERAL(".xslt"))) {
    return true;
  }
#endif
  return false;
}

bool ChromeDownloadManagerDelegate::ShouldBlockFile(
    download::DownloadItem* item,
    download::DownloadDangerType danger_type) const {
  DownloadPrefs::DownloadRestriction download_restriction =
      download_prefs_->download_restriction();

  if (IsDangerTypeBlocked(danger_type))
    return true;

  // TODO(crbug/1061111): Move this into IsDangerTypeBlocked once the UX is
  // ready.
  if (danger_type ==
      download::DOWNLOAD_DANGER_TYPE_BLOCKED_UNSUPPORTED_FILETYPE)
    return true;

  bool file_type_dangerous =
      (item &&
       DownloadItemModel(item).GetDangerLevel() !=
           DownloadFileType::NOT_DANGEROUS &&
       danger_type == download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  switch (download_restriction) {
    case (DownloadPrefs::DownloadRestriction::NONE):
      return false;

    case (DownloadPrefs::DownloadRestriction::POTENTIALLY_DANGEROUS_FILES):
      return danger_type != download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS ||
             file_type_dangerous;

    case (DownloadPrefs::DownloadRestriction::DANGEROUS_FILES): {
      return (danger_type == download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT ||
              danger_type == download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE ||
              danger_type == download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL ||
              danger_type ==
                  download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE ||
              file_type_dangerous);
    }

    case (DownloadPrefs::DownloadRestriction::MALICIOUS_FILES): {
      return (danger_type == download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT ||
              danger_type == download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST ||
              danger_type == download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL ||
              danger_type ==
                  download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE);
    }

    case (DownloadPrefs::DownloadRestriction::ALL_FILES):
      return true;

    default:
      LOG(ERROR) << "Invalid download restruction value: "
                 << static_cast<int>(download_restriction);
  }

  return false;
}

void ChromeDownloadManagerDelegate::MaybeSendDangerousDownloadOpenedReport(
    DownloadItem* download,
    bool show_download_in_folder) {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  safe_browsing::DownloadProtectionService* service =
      GetDownloadProtectionService();
  if (service) {
    service->MaybeSendDangerousDownloadOpenedReport(download,
                                                    show_download_in_folder);
  }
#endif
  if (!download->GetAutoOpened()) {
    download::DownloadContent download_content =
        download::DownloadContentFromMimeType(download->GetMimeType(), false);
    safe_browsing::RecordDownloadOpened(
        download->GetDangerType(), download_content, base::Time::Now(),
        download->GetEndTime(), show_download_in_folder);
  }
}

void ChromeDownloadManagerDelegate::CheckDownloadAllowed(
    const content::WebContents::Getter& web_contents_getter,
    const GURL& url,
    const std::string& request_method,
    absl::optional<url::Origin> request_initiator,
    bool from_download_cross_origin_redirect,
    bool content_initiated,
    content::CheckDownloadAllowedCallback check_download_allowed_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
  // Don't download pdf if it is a file URL, as that might cause an infinite
  // download loop if Chrome is not the system pdf viewer.
  if (url.SchemeIsFile() && download_prefs_->ShouldOpenPdfInSystemReader()) {
    base::FilePath path;
    net::FileURLToFilePath(url, &path);
    base::FilePath::StringType extension = path.Extension();
    if (!extension.empty() && base::FilePath::CompareEqualIgnoreCase(
                                  extension, FILE_PATH_LITERAL(".pdf"))) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(check_download_allowed_cb), false));
      return;
    }
  }
#endif
  CanDownloadCallback cb = base::BindOnce(
      &ChromeDownloadManagerDelegate::OnCheckDownloadAllowedComplete,
      weak_ptr_factory_.GetWeakPtr(), std::move(check_download_allowed_cb));
#if BUILDFLAG(IS_ANDROID)
  DownloadControllerBase::Get()->AcquireFileAccessPermission(
      web_contents_getter,
      base::BindOnce(&OnDownloadAcquireFileAccessPermissionDone,
                     web_contents_getter, url, request_method,
                     std::move(request_initiator), std::move(cb)));
#else
  CheckCanDownload(web_contents_getter, url, request_method,
                   std::move(request_initiator),
                   from_download_cross_origin_redirect, std::move(cb));
#endif
}

download::QuarantineConnectionCallback
ChromeDownloadManagerDelegate::GetQuarantineConnectionCallback() {
  return base::BindRepeating(
      &ChromeDownloadManagerDelegate::ConnectToQuarantineService);
}

std::unique_ptr<download::DownloadItemRenameHandler>
ChromeDownloadManagerDelegate::GetRenameHandlerForDownload(
    download::DownloadItem* download_item) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
  return enterprise_connectors::FileSystemRenameHandler::CreateIfNeeded(
      download_item);
#else
  return nullptr;
#endif
}

void ChromeDownloadManagerDelegate::CheckSavePackageAllowed(
    download::DownloadItem* download_item,
    base::flat_map<base::FilePath, base::FilePath> save_package_files,
    content::SavePackageAllowedCallback callback) {
  DCHECK(download_item);
  DCHECK(download_item->IsSavePackageDownload());

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
  absl::optional<enterprise_connectors::AnalysisSettings> settings =
      safe_browsing::DeepScanningRequest::ShouldUploadBinary(download_item);

  if (settings.has_value()) {
    DownloadProtectionService* service = GetDownloadProtectionService();
    // Save package never need malware scans, so exempt them from scanning if
    // there are no other tags.
    settings->tags.erase("malware");
    if (!settings->tags.empty() && service) {
      download_item->SetUserData(
          enterprise_connectors::SavePackageScanningData::kKey,
          std::make_unique<enterprise_connectors::SavePackageScanningData>(
              std::move(callback)));

      service->UploadSavePackageForDeepScanning(
          download_item, std::move(save_package_files),
          base::BindRepeating(
              &ChromeDownloadManagerDelegate::CheckSavePackageScanningDone,
              weak_ptr_factory_.GetWeakPtr(), download_item->GetId()),
          std::move(settings.value()));
      return;
    }
  }
#endif
  std::move(callback).Run(true);
}

void ChromeDownloadManagerDelegate::OnCheckDownloadAllowedComplete(
    content::CheckDownloadAllowedCallback check_download_allowed_cb,
    bool storage_permission_granted,
    bool allow) {
  if (!storage_permission_granted) {
    // UMA for this will be recorded in MobileDownload.StoragePermission.
  } else if (allow) {
    // Presumes all downloads initiated by navigation use this throttle and
    // nothing else does.
    RecordDownloadSource(DOWNLOAD_INITIATED_BY_NAVIGATION);
  } else {
    RecordDownloadCount(CHROME_DOWNLOAD_COUNT_BLOCKED_BY_THROTTLING);
  }

  std::move(check_download_allowed_cb).Run(allow);
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
ChromeDownloadManagerDelegate::SafeBrowsingState::~SafeBrowsingState() {}

const char ChromeDownloadManagerDelegate::SafeBrowsingState::
    kSafeBrowsingUserDataKey[] = "Safe Browsing ID";
#endif  // FULL_SAFE_BROWSING

base::WeakPtr<ChromeDownloadManagerDelegate>
ChromeDownloadManagerDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

// static
void ChromeDownloadManagerDelegate::ConnectToQuarantineService(
    mojo::PendingReceiver<quarantine::mojom::Quarantine> receiver) {
#if BUILDFLAG(IS_WIN)
  content::ServiceProcessHost::Launch(std::move(receiver),
                                      content::ServiceProcessHost::Options()
                                          .WithDisplayName("Quarantine Service")
                                          .Pass());
#else   // !BUILDFLAG(IS_WIN)
  mojo::MakeSelfOwnedReceiver(std::make_unique<quarantine::QuarantineImpl>(),
                              std::move(receiver));
#endif  // !BUILDFLAG(IS_WIN)
}

void ChromeDownloadManagerDelegate::OnManagerInitialized() {
#if !BUILDFLAG(IS_ANDROID)
  CancelAllEphemeralWarnings();
#endif
}

#if !BUILDFLAG(IS_ANDROID)
void ChromeDownloadManagerDelegate::ScheduleCancelForEphemeralWarning(
    const std::string& guid) {
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ChromeDownloadManagerDelegate::CancelForEphemeralWarning,
                     weak_ptr_factory_.GetWeakPtr(), guid),
      kEphemeralWarningLifetimeBeforeCancel);
}

void ChromeDownloadManagerDelegate::CancelForEphemeralWarning(
    const std::string& guid) {
  download::DownloadItem* download = download_manager_->GetDownloadByGuid(guid);

  if (!download) {
    // The download may have been destroyed since the task was scheduled
    return;
  }

  // Confirm that the user has not already acted on the warning.
  if (std::make_unique<DownloadItemModel>(download)->IsEphemeralWarning()) {
    download->Cancel(/*user_cancel=*/false);
  }
}

void ChromeDownloadManagerDelegate::CancelAllEphemeralWarnings() {
  content::DownloadManager::DownloadVector downloads;
  download_manager_->GetAllDownloads(&downloads);
  for (auto* download : downloads) {
    auto model = std::make_unique<DownloadItemModel>(download);
    if (model->IsEphemeralWarning() &&
        model->GetState() != download::DownloadItem::CANCELLED) {
      download->Cancel(/*user_cancel=*/false);
    }
  }
}
#endif  // !BUILDFLAG(IS_ANDROID)
