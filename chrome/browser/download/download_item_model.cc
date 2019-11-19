// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_item_model.h"

#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/download_protection/download_feedback_service.h"
#include "chrome/common/safe_browsing/download_file_types.pb.h"
#include "chrome/common/safe_browsing/file_type_policies.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/browser/download_item_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/text/bytes_formatting.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser.h"
#endif

using base::TimeDelta;
using download::DownloadItem;
using safe_browsing::DownloadFileType;

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
  bool should_show_in_shelf_;

  // Whether the UI has been notified about this download.
  bool was_ui_notified_;

  // Whether the download should be opened in the browser vs. the system handler
  // for the file type.
  bool should_prefer_opening_in_browser_;

  // Danger level of the file determined based on the file type and whether
  // there was a user action associated with the download.
  DownloadFileType::DangerLevel danger_level_;

  // Whether the download is currently being revived.
  bool is_being_revived_;

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
  if (data == NULL) {
    data = new DownloadItemModelData();
    data->should_show_in_shelf_ = !download->IsTransient();
    download->SetUserData(kKey, base::WrapUnique(data));
  }
  return data;
}

DownloadItemModelData::DownloadItemModelData()
    : should_show_in_shelf_(true),
      was_ui_notified_(false),
      should_prefer_opening_in_browser_(false),
      danger_level_(DownloadFileType::NOT_DANGEROUS),
      is_being_revived_(false) {}

} // namespace

// -----------------------------------------------------------------------------
// DownloadItemModel

// static
DownloadUIModel::DownloadUIModelPtr DownloadItemModel::Wrap(
    download::DownloadItem* download) {
  DownloadUIModel::DownloadUIModelPtr model(
      new DownloadItemModel(download),
      base::OnTaskRunnerDeleter(base::ThreadTaskRunnerHandle::Get()));
  return model;
}

DownloadItemModel::DownloadItemModel(DownloadItem* download)
    : download_(download) {
  download_->AddObserver(this);
}

DownloadItemModel::~DownloadItemModel() {
  if (download_)
    download_->RemoveObserver(this);
}

ContentId DownloadItemModel::GetContentId() const {
  bool off_the_record = content::DownloadItemUtils::GetBrowserContext(download_)
                            ->IsOffTheRecord();
  return ContentId(OfflineItemUtils::GetDownloadNamespacePrefix(off_the_record),
                   download_->GetGuid());
}

Profile* DownloadItemModel::profile() const {
  return Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(download_));
}

base::string16 DownloadItemModel::GetTabProgressStatusText() const {
  int64_t total = GetTotalBytes();
  int64_t size = download_->GetReceivedBytes();
  base::string16 received_size = ui::FormatBytes(size);
  base::string16 amount = received_size;

  // Adjust both strings for the locale direction since we don't yet know which
  // string we'll end up using for constructing the final progress string.
  base::i18n::AdjustStringForLocaleDirection(&amount);

  if (total) {
    base::string16 total_text = ui::FormatBytes(total);
    base::i18n::AdjustStringForLocaleDirection(&total_text);

    base::i18n::AdjustStringForLocaleDirection(&received_size);
    amount = l10n_util::GetStringFUTF16(
        IDS_DOWNLOAD_TAB_PROGRESS_SIZE, received_size, total_text);
  } else {
    amount.assign(received_size);
  }
  int64_t current_speed = download_->CurrentSpeed();
  base::string16 speed_text = ui::FormatSpeed(current_speed);
  base::i18n::AdjustStringForLocaleDirection(&speed_text);

  base::TimeDelta remaining;
  base::string16 time_remaining;
  if (download_->IsPaused()) {
    time_remaining = l10n_util::GetStringUTF16(IDS_DOWNLOAD_PROGRESS_PAUSED);
  } else if (download_->TimeRemaining(&remaining)) {
    time_remaining = ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_REMAINING,
                                            ui::TimeFormat::LENGTH_SHORT,
                                            remaining);
  }

  if (time_remaining.empty()) {
    base::i18n::AdjustStringForLocaleDirection(&amount);
    return l10n_util::GetStringFUTF16(
        IDS_DOWNLOAD_TAB_PROGRESS_STATUS_TIME_UNKNOWN, speed_text, amount);
  }
  return l10n_util::GetStringFUTF16(
      IDS_DOWNLOAD_TAB_PROGRESS_STATUS, speed_text, amount, time_remaining);
}


int64_t DownloadItemModel::GetCompletedBytes() const {
  return download_->GetReceivedBytes();
}

int64_t DownloadItemModel::GetTotalBytes() const {
  return download_->AllDataSaved() ? download_->GetReceivedBytes() :
                                     download_->GetTotalBytes();
}

// TODO(asanka,rdsmith): Once 'open' moves exclusively to the
//     ChromeDownloadManagerDelegate, we should calculate the percentage here
//     instead of calling into the DownloadItem.
int DownloadItemModel::PercentComplete() const {
  return download_->PercentComplete();
}

bool DownloadItemModel::IsDangerous() const {
  return download_->IsDangerous();
}

bool DownloadItemModel::MightBeMalicious() const {
  if (!IsDangerous())
    return false;
  switch (download_->GetDangerType()) {
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK:
      return true;

    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
    case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case download::DOWNLOAD_DANGER_TYPE_WHITELISTED_BY_POLICY:
    case download::DOWNLOAD_DANGER_TYPE_MAX:
      // We shouldn't get any of these due to the IsDangerous() test above.
      NOTREACHED();
      FALLTHROUGH;
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      return false;
  }
  NOTREACHED();
  return false;
}

// If you change this definition of malicious, also update
// DownloadManagerImpl::NonMaliciousInProgressCount.
bool DownloadItemModel::IsMalicious() const {
  if (!MightBeMalicious())
    return false;
  switch (download_->GetDangerType()) {
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
      return true;

    case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case download::DOWNLOAD_DANGER_TYPE_WHITELISTED_BY_POLICY:
    case download::DOWNLOAD_DANGER_TYPE_MAX:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      // We shouldn't get any of these due to the MightBeMalicious() test above.
      NOTREACHED();
      FALLTHROUGH;
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
      return false;
  }
  NOTREACHED();
  return false;
}

bool DownloadItemModel::ShouldAllowDownloadFeedback() const {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  if (!IsDangerous())
    return false;
  return safe_browsing::DownloadFeedbackService::IsEnabledForDownload(
      *download_);
#else
  return false;
#endif
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
      // TODO(asanka): The logic for deciding opening behavior should be in a
      //               central location. http://crbug.com/167702
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
      NOTREACHED();
  }

  NOTREACHED();
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

bool DownloadItemModel::ShouldPreferOpeningInBrowser() const {
  const DownloadItemModelData* data = DownloadItemModelData::Get(download_);
  return data && data->should_prefer_opening_in_browser_;
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

bool DownloadItemModel::IsBeingRevived() const {
  const DownloadItemModelData* data = DownloadItemModelData::Get(download_);
  return data && data->is_being_revived_;
}

void DownloadItemModel::SetIsBeingRevived(bool is_being_revived) {
  DownloadItemModelData* data = DownloadItemModelData::GetOrCreate(download_);
  data->is_being_revived_ = is_being_revived;
}

download::DownloadItem* DownloadItemModel::download() {
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

bool DownloadItemModel::TimeRemaining(base::TimeDelta* remaining) const {
  return download_->TimeRemaining(remaining);
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

void DownloadItemModel::OnDownloadUpdated(DownloadItem* download) {
  for (auto& obs : observers_)
    obs.OnDownloadUpdated();
}

void DownloadItemModel::OnDownloadOpened(DownloadItem* download) {
  for (auto& obs : observers_)
    obs.OnDownloadOpened();
}

void DownloadItemModel::OnDownloadDestroyed(DownloadItem* download) {
  for (auto& obs : observers_)
    obs.OnDownloadDestroyed();
  download_ = nullptr;
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
  RecordDownloadOpenMethod(DOWNLOAD_OPEN_METHOD_USER_PLATFORM);
}

#if !defined(OS_ANDROID)
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
    case DownloadCommands::CANCEL:
    case DownloadCommands::RESUME:
    case DownloadCommands::COPY_TO_CLIPBOARD:
    case DownloadCommands::ANNOTATE:
    case DownloadCommands::DISCARD:
    case DownloadCommands::KEEP:
    case DownloadCommands::LEARN_MORE_SCANNING:
    case DownloadCommands::LEARN_MORE_INTERRUPTED:
      return DownloadUIModel::IsCommandEnabled(download_commands, command);
  }
  NOTREACHED();
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
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX)
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
    case DownloadCommands::COPY_TO_CLIPBOARD:
    case DownloadCommands::ANNOTATE:
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
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX)
      if (download_commands->CanOpenPdfInSystemViewer()) {
        prefs->SetShouldOpenPdfInSystemReader(!is_checked);
        SetShouldPreferOpeningInBrowser(is_checked);
        break;
      }
#endif
      base::FilePath path = download_->GetTargetFilePath();
      if (is_checked)
        prefs->DisableAutoOpenBasedOnExtension(path);
      else
        prefs->EnableAutoOpenBasedOnExtension(path);
      break;
    }
    case DownloadCommands::KEEP:
// Only sends uncommon download accept report if :
// 1. FULL_SAFE_BROWSING is enabled, and
// 2. Download verdict is uncommon, and
// 3. Download URL is not empty, and
// 4. User is not in incognito mode.
#if BUILDFLAG(FULL_SAFE_BROWSING)
      if (GetDangerType() == download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT &&
          !GetURL().is_empty() && !profile()->IsOffTheRecord()) {
        safe_browsing::SafeBrowsingService* sb_service =
            g_browser_process->safe_browsing_service();
        // Compiles the uncommon download warning report.
        safe_browsing::ClientSafeBrowsingReportRequest report;
        report.set_type(safe_browsing::ClientSafeBrowsingReportRequest::
                            DANGEROUS_DOWNLOAD_WARNING);
        report.set_download_verdict(
            safe_browsing::ClientDownloadResponse::UNCOMMON);
        report.set_url(GetURL().spec());
        report.set_did_proceed(true);
        std::string token =
            safe_browsing::DownloadProtectionService::GetDownloadPingToken(
                download_);
        if (!token.empty())
          report.set_token(token);
        std::string serialized_report;
        if (report.SerializeToString(&serialized_report)) {
          sb_service->SendSerializedDownloadReport(serialized_report);
        } else {
          DCHECK(false)
              << "Unable to serialize the uncommon download warning report.";
        }
      }
#endif
      download_->ValidateDangerousDownload();
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
      NOTREACHED();
#endif
      break;
    }
    case DownloadCommands::PLATFORM_OPEN:
    case DownloadCommands::CANCEL:
    case DownloadCommands::DISCARD:
    case DownloadCommands::LEARN_MORE_INTERRUPTED:
    case DownloadCommands::PAUSE:
    case DownloadCommands::RESUME:
    case DownloadCommands::COPY_TO_CLIPBOARD:
    case DownloadCommands::ANNOTATE:
      DownloadUIModel::ExecuteCommand(download_commands, command);
      break;
  }
}
#endif

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
  ChromeDownloadManagerDelegate::SafeBrowsingState* state =
      static_cast<ChromeDownloadManagerDelegate::SafeBrowsingState*>(
          download_->GetUserData(
              &ChromeDownloadManagerDelegate::SafeBrowsingState::
                  kSafeBrowsingUserDataKey));
  state->CompleteDownload();
}
#endif
