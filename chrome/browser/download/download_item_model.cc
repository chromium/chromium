// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_item_model.h"

#include <string>

#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/download/download_target_determiner.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_item_rename_handler.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "components/safe_browsing/content/common/proto/download_file_types.pb.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "ui/views/vector_icons.h"
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/safe_browsing/download_protection/deep_scanning_request.h"
#include "chrome/browser/safe_browsing/download_protection/download_feedback_service.h"
#include "chrome/browser/enterprise/connectors/connectors_manager.h"
#endif

using DangerUiPattern = DownloadUIModel::DangerUiPattern;
using download::DownloadItem;
using InsecureDownloadStatus = download::DownloadItem::InsecureDownloadStatus;
using safe_browsing::DownloadFileType;
using ReportThreatDetailsResult =
    safe_browsing::PingManager::ReportThreatDetailsResult;
using TailoredVerdict = safe_browsing::ClientDownloadResponse::TailoredVerdict;
using TailoredWarningType = DownloadUIModel::TailoredWarningType;

namespace {

// Per DownloadItem data used by DownloadItemModel. The model doesn't keep any
// state since there could be multiple models associated with a single
// DownloadItem, and the lifetime of the model is shorter than the DownloadItem.
class DownloadItemModelData : public base::SupportsUserData::Data {
 public:
  ~DownloadItemModelData() override {}

  // Get the DownloadItemModelData object for |download|. Returns NULL if
  // there's no model data.
  static const DownloadItemModelData* Get(const DownloadItem* download);

  // Get the DownloadItemModelData object for |download|. Creates a model data
  // object if not found. Always returns a non-NULL pointer, unless OOM.
  static DownloadItemModelData* GetOrCreate(DownloadItem* download);

  // Whether the download should be displayed in the download shelf. True by
  // default.
  bool should_show_in_shelf_ = true;

  // Whether the UI has been notified about this download.
  bool was_ui_notified_ = false;

  // Whether the download should be opened in the browser vs. the system handler
  // for the file type.
  std::optional<bool> should_prefer_opening_in_browser_;

  // Danger level of the file determined based on the file type and whether
  // there was a user action associated with the download.
  DownloadFileType::DangerLevel danger_level_ = DownloadFileType::NOT_DANGEROUS;

  // Whether the download is currently being revived.
  bool is_being_revived_ = false;

  // Whether the safe browsing download warning was shown (and recorded) earlier
  // on the UI.
  bool was_ui_warning_shown_ = false;

  // Tracks when an ephemeral warning was first displayed on the UI. Does not
  // persist on restart, though ephemeral warning downloads are canceled by
  // then as all in-progress downloads are.
  std::optional<base::Time> ephemeral_warning_ui_shown_time_;

  // Was the UI actioned on. This defaults to true so that we don't show
  // extraneous items in the partial view the first time the bubble pops up
  // after a browser restart.
  bool actioned_on_ = true;

 private:
  DownloadItemModelData();

  static const char kKey[];
};

// static
const char DownloadItemModelData::kKey[] = "DownloadItemModelData key";

// static
const DownloadItemModelData* DownloadItemModelData::Get(
    const DownloadItem* download) {
  return static_cast<const DownloadItemModelData*>(download->GetUserData(kKey));
}

// static
DownloadItemModelData* DownloadItemModelData::GetOrCreate(
    DownloadItem* download) {
  DownloadItemModelData* data =
      static_cast<DownloadItemModelData*>(download->GetUserData(kKey));
  if (data == nullptr) {
    data = new DownloadItemModelData();
    data->should_show_in_shelf_ = !download->IsTransient();
    download->SetUserData(kKey, base::WrapUnique(data));
  }
  return data;
}

DownloadItemModelData::DownloadItemModelData() = default;

#if BUILDFLAG(FULL_SAFE_BROWSING)
void MaybeSendDownloadReport(bool did_proceed,
                             download::DownloadItem* download) {
  if (safe_browsing::SafeBrowsingService* sb_service =
          g_browser_process->safe_browsing_service()) {
    sb_service->SendDownloadReport(
        download,
        safe_browsing::ClientSafeBrowsingReportRequest::
            DANGEROUS_DOWNLOAD_WARNING,
        did_proceed, /*show_download_in_folder=*/std::nullopt);
  }
}

#endif

}  // namespace

// -----------------------------------------------------------------------------
// DownloadItemModel

// static
DownloadUIModel::DownloadUIModelPtr DownloadItemModel::Wrap(
    download::DownloadItem* download) {
  return std::make_unique<DownloadItemModel>(download);
}

// static
DownloadUIModel::DownloadUIModelPtr DownloadItemModel::Wrap(
    download::DownloadItem* download,
    std::unique_ptr<DownloadUIModel::StatusTextBuilderBase>
        status_text_builder) {
  return std::make_unique<DownloadItemModel>(download,
                                             std::move(status_text_builder));
}

DownloadItemModel::DownloadItemModel(DownloadItem* download)
    : DownloadItemModel(download, std::make_unique<StatusTextBuilder>()) {}

DownloadItemModel::DownloadItemModel(
    download::DownloadItem* download,
    std::unique_ptr<DownloadUIModel::StatusTextBuilderBase> status_text_builder)
    : DownloadUIModel(std::move(status_text_builder)), download_(download) {
  download_->AddObserver(this);
}

DownloadItemModel::~DownloadItemModel() {
  if (download_)
    download_->RemoveObserver(this);
}

ContentId DownloadItemModel::GetContentId() const {
  return OfflineItemUtils::GetContentIdForDownload(download_);
}

Profile* DownloadItemModel::profile() const {
  return Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(download_));
}

std::u16string DownloadItemModel::GetTabProgressStatusText() const {
  int64_t total = GetTotalBytes();
  int64_t size;
  auto* renamer = download_->GetRenameHandler();
  if (renamer && renamer->ShowRenameProgress()) {
    size = static_cast<int>(
        (download_->GetReceivedBytes() + download_->GetUploadedBytes()) * 0.5);
  } else {
    size = download_->GetReceivedBytes();
  }
  std::u16string received_size = ui::FormatBytes(size);
  std::u16string amount = received_size;

  // Adjust both strings for the locale direction since we don't yet know which
  // string we'll end up using for constructing the final progress string.
  base::i18n::AdjustStringForLocaleDirection(&amount);

  if (total) {
    std::u16string total_text = ui::FormatBytes(total);
    base::i18n::AdjustStringForLocaleDirection(&total_text);

    base::i18n::AdjustStringForLocaleDirection(&received_size);
    amount = l10n_util::GetStringFUTF16(IDS_DOWNLOAD_TAB_PROGRESS_SIZE,
                                        received_size, total_text);
  } else {
    amount.assign(received_size);
  }
  int64_t current_speed = download_->CurrentSpeed();
  std::u16string speed_text = ui::FormatSpeed(current_speed);
  base::i18n::AdjustStringForLocaleDirection(&speed_text);

  base::TimeDelta remaining;
  std::u16string time_remaining;
  if (download_->IsPaused()) {
    time_remaining = l10n_util::GetStringUTF16(IDS_DOWNLOAD_PROGRESS_PAUSED);
  } else if (download_->TimeRemaining(&remaining)) {
    time_remaining =
        ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_REMAINING,
                               ui::TimeFormat::LENGTH_SHORT, remaining);
  }

  if (time_remaining.empty()) {
    base::i18n::AdjustStringForLocaleDirection(&amount);
    return l10n_util::GetStringFUTF16(
        IDS_DOWNLOAD_TAB_PROGRESS_STATUS_TIME_UNKNOWN, speed_text, amount);
  }
  return l10n_util::GetStringFUTF16(IDS_DOWNLOAD_TAB_PROGRESS_STATUS,
                                    speed_text, amount, time_remaining);
}

int64_t DownloadItemModel::GetCompletedBytes() const {
  auto* renamer = download_->GetRenameHandler();
  if (renamer && renamer->ShowRenameProgress()) {
    return static_cast<int>(
        (download_->GetReceivedBytes() + download_->GetUploadedBytes()) * 0.5);
  }
  return download_->GetReceivedBytes();
}

int64_t DownloadItemModel::GetTotalBytes() const {
  return download_->AllDataSaved() ? download_->GetReceivedBytes()
                                   : download_->GetTotalBytes();
}

// TODO(asanka,rdsmith): Once 'open' moves exclusively to the
//     ChromeDownloadManagerDelegate, we should calculate the percentage here
//     instead of calling into the DownloadItem.
int DownloadItemModel::PercentComplete() const {
  auto* renamer = download_->GetRenameHandler();
  if (renamer && renamer->ShowRenameProgress()) {
    return static_cast<int>(
        ((download_->GetReceivedBytes() + download_->GetUploadedBytes()) * 0.5 *
         100.0) /
        GetTotalBytes());
  }
  return download_->PercentComplete();
}

bool DownloadItemModel::IsDangerous() const {
  return download_->IsDangerous();
}

bool DownloadItemModel::MightBeMalicious() const {
  return IsDangerous() && (download_->GetDangerType() !=
                           download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE);
}

// If you change this definition of malicious, also update
// DownloadManagerImpl::BlockingShutdownCount.
bool DownloadItemModel::IsMalicious() const {
  if (!MightBeMalicious())
    return false;
  switch (download_->GetDangerType()) {
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
      return true;

    case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case download::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY:
    case download::DOWNLOAD_DANGER_TYPE_MAX:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      // We shouldn't get any of these due to the MightBeMalicious() test above.
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED:
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_SCAN_FAILED:
      return false;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool DownloadItemModel::IsInsecure() const {
  return download_->IsInsecure();
}

bool DownloadItemModel::ShouldRemoveFromShelfWhenComplete() const {
  switch (download_->GetState()) {
    case DownloadItem::IN_PROGRESS:
      // If the download is dangerous or malicious, we should display a warning
      // on the shelf until the user accepts the download.
      if (IsDangerous())
        return false;

      // If the download is a trusted extension, temporary, or will be opened
      // automatically, then it should be removed from the shelf on completion.
      // TODO(crbug.com/40129365): The logic for deciding opening behavior
      // should
      //                          be in a central location.
      return (download_crx_util::IsTrustedExtensionDownload(profile(),
                                                            *download_) ||
              download_->IsTemporary() || download_->GetOpenWhenComplete() ||
              download_->ShouldOpenFileBasedOnExtension());

    case DownloadItem::COMPLETE:
      // If the download completed, then rely on GetAutoOpened() to check for
      // opening behavior. This should accurately reflect whether the download
      // was successfully opened.  Extensions, for example, may fail to open.
      return download_->GetAutoOpened() || download_->IsTemporary();

    case DownloadItem::CANCELLED:
    case DownloadItem::INTERRUPTED:
      // Interrupted or cancelled downloads should remain on the shelf.
      return false;

    case DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED_IN_MIGRATION();
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

bool DownloadItemModel::ShouldShowDownloadStartedAnimation() const {
  return !download_->IsSavePackageDownload() &&
         !download_crx_util::IsTrustedExtensionDownload(profile(), *download_);
}

bool DownloadItemModel::ShouldShowInShelf() const {
  const DownloadItemModelData* data = DownloadItemModelData::Get(download_);
  if (data)
    return data->should_show_in_shelf_;

  return !download_->IsTransient();
}

void DownloadItemModel::SetShouldShowInShelf(bool should_show) {
  DownloadItemModelData* data = DownloadItemModelData::GetOrCreate(download_);
  data->should_show_in_shelf_ = should_show;
}

bool DownloadItemModel::ShouldNotifyUI() const {
  if (download_->IsTransient())
    return false;

  // The browser is only interested in new active downloads. History downloads
  // that are completed or interrupted are not displayed on the shelf. The
  // downloads page independently listens for new downloads when it is active.
  // Note that the UI will be notified of downloads even if they are not meant
  // to be displayed on the shelf (i.e. ShouldShowInShelf() returns false). This
  // is because: *  The shelf isn't the only UI. E.g. on Android, the UI is the
  // system
  //    DownloadManager.
  // *  There are other UI activities that need to be performed. E.g. if the
  //    download was initiated from a new tab, then that tab should be closed.
  return download_->GetDownloadCreationType() !=
             download::DownloadItem::DownloadCreationType::
                 TYPE_HISTORY_IMPORT ||
         download_->GetState() == download::DownloadItem::IN_PROGRESS;
}

bool DownloadItemModel::WasUINotified() const {
  const DownloadItemModelData* data = DownloadItemModelData::Get(download_);
  return data && data->was_ui_notified_;
}

void DownloadItemModel::SetWasUINotified(bool was_ui_notified) {
  DownloadItemModelData* data = DownloadItemModelData::GetOrCreate(download_);
  data->was_ui_notified_ = was_ui_notified;
}

bool DownloadItemModel::WasActionedOn() const {
  const DownloadItemModelData* data = DownloadItemModelData::Get(download_);
  if (!data) {
    return DownloadUIModel::WasActionedOn();
  }
  return data && data->actioned_on_;
}

void DownloadItemModel::SetActionedOn(bool actioned_on) {
  DownloadItemModelData* data = DownloadItemModelData::GetOrCreate(download_);
  data->actioned_on_ = actioned_on;
}

bool DownloadItemModel::WasUIWarningShown() const {
  const DownloadItemModelData* data = DownloadItemModelData::Get(download_);
  return data && data->was_ui_warning_shown_;
}

void DownloadItemModel::SetWasUIWarningShown(bool was_ui_warning_shown) {
  DownloadItemModelData* data = DownloadItemModelData::GetOrCreate(download_);
  data->was_ui_warning_shown_ = was_ui_warning_shown;
}

std::optional<base::Time> DownloadItemModel::GetEphemeralWarningUiShownTime()
    const {
  const DownloadItemModelData* data = DownloadItemModelData::Get(download_);
  return data ? data->ephemeral_warning_ui_shown_time_
              : std::optional<base::Time>();
}

void DownloadItemModel::SetEphemeralWarningUiShownTime(
    std::optional<base::Time> ephemeral_warning_ui_shown_time) {
  DownloadItemModelData* data = DownloadItemModelData::GetOrCreate(download_);
  data->ephemeral_warning_ui_shown_time_ = ephemeral_warning_ui_shown_time;
}

bool DownloadItemModel::ShouldPreferOpeningInBrowser() {
  const DownloadItemModelData* data =
      DownloadItemModelData::GetOrCreate(download_);
#if !BUILDFLAG(IS_ANDROID)
  if (!data->should_prefer_opening_in_browser_) {
    base::FilePath path = GetTargetFilePath();
    std::string mime_type = GetMimeType();
    DetermineAndSetShouldPreferOpeningInBrowser(
        path,
        DownloadTargetDeterminer::DetermineIfHandledSafelyHelperSynchronous(
            download_, path, mime_type));
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  return data->should_prefer_opening_in_browser_.value_or(false);
}

void DownloadItemModel::SetShouldPreferOpeningInBrowser(bool preference) {
  DownloadItemModelData* data = DownloadItemModelData::GetOrCreate(download_);
  data->should_prefer_opening_in_browser_ = preference;
}

DownloadFileType::DangerLevel DownloadItemModel::GetDangerLevel() const {
  const DownloadItemModelData* data = DownloadItemModelData::Get(download_);
  return data ? data->danger_level_ : DownloadFileType::NOT_DANGEROUS;
}

void DownloadItemModel::SetDangerLevel(
    DownloadFileType::DangerLevel danger_level) {
  DownloadItemModelData* data = DownloadItemModelData::GetOrCreate(download_);
  data->danger_level_ = danger_level;
}

download::DownloadItem::InsecureDownloadStatus
DownloadItemModel::GetInsecureDownloadStatus() const {
  return download_->GetInsecureDownloadStatus();
}

bool DownloadItemModel::IsBeingRevived() const {
  const DownloadItemModelData* data = DownloadItemModelData::Get(download_);
  return data && data->is_being_revived_;
}

void DownloadItemModel::SetIsBeingRevived(bool is_being_revived) {
  DownloadItemModelData* data = DownloadItemModelData::GetOrCreate(download_);
  data->is_being_revived_ = is_being_revived;
}

const download::DownloadItem* DownloadItemModel::GetDownloadItem() const {
  return download_;
}

base::FilePath DownloadItemModel::GetFileNameToReportUser() const {
  return download_->GetFileNameToReportUser();
}

base::FilePath DownloadItemModel::GetTargetFilePath() const {
  return download_->GetTargetFilePath();
}

void DownloadItemModel::OpenDownload() {
  download_->OpenDownload();
}

download::DownloadItem::DownloadState DownloadItemModel::GetState() const {
  return download_->GetState();
}

bool DownloadItemModel::IsPaused() const {
  return download_->IsPaused();
}

download::DownloadDangerType DownloadItemModel::GetDangerType() const {
  return download_->GetDangerType();
}

bool DownloadItemModel::GetOpenWhenComplete() const {
  return download_->GetOpenWhenComplete();
}

bool DownloadItemModel::IsOpenWhenCompleteByPolicy() const {
  return download_->ShouldOpenFileByPolicyBasedOnExtension();
}

bool DownloadItemModel::TimeRemaining(base::TimeDelta* remaining) const {
  return download_->TimeRemaining(remaining);
}

base::Time DownloadItemModel::GetStartTime() const {
  return download_->GetStartTime();
}

base::Time DownloadItemModel::GetEndTime() const {
  return download_->GetEndTime();
}

bool DownloadItemModel::GetOpened() const {
  return download_->GetOpened();
}

void DownloadItemModel::SetOpened(bool opened) {
  download_->SetOpened(opened);
}

bool DownloadItemModel::IsDone() const {
  return download_->IsDone();
}

void DownloadItemModel::Pause() {
  download_->Pause();
}

void DownloadItemModel::Resume() {
  download_->Resume(true /* has_user_gesture */);
}

void DownloadItemModel::Cancel(bool user_cancel) {
  download_->Cancel(user_cancel);
}

void DownloadItemModel::Remove() {
  download_->Remove();
}

void DownloadItemModel::SetOpenWhenComplete(bool open) {
  download_->SetOpenWhenComplete(open);
}

base::FilePath DownloadItemModel::GetFullPath() const {
  return download_->GetFullPath();
}

bool DownloadItemModel::CanResume() const {
  return download_->CanResume();
}

bool DownloadItemModel::AllDataSaved() const {
  return download_->AllDataSaved();
}

bool DownloadItemModel::GetFileExternallyRemoved() const {
  return download_->GetFileExternallyRemoved();
}

GURL DownloadItemModel::GetURL() const {
  return download_->GetURL();
}

bool DownloadItemModel::HasUserGesture() const {
  return download_->HasUserGesture();
}

void DownloadItemModel::OnDownloadUpdated(DownloadItem* download) {
  if (delegate_)
    delegate_->OnDownloadUpdated();
}

void DownloadItemModel::OnDownloadOpened(DownloadItem* download) {
  if (delegate_)
    delegate_->OnDownloadOpened();
}

void DownloadItemModel::OnDownloadDestroyed(DownloadItem* download) {
  ContentId id = GetContentId();
  download_->RemoveObserver(this);
  download_ = nullptr;
  // The object could get deleted after this.
  if (delegate_)
    delegate_->OnDownloadDestroyed(id);
}

void DownloadItemModel::OpenUsingPlatformHandler() {
  DownloadCoreService* download_core_service =
      DownloadCoreServiceFactory::GetForBrowserContext(
          content::DownloadItemUtils::GetBrowserContext(download_));
  if (!download_core_service)
    return;

  ChromeDownloadManagerDelegate* delegate =
      download_core_service->GetDownloadManagerDelegate();
  if (!delegate)
    return;
  delegate->OpenDownloadUsingPlatformHandler(download_);
  RecordDownloadOpen(DOWNLOAD_OPEN_METHOD_USER_PLATFORM,
                     download_->GetMimeType());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::optional<DownloadCommands::Command>
DownloadItemModel::MaybeGetMediaAppAction() const {
  std::string mime_type = GetMimeType();

  if (mime_type == "application/pdf") {
    return DownloadCommands::EDIT_WITH_MEDIA_APP;
  }

  if (base::StartsWith(mime_type, "audio/", base::CompareCase::SENSITIVE) ||
      base::StartsWith(mime_type, "video/", base::CompareCase::SENSITIVE)) {
    return DownloadCommands::OPEN_WITH_MEDIA_APP;
  }

  return std::nullopt;
}

void DownloadItemModel::OpenUsingMediaApp() {
  ash::SystemAppLaunchParams params;
  params.launch_paths.push_back(GetTargetFilePath());
  ash::LaunchSystemWebAppAsync(profile(), ash::SystemWebAppType::MEDIA, params);

  RecordDownloadOpen(DOWNLOAD_OPEN_METHOD_MEDIA_APP, GetMimeType());
}
#endif

#if !BUILDFLAG(IS_ANDROID)
bool DownloadItemModel::IsCommandEnabled(
    const DownloadCommands* download_commands,
    DownloadCommands::Command command) const {
  switch (command) {
    case DownloadCommands::SHOW_IN_FOLDER:
      return download_->CanShowInFolder();
    case DownloadCommands::OPEN_WHEN_COMPLETE:
      return download_->CanOpenDownload() &&
             !download_crx_util::IsExtensionDownload(*download_);
    case DownloadCommands::PLATFORM_OPEN:
      return download_->CanOpenDownload() &&
             !download_crx_util::IsExtensionDownload(*download_);
    case DownloadCommands::ALWAYS_OPEN_TYPE:
      // For temporary downloads, the target filename might be a temporary
      // filename. Don't base an "Always open" decision based on it. Also
      // exclude extensions.
      return download_->CanOpenDownload() &&
             safe_browsing::FileTypePolicies::GetInstance()
                 ->IsAllowedToOpenAutomatically(
                     download_->GetTargetFilePath()) &&
             !download_crx_util::IsExtensionDownload(*download_);
    case DownloadCommands::PAUSE:
      return !download_->IsSavePackageDownload() &&
             DownloadUIModel::IsCommandEnabled(download_commands, command);
    case DownloadCommands::OPEN_WITH_MEDIA_APP:
    case DownloadCommands::EDIT_WITH_MEDIA_APP: {
#if BUILDFLAG(IS_CHROMEOS_ASH)
      std::optional<DownloadCommands::Command> media_app_command =
          MaybeGetMediaAppAction();

      return media_app_command == command && download_->CanOpenDownload() &&
             !download_crx_util::IsExtensionDownload(*download_);
#else
      return false;
#endif
    }
    case DownloadCommands::CANCEL:
    case DownloadCommands::RESUME:
    case DownloadCommands::COPY_TO_CLIPBOARD:
    case DownloadCommands::DISCARD:
    case DownloadCommands::KEEP:
    case DownloadCommands::LEARN_MORE_SCANNING:
    case DownloadCommands::LEARN_MORE_INTERRUPTED:
    case DownloadCommands::LEARN_MORE_INSECURE_DOWNLOAD:
    case DownloadCommands::LEARN_MORE_DOWNLOAD_BLOCKED:
    case DownloadCommands::OPEN_SAFE_BROWSING_SETTING:
    case DownloadCommands::DEEP_SCAN:
    case DownloadCommands::BYPASS_DEEP_SCANNING:
    case DownloadCommands::BYPASS_DEEP_SCANNING_AND_OPEN:
    case DownloadCommands::REVIEW:
    case DownloadCommands::RETRY:
    case DownloadCommands::CANCEL_DEEP_SCAN:
      return DownloadUIModel::IsCommandEnabled(download_commands, command);
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool DownloadItemModel::IsCommandChecked(
    const DownloadCommands* download_commands,
    DownloadCommands::Command command) const {
  switch (command) {
    case DownloadCommands::OPEN_WHEN_COMPLETE:
      return download_->GetOpenWhenComplete() ||
             download_crx_util::IsExtensionDownload(*download_);
    case DownloadCommands::ALWAYS_OPEN_TYPE:
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
      if (download_commands->CanOpenPdfInSystemViewer()) {
        DownloadPrefs* prefs = DownloadPrefs::FromBrowserContext(profile());
        return prefs->ShouldOpenPdfInSystemReader();
      }
#endif
      return download_->ShouldOpenFileBasedOnExtension();
    case DownloadCommands::PAUSE:
    case DownloadCommands::RESUME:
      return IsPaused();
    case DownloadCommands::SHOW_IN_FOLDER:
    case DownloadCommands::PLATFORM_OPEN:
    case DownloadCommands::CANCEL:
    case DownloadCommands::DISCARD:
    case DownloadCommands::KEEP:
    case DownloadCommands::LEARN_MORE_SCANNING:
    case DownloadCommands::LEARN_MORE_INTERRUPTED:
    case DownloadCommands::LEARN_MORE_INSECURE_DOWNLOAD:
    case DownloadCommands::LEARN_MORE_DOWNLOAD_BLOCKED:
    case DownloadCommands::OPEN_SAFE_BROWSING_SETTING:
    case DownloadCommands::COPY_TO_CLIPBOARD:
    case DownloadCommands::DEEP_SCAN:
    case DownloadCommands::BYPASS_DEEP_SCANNING:
    case DownloadCommands::BYPASS_DEEP_SCANNING_AND_OPEN:
    case DownloadCommands::REVIEW:
    case DownloadCommands::RETRY:
    case DownloadCommands::CANCEL_DEEP_SCAN:
    case DownloadCommands::OPEN_WITH_MEDIA_APP:
    case DownloadCommands::EDIT_WITH_MEDIA_APP:
      return false;
  }
  return false;
}

void DownloadItemModel::ExecuteCommand(DownloadCommands* download_commands,
                                       DownloadCommands::Command command) {
  switch (command) {
    case DownloadCommands::SHOW_IN_FOLDER:
      download_->ShowDownloadInShell();
      break;
    case DownloadCommands::OPEN_WHEN_COMPLETE:
      download_->OpenDownload();
      break;
    case DownloadCommands::ALWAYS_OPEN_TYPE: {
      bool is_checked = IsCommandChecked(download_commands,
                                         DownloadCommands::ALWAYS_OPEN_TYPE);
      DownloadPrefs* prefs = DownloadPrefs::FromBrowserContext(profile());
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
      if (download_commands->CanOpenPdfInSystemViewer()) {
        prefs->SetShouldOpenPdfInSystemReader(!is_checked);
        SetShouldPreferOpeningInBrowser(is_checked);
        break;
      }
#endif
      base::FilePath path = download_->GetTargetFilePath();
      if (is_checked)
        prefs->DisableAutoOpenByUserBasedOnExtension(path);
      else
        prefs->EnableAutoOpenByUserBasedOnExtension(path);
      break;
    }
    case DownloadCommands::BYPASS_DEEP_SCANNING_AND_OPEN:
#if BUILDFLAG(FULL_SAFE_BROWSING)
      SetOpenWhenComplete(true);
#endif
      [[fallthrough]];
    case DownloadCommands::BYPASS_DEEP_SCANNING:
#if BUILDFLAG(FULL_SAFE_BROWSING)
      CompleteSafeBrowsingScan();
#endif
      if (download_->GetDangerType() ==
              download::DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING ||
          download_->GetDangerType() ==
              download::
                  DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING) {
        safe_browsing::LogLocalDecryptionEvent(
            safe_browsing::DeepScanEvent::kPromptBypassed);
      } else {
        LogDeepScanEvent(download_,
                         safe_browsing::DeepScanEvent::kPromptBypassed);
      }
      [[fallthrough]];
    case DownloadCommands::KEEP:
      if (IsInsecure()) {
        download_->ValidateInsecureDownload();
        break;
      }
      if (GetDangerType() == download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING) {
        break;
      }
      DCHECK(IsDangerous());
#if BUILDFLAG(FULL_SAFE_BROWSING)
      MaybeSendDownloadReport(/*did_proceed=*/true, download_);
#endif
      download_->ValidateDangerousDownload();
      break;
    case DownloadCommands::DISCARD:
#if BUILDFLAG(FULL_SAFE_BROWSING)
      MaybeSendDownloadReport(/*did_proceed=*/false, download_);
      if (GetDangerType() == download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING) {
        LogDeepScanEvent(download_, safe_browsing::DeepScanEvent::kScanDeleted);
      }
#endif
      DownloadUIModel::ExecuteCommand(download_commands, command);
      break;
    case DownloadCommands::LEARN_MORE_SCANNING: {
#if BUILDFLAG(FULL_SAFE_BROWSING)
      using safe_browsing::DownloadProtectionService;

      safe_browsing::SafeBrowsingService* sb_service =
          g_browser_process->safe_browsing_service();
      DownloadProtectionService* protection_service =
          (sb_service ? sb_service->download_protection_service() : nullptr);
      if (protection_service)
        protection_service->ShowDetailsForDownload(
            download_, download_commands->GetBrowser());
#else
      // Should only be getting invoked if we are using safe browsing.
      NOTREACHED_IN_MIGRATION();
#endif
      break;
    }
    case DownloadCommands::PLATFORM_OPEN:
    case DownloadCommands::CANCEL:
    case DownloadCommands::LEARN_MORE_INTERRUPTED:
    case DownloadCommands::LEARN_MORE_INSECURE_DOWNLOAD:
    case DownloadCommands::LEARN_MORE_DOWNLOAD_BLOCKED:
    case DownloadCommands::OPEN_SAFE_BROWSING_SETTING:
    case DownloadCommands::PAUSE:
    case DownloadCommands::RESUME:
    case DownloadCommands::COPY_TO_CLIPBOARD:
    case DownloadCommands::REVIEW:
    case DownloadCommands::RETRY:
    case DownloadCommands::OPEN_WITH_MEDIA_APP:
    case DownloadCommands::EDIT_WITH_MEDIA_APP:
      DownloadUIModel::ExecuteCommand(download_commands, command);
      break;
    case DownloadCommands::DEEP_SCAN: {
      safe_browsing::DownloadProtectionService::UploadForConsumerDeepScanning(
          download_,
          DownloadItemWarningData::DeepScanTrigger::TRIGGER_CONSUMER_PROMPT,
          /*password=*/std::nullopt);
      break;
    }
    case DownloadCommands::CANCEL_DEEP_SCAN: {
      DownloadCoreService* download_core_service =
          DownloadCoreServiceFactory::GetForBrowserContext(
              content::DownloadItemUtils::GetBrowserContext(download_));
      DCHECK(download_core_service);
      ChromeDownloadManagerDelegate* delegate =
          download_core_service->GetDownloadManagerDelegate();
      DCHECK(delegate);
      LogDeepScanEvent(download_, safe_browsing::DeepScanEvent::kScanCanceled);
      delegate->CheckClientDownloadDone(
          download_->GetId(),
          safe_browsing::DownloadCheckResult::PROMPT_FOR_SCANNING);
      break;
    }
  }
}

TailoredWarningType DownloadItemModel::GetTailoredWarningType() const {
  if (!base::FeatureList::IsEnabled(safe_browsing::kDownloadTailoredWarnings)) {
    return TailoredWarningType::kNoTailoredWarning;
  }

  download::DownloadDangerType danger_type = GetDangerType();
  TailoredVerdict tailored_verdict = safe_browsing::DownloadProtectionService::
      GetDownloadProtectionTailoredVerdict(download_);
  if (danger_type == download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT &&
      tailored_verdict.tailored_verdict_type() ==
          TailoredVerdict::SUSPICIOUS_ARCHIVE) {
    return TailoredWarningType::kSuspiciousArchive;
  }

  if (danger_type ==
          download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE &&
      tailored_verdict.tailored_verdict_type() ==
          TailoredVerdict::COOKIE_THEFT) {
    if (base::Contains(tailored_verdict.adjustments(),
                       TailoredVerdict::ACCOUNT_INFO_STRING)) {
      return TailoredWarningType::kCookieTheftWithAccountInfo;
    }
    return TailoredWarningType::kCookieTheft;
  }

  return TailoredWarningType::kNoTailoredWarning;
}

DangerUiPattern DownloadItemModel::GetDangerUiPattern() const {
  // Keep logic here in sync with DownloadBubbleRowViewInfo and
  // IconAndColor code in download_bubble_info_utils.cc, and
  // chrome://downloads WebUI frontend code.
  DownloadItem::DownloadState state = GetState();

  // Error conditions, including cancellations, have a "download off" icon or
  // some combination of "info" icon and red or gray.
  if (state == DownloadItem::CANCELLED || state == DownloadItem::INTERRUPTED) {
    return DangerUiPattern::kOther;
  } else if (state == DownloadItem::MAX_DOWNLOAD_STATE) {
    NOTREACHED();
  }

  switch (GetInsecureDownloadStatus()) {
    case DownloadItem::InsecureDownloadStatus::BLOCK:
    case DownloadItem::InsecureDownloadStatus::WARN:
      return DangerUiPattern::kSuspicious;
    case DownloadItem::InsecureDownloadStatus::UNKNOWN:
    case DownloadItem::InsecureDownloadStatus::SAFE:
    case DownloadItem::InsecureDownloadStatus::VALIDATED:
    case DownloadItem::InsecureDownloadStatus::SILENT_BLOCK:
      break;
  }

  switch (GetTailoredWarningType()) {
    case TailoredWarningType::kCookieTheft:
    case TailoredWarningType::kCookieTheftWithAccountInfo:
      return DangerUiPattern::kDangerous;
    case TailoredWarningType::kSuspiciousArchive:
      return DangerUiPattern::kSuspicious;
    case TailoredWarningType::kNoTailoredWarning:
      break;
  }

  switch (GetDangerType()) {
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      return DangerUiPattern::kDangerous;
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED:
      return DangerUiPattern::kSuspicious;
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
      return DangerUiPattern::kOther;
    // TODO(crbug.com/329254526): The following two may be wrong.
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_SCAN_FAILED:
    case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
    case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case download::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY:
      break;
    case download::DOWNLOAD_DANGER_TYPE_MAX:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  return DangerUiPattern::kNormal;
}

bool DownloadItemModel::ShouldShowInBubble() const {
  // Downloads blocked by local policies should be notified, otherwise users
  // won't get any feedback that the download has failed.
  bool should_notify =
      download_->GetLastReason() ==
          download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED &&
      download_->GetInsecureDownloadStatus() !=
          download::DownloadItem::InsecureDownloadStatus::SILENT_BLOCK;

  // Wait until the target path is determined.
  if (download_->GetTargetFilePath().empty() && !should_notify) {
    return false;
  }

  if (IsEphemeralWarning()) {
    // Ephemeral warnings become canceled if the browser shuts down (or an hour
    // after being displayed if the user hasn't acted on them). These should no
    // longer be shown, regardless of what the shown time is set to.
    if (download_->GetState() == download::DownloadItem::CANCELLED) {
      return false;
    }

    // If the user hasn't acted on an ephemeral warning within 5 minutes, it
    // should no longer be shown in the bubble. (IsEphemeralWarning no longer
    // returns true once the user has acted on the warning.)
    auto warning_shown_time = GetEphemeralWarningUiShownTime();
    if (warning_shown_time.has_value() &&
        base::Time::Now() - warning_shown_time.value() >
            kEphemeralWarningLifetimeOnBubble) {
      return false;
    }
  }

  return DownloadUIModel::ShouldShowInBubble();
}

bool DownloadItemModel::IsEphemeralWarning() const {
  switch (GetInsecureDownloadStatus()) {
    case download::DownloadItem::InsecureDownloadStatus::BLOCK:
    case download::DownloadItem::InsecureDownloadStatus::WARN:
      return true;
    case download::DownloadItem::InsecureDownloadStatus::UNKNOWN:
    case download::DownloadItem::InsecureDownloadStatus::SAFE:
    case download::DownloadItem::InsecureDownloadStatus::VALIDATED:
    case download::DownloadItem::InsecureDownloadStatus::SILENT_BLOCK:
      break;
  }

  switch (GetDangerType()) {
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED:
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING:
      return true;
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case download::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY:
    case download::DOWNLOAD_DANGER_TYPE_MAX:
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_SCAN_FAILED:
      return false;
  }
}

#endif  // !BUILDFLAG(IS_ANDROID)

offline_items_collection::FailState DownloadItemModel::GetLastFailState()
    const {
  return OfflineItemUtils::ConvertDownloadInterruptReasonToFailState(
      download_->GetLastReason());
}

std::string DownloadItemModel::GetMimeType() const {
  return download_->GetMimeType();
}

bool DownloadItemModel::IsExtensionDownload() const {
  return download_crx_util::IsExtensionDownload(*download_);
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
void DownloadItemModel::CompleteSafeBrowsingScan() {
  if (download_->IsSavePackageDownload()) {
    download_->OnAsyncScanningCompleted(
        download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED);
    enterprise_connectors::RunSavePackageScanningCallback(download_, true);
  } else {
    ChromeDownloadManagerDelegate::SafeBrowsingState* state =
        static_cast<ChromeDownloadManagerDelegate::SafeBrowsingState*>(
            download_->GetUserData(
                &ChromeDownloadManagerDelegate::SafeBrowsingState::
                    kSafeBrowsingUserDataKey));
    state->CompleteDownload();
  }
}

void DownloadItemModel::ReviewScanningVerdict(
    content::WebContents* web_contents) {
  auto command_callback =
      [](std::unique_ptr<DownloadItemModel> model,
         std::unique_ptr<DownloadCommands> download_commands,
         DownloadCommands::Command command) {
        model->ExecuteCommand(download_commands.get(), command);
      };
  enterprise_connectors::ShowDownloadReviewDialog(
      GetFileNameToReportUser().LossyDisplayName(), profile(), download_,
      web_contents,
      base::BindOnce(
          command_callback, std::make_unique<DownloadItemModel>(download_),
          std::make_unique<DownloadCommands>(DownloadUIModel::GetWeakPtr()),
          DownloadCommands::KEEP),
      base::BindOnce(
          command_callback, std::make_unique<DownloadItemModel>(download_),
          std::make_unique<DownloadCommands>(DownloadUIModel::GetWeakPtr()),
          DownloadCommands::DISCARD));
}
#endif

bool DownloadItemModel::ShouldShowDropdown() const {
  // We don't show the dropdown for dangerous file types or for files
  // blocked by enterprise policy.
  if (IsDangerous() && GetState() != DownloadItem::CANCELLED &&
      !MightBeMalicious()) {
    return false;
  }

  if (GetDangerType() ==
          download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK ||
      GetDangerType() ==
          download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED ||
      GetDangerType() == download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE ||
      GetDangerType() == download::DOWNLOAD_DANGER_TYPE_BLOCKED_SCAN_FAILED) {
    return false;
  }

  return true;
}

void DownloadItemModel::DetermineAndSetShouldPreferOpeningInBrowser(
    const base::FilePath& target_path,
    bool is_filetype_handled_safely) {
  DownloadCoreService* download_core_service =
      DownloadCoreServiceFactory::GetForBrowserContext(
          content::DownloadItemUtils::GetBrowserContext(download_));
  if (!download_core_service)
    return;

  ChromeDownloadManagerDelegate* delegate =
      download_core_service->GetDownloadManagerDelegate();
  if (!delegate)
    return;

  if (!target_path.empty() &&
      delegate->IsOpenInBrowserPreferredForFile(target_path) &&
      is_filetype_handled_safely) {
    SetShouldPreferOpeningInBrowser(true);
    return;
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (download_->GetOriginalMimeType() == "application/x-x509-user-cert") {
    SetShouldPreferOpeningInBrowser(true);
    return;
  }
#endif
  SetShouldPreferOpeningInBrowser(false);
}

bool DownloadItemModel::IsTopLevelEncryptedArchive() const {
  return DownloadItemWarningData::IsTopLevelEncryptedArchive(download_);
}
