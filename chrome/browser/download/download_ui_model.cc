// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_ui_model.h"

#include <utility>

#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/download/download_ui_safe_browsing_util.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/google/core/common/google_util.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_referral_methods.h"
#include "components/vector_icons/vector_icons.h"
#include "net/base/mime_util.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "components/url_formatter/elide_url.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/vector_icons.h"
#endif

using download::DownloadItem;
using offline_items_collection::FailState;
using safe_browsing::DownloadFileType;

namespace {

// TODO(qinmin): Migrate this description generator to OfflineItemUtils once
// that component gets used to build desktop UI.
std::u16string FailStateDescription(FailState fail_state) {
  int string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS;
  std::u16string status_text;

  switch (fail_state) {
    case FailState::FILE_ACCESS_DENIED:
      string_id = IDS_DOWNLOAD_INTERRUPTED_DESCRIPTION_ACCESS_DENIED;
      break;
    case FailState::FILE_NO_SPACE:
      string_id = IDS_DOWNLOAD_INTERRUPTED_DESCRIPTION_DISK_FULL;
      break;
    case FailState::FILE_NAME_TOO_LONG:
      string_id = IDS_DOWNLOAD_INTERRUPTED_DESCRIPTION_PATH_TOO_LONG;
      break;
    case FailState::FILE_TOO_LARGE:
      string_id = IDS_DOWNLOAD_INTERRUPTED_DESCRIPTION_FILE_TOO_LARGE;
      break;
    case FailState::FILE_VIRUS_INFECTED:
      string_id = IDS_DOWNLOAD_INTERRUPTED_DESCRIPTION_VIRUS;
      break;
    case FailState::FILE_TRANSIENT_ERROR:
      string_id = IDS_DOWNLOAD_INTERRUPTED_DESCRIPTION_TEMPORARY_PROBLEM;
      break;
    case FailState::FILE_BLOCKED:
      string_id = IDS_DOWNLOAD_INTERRUPTED_DESCRIPTION_BLOCKED;
      break;
    case FailState::FILE_SECURITY_CHECK_FAILED:
      string_id = IDS_DOWNLOAD_INTERRUPTED_DESCRIPTION_SECURITY_CHECK_FAILED;
      break;
    case FailState::FILE_TOO_SHORT:
      string_id = IDS_DOWNLOAD_INTERRUPTED_DESCRIPTION_FILE_TOO_SHORT;
      break;
    case FailState::FILE_SAME_AS_SOURCE:
      string_id = IDS_DOWNLOAD_INTERRUPTED_DESCRIPTION_FILE_SAME_AS_SOURCE;
      break;
    case FailState::NETWORK_INVALID_REQUEST:
    case FailState::NETWORK_FAILED:
      string_id = IDS_DOWNLOAD_INTERRUPTED_DESCRIPTION_NETWORK_ERROR;
      break;
    case FailState::NETWORK_TIMEOUT:
      string_id = IDS_DOWNLOAD_INTERRUPTED_DESCRIPTION_NETWORK_TIMEOUT;
      break;
    case FailState::NETWORK_DISCONNECTED:
      string_id = IDS_DOWNLOAD_INTERRUPTED_DESCRIPTION_NETWORK_DISCONNECTED;
      break;
    case FailState::NETWORK_SERVER_DOWN:
      string_id = IDS_DOWNLOAD_INTERRUPTED_DESCRIPTION_SERVER_DOWN;
      break;
    case FailState::SERVER_FAILED:
      string_id = IDS_DOWNLOAD_INTERRUPTED_DESCRIPTION_SERVER_PROBLEM;
      break;
    case FailState::SERVER_BAD_CONTENT:
      string_id = IDS_DOWNLOAD_INTERRUPTED_DESCRIPTION_NO_FILE;
      break;
    case FailState::USER_CANCELED:
      string_id = IDS_DOWNLOAD_STATUS_CANCELLED;
      break;
    case FailState::USER_SHUTDOWN:
      string_id = IDS_DOWNLOAD_INTERRUPTED_DESCRIPTION_SHUTDOWN;
      break;
    case FailState::CRASH:
      string_id = IDS_DOWNLOAD_INTERRUPTED_DESCRIPTION_CRASH;
      break;
    case FailState::SERVER_UNAUTHORIZED:
      string_id = IDS_DOWNLOAD_INTERRUPTED_DESCRIPTION_UNAUTHORIZED;
      break;
    case FailState::SERVER_CERT_PROBLEM:
      string_id = IDS_DOWNLOAD_INTERRUPTED_DESCRIPTION_SERVER_CERT_PROBLEM;
      break;
    case FailState::SERVER_FORBIDDEN:
      string_id = IDS_DOWNLOAD_INTERRUPTED_DESCRIPTION_FORBIDDEN;
      break;
    case FailState::SERVER_UNREACHABLE:
      string_id = IDS_DOWNLOAD_INTERRUPTED_DESCRIPTION_UNREACHABLE;
      break;
    case FailState::SERVER_CONTENT_LENGTH_MISMATCH:
      string_id = IDS_DOWNLOAD_INTERRUPTED_DESCRIPTION_CONTENT_LENGTH_MISMATCH;
      break;
    case FailState::NO_FAILURE:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    // fallthrough
    case FailState::CANNOT_DOWNLOAD:
    case FailState::NETWORK_INSTABILITY:
    case FailState::SERVER_NO_RANGE:
    case FailState::SERVER_CROSS_ORIGIN_REDIRECT:
    case FailState::FILE_FAILED:
    case FailState::FILE_HASH_MISMATCH:
      string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS;
  }

  status_text = l10n_util::GetStringUTF16(string_id);

  return status_text;
}

}  // namespace

DownloadUIModel::DownloadUIModel()
    : DownloadUIModel::DownloadUIModel(std::make_unique<StatusTextBuilder>()) {}

DownloadUIModel::DownloadUIModel(
    std::unique_ptr<StatusTextBuilderBase> status_text_builder)
    : status_text_builder_(std::move(status_text_builder)) {
  status_text_builder_->SetModel(this);
}

DownloadUIModel::~DownloadUIModel() = default;

void DownloadUIModel::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

base::WeakPtr<DownloadUIModel> DownloadUIModel::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool DownloadUIModel::HasSupportedImageMimeType() const {
  if (blink::IsSupportedImageMimeType(GetMimeType()))
    return true;

  std::string mime;
  base::FilePath::StringType extension_with_dot =
      GetTargetFilePath().FinalExtension();
  if (!extension_with_dot.empty() &&
      net::GetWellKnownMimeTypeFromExtension(extension_with_dot.substr(1),
                                             &mime) &&
      blink::IsSupportedImageMimeType(mime)) {
    return true;
  }

  return false;
}

std::u16string DownloadUIModel::GetProgressSizesString() const {
  return status_text_builder_->GetProgressSizesString();
}

std::u16string DownloadUIModel::StatusTextBuilder::GetProgressSizesString()
    const {
  std::u16string size_ratio;
  int64_t size = model_->GetCompletedBytes();
  int64_t total = model_->GetTotalBytes();
  if (total > 0) {
    ui::DataUnits amount_units = ui::GetByteDisplayUnits(total);
    std::u16string simple_size =
        ui::FormatBytesWithUnits(size, amount_units, false);

    // In RTL locales, we render the text "size/total" in an RTL context. This
    // is problematic since a string such as "123/456 MB" is displayed
    // as "MB 123/456" because it ends with an LTR run. In order to solve this,
    // we mark the total string as an LTR string if the UI layout is
    // right-to-left so that the string "456 MB" is treated as an LTR run.
    std::u16string simple_total =
        base::i18n::GetDisplayStringInLTRDirectionality(
            ui::FormatBytesWithUnits(total, amount_units, true));
    size_ratio = l10n_util::GetStringFUTF16(IDS_DOWNLOAD_STATUS_SIZES,
                                            simple_size, simple_total);
  } else {
    size_ratio = ui::FormatBytes(size);
  }
  return size_ratio;
}

std::u16string
DownloadUIModel::BubbleStatusTextBuilder::GetProgressSizesString() const {
  std::u16string size_ratio;
  int64_t size = model_->GetCompletedBytes();
  int64_t total = model_->GetTotalBytes();
  if (total > 0) {
    ui::DataUnits amount_units = ui::GetByteDisplayUnits(total);
    std::u16string simple_size =
        ui::FormatBytesWithUnits(size, amount_units, false);
    std::u16string simple_total =
        ui::FormatBytesWithUnits(total, amount_units, true);

    // Linux prepends an RLM (right-to-left mark) in the FormatBytesWithUnits
    // call when showing units if the string has strong RTL characters. This is
    // problematic for this fraction use case because it ends up moving it
    // around so that the numerator is in the wrong place. Therefore, we remove
    // that extra marker before proceeding.
    base::i18n::UnadjustStringForLocaleDirection(&simple_total);
    size_ratio = l10n_util::GetStringFUTF16(IDS_DOWNLOAD_STATUS_SIZES,
                                            simple_size, simple_total);
  } else {
    size_ratio = ui::FormatBytes(size);
  }

  return size_ratio;
}

std::u16string DownloadUIModel::GetStatusText() const {
  return status_text_builder_->GetStatusText(GetState());
}

#if !BUILDFLAG(IS_ANDROID)
std::u16string DownloadUIModel::GetStatusTextForLabel(
    const gfx::FontList& font_list,
    float available_pixel_width) const {
  if (!ShouldPromoteOrigin()) {
    return GetStatusText();
  }
  if (const GURL url = GetOriginalURL(); url.is_valid()) {
    std::u16string url_string = url_formatter::FormatUrlForSecurityDisplay(url);
    // available_pixel_width can be 0 before the view is inflated.
    return available_pixel_width <= 0
               ? url_string
               : gfx::ElideText(url_string, font_list, available_pixel_width,
                                gfx::ELIDE_TAIL);
  }
  return GetStatusText();
}
#endif

std::u16string DownloadUIModel::StatusTextBuilderBase::GetStatusText(
    download::DownloadItem::DownloadState state) const {
  DCHECK(model_);
  switch (state) {
    case DownloadItem::IN_PROGRESS:
      return GetInProgressStatusText();
    case DownloadItem::COMPLETE:
      return GetCompletedStatusText();
    case DownloadItem::INTERRUPTED: {
      const FailState fail_state = model_->GetLastFailState();
      if (fail_state != FailState::USER_CANCELED) {
        return GetInterruptedStatusText(fail_state);
      }
    }
      [[fallthrough]];
    case DownloadItem::CANCELLED:
      return l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_CANCELLED);
    case DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED_IN_MIGRATION();
      return std::u16string();
  }
}

std::u16string DownloadUIModel::GetTooltipText() const {
  std::u16string tooltip = GetFileNameToReportUser().LossyDisplayName();
  if (GetState() == DownloadItem::INTERRUPTED &&
      GetLastFailState() != FailState::USER_CANCELED) {
    tooltip +=
        u"\n" + status_text_builder_->GetFailStateMessage(GetLastFailState());
  }
  return tooltip;
}

std::u16string DownloadUIModel::GetWarningText(const std::u16string& filename,
                                               size_t* offset) const {
  *offset = std::string::npos;
  switch (GetDangerType()) {
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
      return l10n_util::GetStringUTF16(IDS_PROMPT_MALICIOUS_DOWNLOAD_URL);
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      return IsExtensionDownload()
                 ? l10n_util::GetStringUTF16(
                       IDS_PROMPT_DANGEROUS_DOWNLOAD_EXTENSION)
                 : l10n_util::GetStringFUTF16(IDS_PROMPT_DANGEROUS_DOWNLOAD,
                                              filename, offset);
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
      return l10n_util::GetStringFUTF16(IDS_PROMPT_MALICIOUS_DOWNLOAD_CONTENT,
                                        filename, offset);
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT: {
      bool request_ap_verdicts = false;
#if BUILDFLAG(FULL_SAFE_BROWSING)
      request_ap_verdicts =
          safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
              profile())
              ->IsUnderAdvancedProtection();
#endif
      return l10n_util::GetStringFUTF16(
          request_ap_verdicts
              ? IDS_PROMPT_UNCOMMON_DOWNLOAD_CONTENT_IN_ADVANCED_PROTECTION
              : IDS_PROMPT_UNCOMMON_DOWNLOAD_CONTENT,
          filename, offset);
    }
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      return l10n_util::GetStringFUTF16(IDS_PROMPT_DOWNLOAD_CHANGES_SETTINGS,
                                        filename, offset);
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
      return l10n_util::GetStringFUTF16(IDS_PROMPT_DOWNLOAD_BLOCKED_TOO_LARGE,
                                        filename, offset);
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
      return l10n_util::GetStringFUTF16(
          IDS_PROMPT_DOWNLOAD_BLOCKED_PASSWORD_PROTECTED, filename, offset);
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
      return l10n_util::GetStringUTF16(
          IDS_PROMPT_DOWNLOAD_SENSITIVE_CONTENT_WARNING);
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK:
      return l10n_util::GetStringUTF16(
          IDS_PROMPT_DOWNLOAD_SENSITIVE_CONTENT_BLOCKED);
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING:
      return l10n_util::GetStringFUTF16(IDS_PROMPT_DEEP_SCANNING, filename,
                                        offset);
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING:
      return l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_LOCAL_DECRYPTION_PROMPT_ALERT, filename, offset);
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_SCAN_FAILED:
      return l10n_util::GetStringUTF16(IDS_PROMPT_DOWNLOAD_BLOCKED_SCAN_FAILED);
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case download::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY:
    case download::DOWNLOAD_DANGER_TYPE_MAX:
      break;
  }

  switch (GetInsecureDownloadStatus()) {
    case download::DownloadItem::InsecureDownloadStatus::BLOCK:
      return l10n_util::GetStringFUTF16(IDS_PROMPT_DOWNLOAD_INSECURE_BLOCKED,
                                        filename, offset);
    case download::DownloadItem::InsecureDownloadStatus::WARN:
      return l10n_util::GetStringFUTF16(IDS_PROMPT_DOWNLOAD_INSECURE_WARNING,
                                        filename, offset);
    case download::DownloadItem::InsecureDownloadStatus::UNKNOWN:
    case download::DownloadItem::InsecureDownloadStatus::SAFE:
    case download::DownloadItem::InsecureDownloadStatus::VALIDATED:
    case download::DownloadItem::InsecureDownloadStatus::SILENT_BLOCK:
      break;
  }

  return std::u16string();
}

std::u16string DownloadUIModel::GetWarningConfirmButtonText() const {
  const auto kDangerousFile = download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE;
  return l10n_util::GetStringUTF16(
      (GetDangerType() == kDangerousFile && IsExtensionDownload())
          ? IDS_CONTINUE_EXTENSION_DOWNLOAD
          : IDS_CONFIRM_DOWNLOAD);
}

std::u16string DownloadUIModel::GetShowInFolderText() const {
  return l10n_util::GetStringUTF16(IDS_DOWNLOAD_LINK_SHOW);
}

ContentId DownloadUIModel::GetContentId() const {
  NOTREACHED_IN_MIGRATION();
  return ContentId();
}

Profile* DownloadUIModel::profile() const {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

std::u16string DownloadUIModel::GetTabProgressStatusText() const {
  return std::u16string();
}

int64_t DownloadUIModel::GetCompletedBytes() const {
  return 0;
}

int64_t DownloadUIModel::GetTotalBytes() const {
  return 0;
}

int DownloadUIModel::PercentComplete() const {
  return -1;
}

bool DownloadUIModel::IsDangerous() const {
  return false;
}

bool DownloadUIModel::MightBeMalicious() const {
  return false;
}

bool DownloadUIModel::IsMalicious() const {
  return false;
}

bool DownloadUIModel::IsInsecure() const {
  return false;
}

bool DownloadUIModel::ShouldRemoveFromShelfWhenComplete() const {
  return false;
}

bool DownloadUIModel::ShouldShowDownloadStartedAnimation() const {
  return true;
}

bool DownloadUIModel::ShouldShowInShelf() const {
  return true;
}

void DownloadUIModel::SetShouldShowInShelf(bool should_show) {}

bool DownloadUIModel::ShouldNotifyUI() const {
  return true;
}

bool DownloadUIModel::WasUINotified() const {
  return false;
}

void DownloadUIModel::SetWasUINotified(bool should_notify) {}

bool DownloadUIModel::WasActionedOn() const {
  return true;
}

void DownloadUIModel::SetActionedOn(bool actioned_on) {}

bool DownloadUIModel::WasUIWarningShown() const {
  return false;
}

void DownloadUIModel::SetWasUIWarningShown(bool was_ui_warning_shown) {}

std::optional<base::Time> DownloadUIModel::GetEphemeralWarningUiShownTime()
    const {
  return std::optional<base::Time>();
}

void DownloadUIModel::SetEphemeralWarningUiShownTime(
    std::optional<base::Time> time) {}

bool DownloadUIModel::ShouldPreferOpeningInBrowser() {
  return true;
}

void DownloadUIModel::SetShouldPreferOpeningInBrowser(bool preference) {}

DownloadFileType::DangerLevel DownloadUIModel::GetDangerLevel() const {
  return DownloadFileType::NOT_DANGEROUS;
}

void DownloadUIModel::SetDangerLevel(
    DownloadFileType::DangerLevel danger_level) {}

download::DownloadItem::InsecureDownloadStatus
DownloadUIModel::GetInsecureDownloadStatus() const {
  return download::DownloadItem::InsecureDownloadStatus::UNKNOWN;
}

void DownloadUIModel::OpenUsingPlatformHandler() {}

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::optional<DownloadCommands::Command>
DownloadUIModel::MaybeGetMediaAppAction() const {
  return std::nullopt;
}

void DownloadUIModel::OpenUsingMediaApp() {}
#endif

bool DownloadUIModel::IsBeingRevived() const {
  return true;
}

void DownloadUIModel::SetIsBeingRevived(bool is_being_revived) {}

const DownloadItem* DownloadUIModel::GetDownloadItem() const {
  return nullptr;
}

DownloadItem* DownloadUIModel::GetDownloadItem() {
  return const_cast<DownloadItem*>(std::as_const(*this).GetDownloadItem());
}

base::FilePath DownloadUIModel::GetFileNameToReportUser() const {
  return base::FilePath();
}

base::FilePath DownloadUIModel::GetTargetFilePath() const {
  return base::FilePath();
}

void DownloadUIModel::OpenDownload() {
  NOTREACHED_IN_MIGRATION();
}

download::DownloadItem::DownloadState DownloadUIModel::GetState() const {
  return download::DownloadItem::IN_PROGRESS;
}

bool DownloadUIModel::IsPaused() const {
  return false;
}

download::DownloadDangerType DownloadUIModel::GetDangerType() const {
  return download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;
}

bool DownloadUIModel::GetOpenWhenComplete() const {
  return false;
}

bool DownloadUIModel::IsOpenWhenCompleteByPolicy() const {
  return false;
}

bool DownloadUIModel::TimeRemaining(base::TimeDelta* remaining) const {
  return false;
}

base::Time DownloadUIModel::GetStartTime() const {
  return base::Time();
}

base::Time DownloadUIModel::GetEndTime() const {
  return base::Time();
}

bool DownloadUIModel::GetOpened() const {
  return false;
}

void DownloadUIModel::SetOpened(bool opened) {}

bool DownloadUIModel::IsDone() const {
  return false;
}

void DownloadUIModel::Pause() {}

void DownloadUIModel::Resume() {}

void DownloadUIModel::Cancel(bool user_cancel) {}

void DownloadUIModel::Remove() {}

void DownloadUIModel::SetOpenWhenComplete(bool open) {}

FailState DownloadUIModel::GetLastFailState() const {
  return FailState::NO_FAILURE;
}

base::FilePath DownloadUIModel::GetFullPath() const {
  return base::FilePath();
}

bool DownloadUIModel::CanResume() const {
  return false;
}

bool DownloadUIModel::AllDataSaved() const {
  return false;
}

bool DownloadUIModel::GetFileExternallyRemoved() const {
  return false;
}

GURL DownloadUIModel::GetURL() const {
  return GURL();
}

bool DownloadUIModel::HasUserGesture() const {
  return false;
}

GURL DownloadUIModel::GetOriginalURL() const {
  return GURL();
}

bool DownloadUIModel::ShouldPromoteOrigin() const {
  return false;
}

#if !BUILDFLAG(IS_ANDROID)
bool DownloadUIModel::IsCommandEnabled(
    const DownloadCommands* download_commands,
    DownloadCommands::Command command) const {
  switch (command) {
    case DownloadCommands::SHOW_IN_FOLDER:
    case DownloadCommands::OPEN_WHEN_COMPLETE:
    case DownloadCommands::PLATFORM_OPEN:
    case DownloadCommands::ALWAYS_OPEN_TYPE:
    case DownloadCommands::OPEN_WITH_MEDIA_APP:
    case DownloadCommands::EDIT_WITH_MEDIA_APP:
      NOTREACHED_IN_MIGRATION();
      return false;
    case DownloadCommands::CANCEL:
      return !IsDone();
    case DownloadCommands::PAUSE:
      return !IsDone() && !IsPaused() &&
             GetState() == download::DownloadItem::IN_PROGRESS;
    case DownloadCommands::RESUME:
      return CanResume() &&
             (IsPaused() || GetState() != download::DownloadItem::IN_PROGRESS);
    case DownloadCommands::COPY_TO_CLIPBOARD:
      return download_commands->CanBeCopiedToClipboard();
    case DownloadCommands::DISCARD:
    case DownloadCommands::KEEP:
    case DownloadCommands::LEARN_MORE_SCANNING:
    case DownloadCommands::LEARN_MORE_INTERRUPTED:
    case DownloadCommands::LEARN_MORE_INSECURE_DOWNLOAD:
    case DownloadCommands::LEARN_MORE_DOWNLOAD_BLOCKED:
    case DownloadCommands::DEEP_SCAN:
    case DownloadCommands::BYPASS_DEEP_SCANNING:
    case DownloadCommands::BYPASS_DEEP_SCANNING_AND_OPEN:
    case DownloadCommands::REVIEW:
    case DownloadCommands::RETRY:
    case DownloadCommands::CANCEL_DEEP_SCAN:
      return true;
    case DownloadCommands::OPEN_SAFE_BROWSING_SETTING:
      return CanUserTurnOnSafeBrowsing(profile());
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool DownloadUIModel::IsCommandChecked(
    const DownloadCommands* download_commands,
    DownloadCommands::Command command) const {
  switch (command) {
    case DownloadCommands::OPEN_WHEN_COMPLETE:
    case DownloadCommands::ALWAYS_OPEN_TYPE:
      NOTREACHED_IN_MIGRATION();
      return false;
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

void DownloadUIModel::ExecuteCommand(DownloadCommands* download_commands,
                                     DownloadCommands::Command command) {
  switch (command) {
    case DownloadCommands::SHOW_IN_FOLDER:
    case DownloadCommands::OPEN_WHEN_COMPLETE:
    case DownloadCommands::ALWAYS_OPEN_TYPE:
      NOTREACHED_IN_MIGRATION();
      break;
    case DownloadCommands::PLATFORM_OPEN:
      OpenUsingPlatformHandler();
      break;
    case DownloadCommands::CANCEL:
      Cancel(true /* Cancelled by user */);
      break;
    case DownloadCommands::DISCARD:
      Remove();
      break;
    case DownloadCommands::KEEP:
    case DownloadCommands::LEARN_MORE_SCANNING:
      NOTREACHED_IN_MIGRATION();
      break;
    case DownloadCommands::LEARN_MORE_INTERRUPTED:
      download_commands->GetBrowser()->OpenURL(
          content::OpenURLParams(
              download_commands->GetLearnMoreURLForInterruptedDownload(),
              content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
              ui::PAGE_TRANSITION_LINK, false),
          /*navigation_handle_callback=*/{});
      break;
    case DownloadCommands::LEARN_MORE_INSECURE_DOWNLOAD:
      download_commands->GetBrowser()->OpenURL(
          content::OpenURLParams(
              GURL(chrome::kInsecureDownloadBlockingLearnMoreUrl),
              content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
              ui::PAGE_TRANSITION_LINK, false),
          /*navigation_handle_callback=*/{});
      break;
    case DownloadCommands::LEARN_MORE_DOWNLOAD_BLOCKED:
      download_commands->GetBrowser()->OpenURL(
          content::OpenURLParams(google_util::AppendGoogleLocaleParam(
                                     GURL(chrome::kDownloadBlockedLearnMoreURL),
                                     g_browser_process->GetApplicationLocale()),
                                 content::Referrer(),
                                 WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                 ui::PAGE_TRANSITION_LINK, false),
          /*navigation_handle_callback=*/{});
      break;
    case DownloadCommands::OPEN_SAFE_BROWSING_SETTING:
      chrome::ShowSafeBrowsingEnhancedProtectionWithIph(
          download_commands->GetBrowser(),
          safe_browsing::SafeBrowsingSettingReferralMethod::
              kDownloadBubbleSubpage);
      break;
    case DownloadCommands::PAUSE:
      Pause();
      break;
    case DownloadCommands::RESUME:
      Resume();
      break;
    case DownloadCommands::COPY_TO_CLIPBOARD:
      download_commands->CopyFileAsImageToClipboard();
      break;
    case DownloadCommands::DEEP_SCAN:
      break;
    case DownloadCommands::BYPASS_DEEP_SCANNING:
    case DownloadCommands::BYPASS_DEEP_SCANNING_AND_OPEN:
    case DownloadCommands::REVIEW:
    case DownloadCommands::RETRY:
    case DownloadCommands::CANCEL_DEEP_SCAN:
      break;
    case DownloadCommands::OPEN_WITH_MEDIA_APP:
    case DownloadCommands::EDIT_WITH_MEDIA_APP:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      OpenUsingMediaApp();
#else
      NOTREACHED_IN_MIGRATION();
#endif
      break;
  }
}

DownloadUIModel::TailoredWarningType DownloadUIModel::GetTailoredWarningType()
    const {
  return TailoredWarningType::kNoTailoredWarning;
}

DownloadUIModel::DangerUiPattern DownloadUIModel::GetDangerUiPattern() const {
  return DangerUiPattern::kNormal;
}

bool DownloadUIModel::ShouldShowInBubble() const {
  return ShouldShowInShelf();
}

bool DownloadUIModel::IsEphemeralWarning() const {
  return false;
}

#endif  // !BUILDFLAG(IS_ANDROID)

std::string DownloadUIModel::GetMimeType() const {
  return "text/html";
}

bool DownloadUIModel::IsExtensionDownload() const {
  return false;
}

std::u16string DownloadUIModel::StatusTextBuilder::GetInProgressStatusText()
    const {
  DCHECK_EQ(DownloadItem::IN_PROGRESS, model_->GetState());

  base::TimeDelta time_remaining;
  // time_remaining is only known if the download isn't paused, and it isn't
  // going to be rerouted to a web drive.
  bool time_remaining_known =
      (!model_->IsPaused() && model_->TimeRemaining(&time_remaining));

  // Indication of progress. (E.g.:"100/200 MB" or "100MB")
  std::u16string size_ratio = GetProgressSizesString();

  // The download is a CRX (app, extension, theme, ...) and it is being unpacked
  // and validated.
  if (model_->AllDataSaved() && model_->IsExtensionDownload()) {
    return l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_CRX_INSTALL_RUNNING);
  }

  // A paused download: "100/120 MB, Paused"
  if (model_->IsPaused()) {
    return l10n_util::GetStringFUTF16(
        IDS_DOWNLOAD_STATUS_IN_PROGRESS, size_ratio,
        l10n_util::GetStringUTF16(IDS_DOWNLOAD_PROGRESS_PAUSED));
  }

  // A download scheduled to be opened when complete: "Opening in 10 secs"
  if (model_->GetOpenWhenComplete()) {
    if (!time_remaining_known)
      return l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_OPEN_WHEN_COMPLETE);

    return l10n_util::GetStringFUTF16(
        IDS_DOWNLOAD_STATUS_OPEN_IN,
        ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                               ui::TimeFormat::LENGTH_SHORT, time_remaining));
  }

  // In progress download with known time left: "100/120 MB, 10 secs left"
  if (time_remaining_known) {
    return l10n_util::GetStringFUTF16(
        IDS_DOWNLOAD_STATUS_IN_PROGRESS, size_ratio,
        ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_REMAINING,
                               ui::TimeFormat::LENGTH_SHORT, time_remaining));
  }

  const auto completed_bytes = model_->GetCompletedBytes();
  const auto total_bytes = model_->GetTotalBytes();
  if (completed_bytes == 0) {
    // Instead of displaying "0 B" we say "Starting..."
    return l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_STARTING);
  } else if (completed_bytes < total_bytes || total_bytes == 0) {
    // In progress download with no known time left and non-zero completed
    // bytes: "100/120 MB • Resuming..." or "100 MB • Resuming...", or "100/120
    // MB" or "100 MB"
    return size_ratio;
  } else {
    return std::u16string();
  }
}

// static
std::u16string
DownloadUIModel::BubbleStatusTextBuilder::GetBubbleStatusMessageWithBytes(
    const std::u16string& bytes_substring,
    const std::u16string& detail_message,
    bool is_active) {
  // For some RTL languages (e.g. Hebrew), the translated form of 123/456 MB
  // still uses the English characters "MB" rather than RTL characters. We
  // specifically mark this as LTR because it should be displayed as "123/456
  // MB" (not "MB 123/456"). Conversely, some other RTL languages (e.g. Arabic)
  // do translate "MB" to RTL characters. For these, we do nothing, that way the
  // phrase is correctly displayed as RTL, with the translated "MB" to the left
  // of the fraction.
  std::u16string final_bytes_substring =
      base::i18n::GetStringDirection(bytes_substring) ==
              base::i18n::TextDirection::LEFT_TO_RIGHT
          ? base::i18n::GetDisplayStringInLTRDirectionality(bytes_substring)
          : bytes_substring;

  std::u16string download_progress =
      is_active ? l10n_util::GetStringFUTF16(
                      IDS_DOWNLOAD_BUBBLE_DOWNLOAD_STATUS_WITH_SYMBOL,
                      final_bytes_substring)
                : final_bytes_substring;

  std::u16string text = l10n_util::GetStringFUTF16(
      IDS_DOWNLOAD_BUBBLE_DOWNLOAD_STATUS_MESSAGE_WITH_SEPARATOR,
      download_progress, detail_message);

  // Some RTL languages like Hebrew still display "MB" in English
  // characters, which are the first strongly directional characters in
  // the full string. We mark the full string as RTL to ensure it doesn't get
  // displayed as LTR in spite of the first characters ("MB") being LTR.
  base::i18n::AdjustStringForLocaleDirection(&text);
  return text;
}

std::u16string
DownloadUIModel::BubbleStatusTextBuilder::GetBubbleWarningStatusText() const {
  // If the detail message is "Malware", then this returns "Blocked • Malware"
  auto get_blocked_warning = [](int detail_message_id) {
    return l10n_util::GetStringFUTF16(
        IDS_DOWNLOAD_BUBBLE_DOWNLOAD_STATUS_MESSAGE_WITH_SEPARATOR,
        l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_STATUS_BLOCKED),
        l10n_util::GetStringUTF16(detail_message_id));
  };

  switch (model_->GetInsecureDownloadStatus()) {
    case download::DownloadItem::InsecureDownloadStatus::BLOCK:
    case download::DownloadItem::InsecureDownloadStatus::WARN:
      // "Insecure download blocked"
      return l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_BUBBLE_STATUS_WARNING_INSECURE);
    case download::DownloadItem::InsecureDownloadStatus::UNKNOWN:
    case download::DownloadItem::InsecureDownloadStatus::SAFE:
    case download::DownloadItem::InsecureDownloadStatus::VALIDATED:
    case download::DownloadItem::InsecureDownloadStatus::SILENT_BLOCK:
      break;
  }

  switch (model_->GetDangerType()) {
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      if (model_->IsExtensionDownload()) {
        // "Blocked • Unknown source"
        return get_blocked_warning(IDS_DOWNLOAD_BUBBLE_STATUS_UNKNOWN_SOURCE);
      }
      if (WasSafeBrowsingVerdictObtained(model_->GetDownloadItem())) {
        // "Suspicious download blocked"
        return l10n_util::GetStringUTF16(
            IDS_DOWNLOAD_BUBBLE_STATUS_WARNING_SUSPICIOUS);
      }
      // "Unverified download blocked"
      return l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_BUBBLE_STATUS_WARNING_UNVERIFIED);
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
      // "Dangerous download blocked"
      return l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_BUBBLE_STATUS_WARNING_DANGEROUS);
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
      // "Blocked • Encrypted"
      return get_blocked_warning(IDS_DOWNLOAD_BUBBLE_STATUS_ENCRYPTED);
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
      // "Blocked • Too big"
      return get_blocked_warning(IDS_DOWNLOAD_BUBBLE_STATUS_TOO_BIG);
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT: {
      bool request_ap_verdicts = false;
#if BUILDFLAG(FULL_SAFE_BROWSING)
      request_ap_verdicts =
          safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
              model_->profile())
              ->IsUnderAdvancedProtection();
#endif
      // "Blocked by Advanced Protection" or "Suspicious download blocked"
      return request_ap_verdicts
                 ? l10n_util::GetStringUTF16(
                       IDS_DOWNLOAD_BUBBLE_STATUS_ADVANCED_PROTECTION)
                 : l10n_util::GetStringUTF16(
                       IDS_DOWNLOAD_BUBBLE_STATUS_WARNING_SUSPICIOUS);
    }

    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
      // "Sensitive content"
      return l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_BUBBLE_STATUS_SENSITIVE_CONTENT);
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK:
      // "Blocked by your organization"
      return l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_BUBBLE_INTERRUPTED_STATUS_BLOCKED_ORGANIZATION);
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING:
        // "Scan for malware • Suspicious"
        return l10n_util::GetStringFUTF16(
            IDS_DOWNLOAD_BUBBLE_DOWNLOAD_STATUS_MESSAGE_WITH_SEPARATOR,
            l10n_util::GetStringUTF16(
                IDS_DOWNLOAD_BUBBLE_STATUS_DEEP_SCANNING_PROMPT_UPDATED),
            l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_STATUS_SUSPICIOUS));
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING:
      // "Suspicious file blocked • Password needed"
      return l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_BUBBLE_DOWNLOAD_STATUS_MESSAGE_WITH_SEPARATOR,
          l10n_util::GetStringUTF16(
              IDS_DOWNLOAD_BUBBLE_STATUS_LOCAL_DECRYPTION_STATUS),
          l10n_util::GetStringUTF16(
              IDS_DOWNLOAD_BUBBLE_STATUS_PASSWORD_NEEDED));
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
#if BUILDFLAG(IS_ANDROID)
      // "Scanning..."
      return l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_BUBBLE_STATUS_ASYNC_SCANNING);
#else
      // Either "Checking with your organization's security policies..." or
      // "Scanning..."
      if (download::DoesDownloadConnectorBlock(
              model_->profile(), model_->GetDownloadItem()->GetURL())) {
        return l10n_util::GetStringUTF16(
            IDS_DOWNLOAD_BUBBLE_STATUS_ASYNC_SCANNING_ENTERPRISE);
      } else {
        return l10n_util::GetStringUTF16(
            IDS_DOWNLOAD_BUBBLE_STATUS_ASYNC_SCANNING);
      }
#endif
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING:
      // "Checking for malware..."
      return l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_BUBBLE_STATUS_LOCAL_DECRYPTING);
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED:
        // "Scan failed • Suspicious"
        return l10n_util::GetStringFUTF16(
            IDS_DOWNLOAD_BUBBLE_DOWNLOAD_STATUS_MESSAGE_WITH_SEPARATOR,
            l10n_util::GetStringUTF16(
                IDS_DOWNLOAD_BUBBLE_STATUS_DEEP_SCANNED_FAILED_UPDATED),
            l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_STATUS_SUSPICIOUS));
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_SCAN_FAILED:
      // "Blocked • Scan failed"
      return get_blocked_warning(IDS_DOWNLOAD_BUBBLE_STATUS_SCAN_FAILED);
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case download::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY:
    case download::DOWNLOAD_DANGER_TYPE_MAX:
      break;
  }

  return std::u16string();
}

std::u16string
DownloadUIModel::BubbleStatusTextBuilder::GetInProgressStatusText() const {
  DCHECK_EQ(DownloadItem::IN_PROGRESS, model_->GetState());

  std::u16string warning_status_text = GetBubbleWarningStatusText();
  if (!warning_status_text.empty())
    return warning_status_text;

  base::TimeDelta time_remaining;
  // time_remaining is only known if the download isn't paused, and it isn't
  // going to be rerouted to a web drive.
  bool time_remaining_known =
      (!model_->IsPaused() && model_->TimeRemaining(&time_remaining));

  // Indication of progress. (E.g.:"100/200 MB" or "100MB")
  std::u16string size_ratio = GetProgressSizesString();

  // If the detail message is "Paused" and the size_ratio is "100/120 MB", then
  // this returns "100/120 MB • Paused".
  auto get_size_ratio_string = [size_ratio](std::u16string detail_message) {
    return GetBubbleStatusMessageWithBytes(size_ratio, detail_message,
                                           /*is_active=*/false);
  };
  // If the detail message is "Opening in 10 seconds..." and the size_ratio is
  // "100/120 MB", then this returns "↓ 100/120 MB • Opening in 10 seconds...".
  auto get_active_download_size_ratio_string =
      [size_ratio](std::u16string detail_message) {
        return GetBubbleStatusMessageWithBytes(size_ratio, detail_message,
                                               /*is_active=*/true);
      };

  const auto completed_bytes = model_->GetCompletedBytes();
  const auto total_bytes = model_->GetTotalBytes();

  // If the detail message is "Done" and the total_bytes is "120 MB", then
  // this returns "120 MB • Done".
  auto get_total_string = [total_bytes](std::u16string detail_message) {
    return GetBubbleStatusMessageWithBytes(ui::FormatBytes(total_bytes),
                                           detail_message, /*is_active=*/false);
  };

  // The download is a CRX (app, extension, theme, ...) and it is being unpacked
  // and validated.
  if (model_->AllDataSaved() && model_->IsExtensionDownload()) {
    // "120 MB • Adding to Chrome..."
    return get_total_string(
        l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_CRX_INSTALL_RUNNING));
  }

  // A paused download: "100/120 MB • Paused"
  if (model_->IsPaused()) {
    return get_size_ratio_string(
        l10n_util::GetStringUTF16(IDS_DOWNLOAD_PROGRESS_PAUSED));
  }

  // A download scheduled to be opened when complete: "↓ 100/120 MB • Opening in
  // 10 seconds"
  if (model_->GetOpenWhenComplete()) {
    if (!time_remaining_known)
      // "100/120 MB • Opening when complete"
      return get_size_ratio_string(
          l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_OPEN_WHEN_COMPLETE));

    // "↓ 100/120 MB • Opening in 10 seconds..."
    return get_active_download_size_ratio_string(l10n_util::GetStringFUTF16(
        IDS_DOWNLOAD_STATUS_OPEN_IN,
        ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                               ui::TimeFormat::LENGTH_LONG, time_remaining)));
  }

  // In progress download with known time left: "↓ 100/120 MB • 10 seconds left"
  if (time_remaining_known) {
    return get_active_download_size_ratio_string(
        ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_REMAINING,
                               ui::TimeFormat::LENGTH_LONG, time_remaining));
  }

  if (completed_bytes == 0) {
    // "0/120 MB • Starting..."
    return get_size_ratio_string(
        l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_STARTING));
  } else if (completed_bytes < total_bytes || total_bytes == 0) {
    // In progress download with no known time left and non-zero completed
    // bytes: "100/120 MB • Resuming..." or "100 MB • Resuming..."
    return get_size_ratio_string(
        l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_STATUS_RESUMING));
  } else {
    // "120 MB • Done"
    return get_total_string(
        l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_STATUS_DONE));
  }
}

std::u16string
DownloadUIModel::StatusTextBuilderBase::GetCompletedRemovedOrSavedStatusText()
    const {
  if (model_->GetFileExternallyRemoved()) {
    return l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_REMOVED);
  }
  return std::u16string();
}

std::u16string DownloadUIModel::StatusTextBuilder::GetCompletedStatusText()
    const {
  return GetCompletedRemovedOrSavedStatusText();
}

std::u16string
DownloadUIModel::BubbleStatusTextBuilder::GetCompletedStatusText() const {
  std::u16string warning_status_text = GetBubbleWarningStatusText();
  if (!warning_status_text.empty())
    return warning_status_text;

  std::u16string status_text = GetCompletedRemovedOrSavedStatusText();
  if (!status_text.empty())
    return status_text;

  if (model_->GetEndTime().is_null()) {
    // Offline items have these null.
    return l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_STATUS_DONE);
  }
  std::u16string delta_str;
  if (model_->GetDangerType() ==
      download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE) {
    // "2 B • Scan is done"
    delta_str = l10n_util::GetStringUTF16(
        IDS_DOWNLOAD_BUBBLE_STATUS_DEEP_SCANNING_DONE_UPDATED);
  } else {
    base::TimeDelta time_elapsed = base::Time::Now() - model_->GetEndTime();
    // If less than 1 minute has passed since download completed: "2 B • Done"
    // Otherwise: e.g. "2 B • 3 minutes ago"
    // If the elapsed time is negative (could happen if the system time has
    // been adjusted backwards), also just display "2 B • Done".
    delta_str =
        time_elapsed.InMinutes() <= 0
            ? l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_STATUS_DONE)
            : ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                                     ui::TimeFormat::LENGTH_LONG, time_elapsed);
  }
  return GetBubbleStatusMessageWithBytes(
      ui::FormatBytes(model_->GetTotalBytes()), delta_str,
      /*is_active=*/false);
}

// To clarify variable / method names in methods below that help form failure
// status messages:
//                                long & descriptive / short & concise
// "Failed                      - <STATE_DESCRIPTION / STATE_MESSAGE>"
// "Fail to save to <WEB_DRIVE> - <STATE_DESCRIPTION / STATE_MESSAGE>"
// <                     DESCRIPTION/STATUS_TEXT                     >

std::u16string DownloadUIModel::StatusTextBuilderBase::GetFailStateMessage(
    offline_items_collection::FailState fail_state) const {
  std::u16string state_msg;
  return OfflineItemUtils::GetFailStateMessage(fail_state);
}

void DownloadUIModel::set_clock_for_testing(base::Clock* clock) {
  clock_ = clock;
}

void DownloadUIModel::set_status_text_builder_for_testing(bool for_bubble) {
  if (for_bubble) {
    status_text_builder_ =
        std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>();
  } else {
    status_text_builder_ =
        std::make_unique<DownloadUIModel::StatusTextBuilder>();
  }
  status_text_builder_->SetModel(this);
}

std::u16string DownloadUIModel::GetInterruptDescription() const {
  const auto fail_state = GetLastFailState();
  std::u16string state_description = FailStateDescription(fail_state);
  return l10n_util::GetStringFUTF16(IDS_DOWNLOAD_STATUS_INTERRUPTED,
                                    state_description);
}

std::u16string DownloadUIModel::GetHistoryPageStatusText() const {
  if (GetLastFailState() == FailState::SERVER_FAILED) {
    // Display the full error description in case of server failure.
    return GetInterruptDescription();
  }
  return GetStatusText();
}

void DownloadUIModel::StatusTextBuilderBase::SetModel(DownloadUIModel* model) {
  model_ = model;
}

std::u16string DownloadUIModel::StatusTextBuilderBase::GetInterruptedStatusText(
    FailState fail_state) const {
  auto state_msg = GetFailStateMessage(fail_state);
  return l10n_util::GetStringFUTF16(IDS_DOWNLOAD_STATUS_INTERRUPTED, state_msg);
}

std::u16string
DownloadUIModel::BubbleStatusTextBuilder::GetInterruptedStatusText(
    FailState fail_state) const {
  int string_id = IDS_DOWNLOAD_BUBBLE_INTERRUPTED_STATUS_WRONG;

  switch (fail_state) {
    case FailState::FILE_ACCESS_DENIED:
      string_id = IDS_DOWNLOAD_BUBBLE_INTERRUPTED_STATUS_NEEDS_PERMISSION;
      break;
    case FailState::FILE_NO_SPACE:
      string_id = IDS_DOWNLOAD_BUBBLE_INTERRUPTED_STATUS_DISK_FULL;
      break;
    case FailState::FILE_NAME_TOO_LONG:
      string_id = IDS_DOWNLOAD_BUBBLE_INTERRUPTED_STATUS_PATH_TOO_LONG;
      break;
    case FailState::FILE_TOO_LARGE:
      string_id = IDS_DOWNLOAD_BUBBLE_INTERRUPTED_STATUS_FILE_TOO_LARGE;
      break;
    case FailState::FILE_VIRUS_INFECTED:
      string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS_VIRUS;
      break;
    case FailState::FILE_BLOCKED:
      string_id = IDS_DOWNLOAD_BUBBLE_INTERRUPTED_STATUS_BLOCKED_ORGANIZATION;
      break;
    case FailState::FILE_SECURITY_CHECK_FAILED:
      string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS_SECURITY_CHECK_FAILED;
      break;
    case FailState::FILE_TOO_SHORT:
      string_id = IDS_DOWNLOAD_BUBBLE_INTERRUPTED_STATUS_WRONG;
      break;
    case FailState::FILE_SAME_AS_SOURCE:
      string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS_FILE_SAME_AS_SOURCE;
      break;
    case FailState::NETWORK_INVALID_REQUEST:
    case FailState::NETWORK_FAILED:
    case FailState::NETWORK_INSTABILITY:
    case FailState::NETWORK_TIMEOUT:
    case FailState::NETWORK_DISCONNECTED:
      string_id = IDS_DOWNLOAD_BUBBLE_INTERRUPTED_STATUS_NETWORK_ERROR;
      break;
    case FailState::NETWORK_SERVER_DOWN:
    case FailState::SERVER_FAILED:
    case FailState::SERVER_CERT_PROBLEM:
    case FailState::SERVER_UNREACHABLE:
      string_id = IDS_DOWNLOAD_BUBBLE_INTERRUPTED_STATUS_SITE_UNAVAILABLE;
      break;
    case FailState::SERVER_UNAUTHORIZED:
    case FailState::SERVER_FORBIDDEN:
    case FailState::SERVER_BAD_CONTENT:
      string_id = IDS_DOWNLOAD_BUBBLE_INTERRUPTED_STATUS_FILE_UNAVAILABLE;
      break;
    case FailState::USER_CANCELED:
      string_id = IDS_DOWNLOAD_STATUS_CANCELLED;
      break;

    case FailState::FILE_TRANSIENT_ERROR:
    case FailState::USER_SHUTDOWN:
    case FailState::CRASH:
    case FailState::SERVER_CONTENT_LENGTH_MISMATCH:
      string_id = IDS_DOWNLOAD_BUBBLE_INTERRUPTED_STATUS_FILE_UNFINISHED;
      break;
    case FailState::CANNOT_DOWNLOAD:
    case FailState::SERVER_NO_RANGE:
    case FailState::SERVER_CROSS_ORIGIN_REDIRECT:
    case FailState::FILE_FAILED:
    case FailState::FILE_HASH_MISMATCH:
      string_id = IDS_DOWNLOAD_BUBBLE_INTERRUPTED_STATUS_WRONG;
      break;
    case FailState::NO_FAILURE:
      NOTREACHED_IN_MIGRATION();
  }

  return l10n_util::GetStringUTF16(string_id);
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
void DownloadUIModel::CompleteSafeBrowsingScan() {}
void DownloadUIModel::ReviewScanningVerdict(
    content::WebContents* web_contents) {}
#endif

bool DownloadUIModel::ShouldShowDropdown() const {
  return true;
}

void DownloadUIModel::DetermineAndSetShouldPreferOpeningInBrowser(
    const base::FilePath& target_path,
    bool is_filetype_handled_safely) {}

std::u16string DownloadUIModel::GetInProgressAccessibleAlertText() const {
  // Prefer to announce the time remaining, if known.
  base::TimeDelta remaining;
  if (TimeRemaining(&remaining)) {
    // If complete, skip this round: a completion status update is coming soon.
    if (remaining.is_zero())
      return std::u16string();

    const std::u16string remaining_string =
        ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_REMAINING,
                               ui::TimeFormat::LENGTH_SHORT, remaining);
    return l10n_util::GetStringFUTF16(
        IDS_DOWNLOAD_STATUS_TIME_REMAINING_ACCESSIBLE_ALERT,
        GetFileNameToReportUser().LossyDisplayName(), remaining_string);
  }

  // Time remaining is unknown, try to announce percent remaining.
  if (PercentComplete() > 0) {
    DCHECK_LE(PercentComplete(), 100);
    return l10n_util::GetStringFUTF16(
        IDS_DOWNLOAD_STATUS_PERCENT_COMPLETE_ACCESSIBLE_ALERT,
        GetFileNameToReportUser().LossyDisplayName(),
        base::FormatNumber(100 - PercentComplete()));
  }

  // Percent remaining is also unknown, announce bytes to download.
  return l10n_util::GetStringFUTF16(
      IDS_DOWNLOAD_STATUS_IN_PROGRESS_ACCESSIBLE_ALERT,
      ui::FormatBytes(GetTotalBytes()),
      GetFileNameToReportUser().LossyDisplayName());
}

bool DownloadUIModel::IsTopLevelEncryptedArchive() const {
  return false;
}
