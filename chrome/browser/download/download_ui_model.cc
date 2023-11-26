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
#include "components/safe_browsing/core/common/features.h"
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
      NOTREACHED();
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
  // TODO(crbug.com/1409167): Avoid calling the deprecated function.
  const GURL url = GetOriginalURL().DeprecatedGetOriginAsURL();
  return url.is_valid()
             ? url_formatter::ElideUrl(url, font_list, available_pixel_width)
             : GetStatusText();
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
      NOTREACHED();
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
      // TODO(crbug.com/1491184): Implement UX for this danger type.
      NOTREACHED();
      break;
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_UNSUPPORTED_FILETYPE:
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
  NOTREACHED();
  return ContentId();
}

Profile* DownloadUIModel::profile() const {
  NOTREACHED();
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

absl::optional<base::Time> DownloadUIModel::GetEphemeralWarningUiShownTime()
    const {
  return absl::optional<base::Time>();
}

void DownloadUIModel::SetEphemeralWarningUiShownTime(
    absl::optional<base::Time> time) {}

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
  NOTREACHED();
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
      NOTREACHED();
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
  NOTREACHED();
  return false;
}

bool DownloadUIModel::IsCommandChecked(
    const DownloadCommands* download_commands,
    DownloadCommands::Command command) const {
  switch (command) {
    case DownloadCommands::OPEN_WHEN_COMPLETE:
    case DownloadCommands::ALWAYS_OPEN_TYPE:
      NOTREACHED();
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
      NOTREACHED();
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
      NOTREACHED();
      break;
    case DownloadCommands::LEARN_MORE_INTERRUPTED:
      download_commands->GetBrowser()->OpenURL(content::OpenURLParams(
          download_commands->GetLearnMoreURLForInterruptedDownload(),
          content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui::PAGE_TRANSITION_LINK, false));
      break;
    case DownloadCommands::LEARN_MORE_INSECURE_DOWNLOAD:
      download_commands->GetBrowser()->OpenURL(content::OpenURLParams(
          GURL(chrome::kInsecureDownloadBlockingLearnMoreUrl),
          content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui::PAGE_TRANSITION_LINK, false));
      break;
    case DownloadCommands::LEARN_MORE_DOWNLOAD_BLOCKED:
      download_commands->GetBrowser()->OpenURL(content::OpenURLParams(
          google_util::AppendGoogleLocaleParam(
              GURL(chrome::kDownloadBlockedLearnMoreURL),
              g_browser_process->GetApplicationLocale()),
          content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui::PAGE_TRANSITION_LINK, false));
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
  }
}

DownloadUIModel::BubbleUIInfo::SubpageButton::SubpageButton(
    DownloadCommands::Command command,
    std::u16string label,
    bool is_prominent,
    absl::optional<ui::ColorId> color)
    : command(command),
      label(label),
      is_prominent(is_prominent),
      color(color) {}
DownloadUIModel::BubbleUIInfo::QuickAction::QuickAction(
    DownloadCommands::Command command,
    const std::u16string& hover_text,
    const gfx::VectorIcon* icon)
    : command(command), hover_text(hover_text), icon(icon) {}
DownloadUIModel::BubbleUIInfo::BubbleUIInfo() = default;
DownloadUIModel::BubbleUIInfo::~BubbleUIInfo() = default;
DownloadUIModel::BubbleUIInfo::BubbleUIInfo(const BubbleUIInfo& rhs) = default;
DownloadUIModel::BubbleUIInfo& DownloadUIModel::BubbleUIInfo::AddSubpageSummary(
    const std::u16string& summary) {
  CHECK(!summary.empty());  // An empty summary is interpreted as no subpage
  warning_summary = summary;
  return *this;
}
DownloadUIModel::BubbleUIInfo&
DownloadUIModel::BubbleUIInfo::AddSubpageSecondaryIconAndText(
    const gfx::VectorIcon& icon,
    const std::u16string& secondary_text) {
  warning_secondary_icon = &icon;
  warning_secondary_text = secondary_text;
  return *this;
}
DownloadUIModel::BubbleUIInfo& DownloadUIModel::BubbleUIInfo::AddProgressBar() {
  has_progress_bar = true;
  return *this;
}
DownloadUIModel::BubbleUIInfo& DownloadUIModel::BubbleUIInfo::AddIconAndColor(
    const gfx::VectorIcon& vector_icon,
    ui::ColorId color_id) {
  secondary_color = color_id;
  icon_model_override = &vector_icon;
  return *this;
}
DownloadUIModel::BubbleUIInfo&
DownloadUIModel::BubbleUIInfo::AddSecondaryTextColor(ui::ColorId color_id) {
  secondary_text_color = color_id;
  return *this;
}
DownloadUIModel::BubbleUIInfo& DownloadUIModel::BubbleUIInfo::AddPrimaryButton(
    DownloadCommands::Command command) {
  primary_button_command = command;
  return *this;
}
DownloadUIModel::BubbleUIInfo&
DownloadUIModel::BubbleUIInfo::AddPrimarySubpageButton(
    const std::u16string& label,
    DownloadCommands::Command command) {
  // The subpage of the bubble supports at most 2 buttons. The primary one must
  // come first, then the secondary.
  CHECK(subpage_buttons.empty());
  subpage_buttons.emplace_back(command, label, /*is_prominent=*/true);
  return *this;
}
DownloadUIModel::BubbleUIInfo&
DownloadUIModel::BubbleUIInfo::AddSecondarySubpageButton(
    const std::u16string& label,
    DownloadCommands::Command command,
    absl::optional<ui::ColorId> color) {
  // The subpage of the bubble supports at most 2 buttons. The primary one must
  // come first, then the secondary.
  CHECK(subpage_buttons.size() == 1);
  subpage_buttons.emplace_back(command, label, /*is_prominent=*/false, color);
  return *this;
}

DownloadUIModel::BubbleUIInfo&
DownloadUIModel::BubbleUIInfo::SetProgressBarLooping() {
  is_progress_bar_looping = true;
  return *this;
}

DownloadUIModel::BubbleUIInfo& DownloadUIModel::BubbleUIInfo::AddQuickAction(
    DownloadCommands::Command command,
    const std::u16string& label,
    const gfx::VectorIcon* icon) {
  quick_actions.emplace_back(command, label, icon);
  return *this;
}

DownloadUIModel::BubbleUIInfo& DownloadUIModel::BubbleUIInfo::AddLearnMoreLink(
    int label_text_id,
    int link_text_id,
    DownloadCommands::Command command) {
  size_t link_start_offset = 0;
  std::u16string link_text = l10n_util::GetStringUTF16(link_text_id);
  std::u16string label_and_link_text =
      l10n_util::GetStringFUTF16(label_text_id, link_text, &link_start_offset);
  learn_more_link = LabelWithLink{
      label_and_link_text, LabelWithLink::LinkedRange{
                               link_start_offset, link_text.length(), command}};
  return *this;
}

DownloadUIModel::BubbleUIInfo& DownloadUIModel::BubbleUIInfo::AddLearnMoreLink(
    const std::u16string& link_text,
    DownloadCommands::Command command) {
  learn_more_link = LabelWithLink{
      link_text, LabelWithLink::LinkedRange{0u, link_text.length(), command}};
  return *this;
}

DownloadUIModel::BubbleUIInfo&
DownloadUIModel::BubbleUIInfo::DisableMainButton() {
  main_button_enabled = false;
  return *this;
}

// static
DownloadUIModel::BubbleUIInfo DownloadUIModel::BubbleUIInfo::DangerousUiPattern(
    const std::u16string& subpage_summary) {
  return DownloadUIModel::BubbleUIInfo()
      .AddSubpageSummary(subpage_summary)
      .AddLearnMoreLink(
          l10n_util::GetStringUTF16(
              IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_WARNING_BLOCKED_LEARN_MORE_LINK),
          DownloadCommands::Command::LEARN_MORE_DOWNLOAD_BLOCKED)
      .AddIconAndColor(features::IsChromeRefresh2023()
                           ? vector_icons::kDangerousChromeRefreshIcon
                           : vector_icons::kDangerousIcon,
                       kColorDownloadItemIconDangerous)
      .AddSecondaryTextColor(kColorDownloadItemTextDangerous)
      .AddPrimarySubpageButton(
          l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_DELETE_FROM_HISTORY),
          DownloadCommands::Command::DISCARD);
}

// static
DownloadUIModel::BubbleUIInfo
DownloadUIModel::BubbleUIInfo::SuspiciousUiPattern(
    const std::u16string& subpage_summary,
    const std::u16string& secondary_subpage_button_label) {
  return DownloadUIModel::BubbleUIInfo()
      .AddSubpageSummary(subpage_summary)
      .AddLearnMoreLink(
          l10n_util::GetStringUTF16(
              IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_WARNING_BLOCKED_LEARN_MORE_LINK),
          DownloadCommands::Command::LEARN_MORE_DOWNLOAD_BLOCKED)
      .AddIconAndColor(features::IsChromeRefresh2023()
                           ? kDownloadWarningIcon
                           : vector_icons::kNotSecureWarningIcon,
                       kColorDownloadItemIconWarning)
      .AddSecondaryTextColor(kColorDownloadItemTextWarning)
      .AddPrimarySubpageButton(
          l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_DELETE_FROM_HISTORY),
          DownloadCommands::Command::DISCARD)
      .AddSecondarySubpageButton(secondary_subpage_button_label,
                                 DownloadCommands::Command::KEEP);
}

ui::ColorId DownloadUIModel::BubbleUIInfo::GetColorForSecondaryText() const {
  return secondary_text_color.value_or(secondary_color);
}

bool DownloadUIModel::BubbleUIInfo::HasSubpage() const {
  return !warning_summary.empty();
}

DownloadUIModel::BubbleUIInfo DownloadUIModel::GetBubbleUIInfoForInterrupted(
    FailState fail_state) const {
  // Only handle danger types that are terminated in the interrupted state in
  // this function. The other danger types are handled in
  // `GetBubbleUIInfoForInProgressOrComplete`.
  switch (GetDangerType()) {
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
      return DownloadUIModel::BubbleUIInfo()
          .AddSubpageSummary(l10n_util::GetStringUTF16(
              IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_ENCRYPTED))
          .AddIconAndColor(features::IsChromeRefresh2023()
                               ? views::kInfoChromeRefreshIcon
                               : views::kInfoIcon,
                           kColorDownloadItemIconDangerous);
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
      return DownloadUIModel::BubbleUIInfo()
          .AddSubpageSummary(l10n_util::GetStringUTF16(
              IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_TOO_BIG))
          .AddIconAndColor(features::IsChromeRefresh2023()
                               ? views::kInfoChromeRefreshIcon
                               : views::kInfoIcon,
                           kColorDownloadItemIconDangerous);
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK: {
      if (enterprise_connectors::ShouldPromptReviewForDownload(
              profile(), GetDangerType())) {
        return DownloadUIModel::BubbleUIInfo()
            .AddIconAndColor(features::IsChromeRefresh2023()
                                 ? kDownloadWarningIcon
                                 : vector_icons::kNotSecureWarningIcon,
                             kColorDownloadItemIconDangerous)
            .AddPrimaryButton(DownloadCommands::Command::REVIEW);
      } else {
        return DownloadUIModel::BubbleUIInfo()
            .AddSubpageSummary(l10n_util::GetStringUTF16(
                IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_SENSITIVE_CONTENT_BLOCK))
            .AddIconAndColor(features::IsChromeRefresh2023()
                                 ? views::kInfoChromeRefreshIcon
                                 : views::kInfoIcon,
                             kColorDownloadItemIconDangerous);
      }
    }
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_UNSUPPORTED_FILETYPE:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case download::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY:
    case download::DOWNLOAD_DANGER_TYPE_MAX:
      break;
  }

  switch (fail_state) {
    case FailState::FILE_BLOCKED:
      return DownloadUIModel::BubbleUIInfo()
          .AddSubpageSummary(l10n_util::GetStringUTF16(
              IDS_DOWNLOAD_BUBBLE_INTERRUPTED_SUBPAGE_SUMMARY_BLOCKED_ORGANIZATION))
          .AddIconAndColor(features::IsChromeRefresh2023()
                               ? views::kInfoChromeRefreshIcon
                               : views::kInfoIcon,
                           kColorDownloadItemIconDangerous);
    case FailState::FILE_NAME_TOO_LONG:
      return DownloadUIModel::BubbleUIInfo()
          .AddSubpageSummary(l10n_util::GetStringUTF16(
              IDS_DOWNLOAD_BUBBLE_INTERRUPTED_SUBPAGE_SUMMARY_PATH_TOO_LONG))
          .AddIconAndColor(features::IsChromeRefresh2023()
                               ? vector_icons::kFileDownloadOffChromeRefreshIcon
                               : vector_icons::kFileDownloadOffIcon,
                           kColorDownloadItemIconDangerous);
    case FailState::FILE_NO_SPACE:
      return DownloadUIModel::BubbleUIInfo()
          .AddSubpageSummary(l10n_util::GetStringUTF16(
              IDS_DOWNLOAD_BUBBLE_INTERRUPTED_SUBPAGE_SUMMARY_DISK_FULL))
          .AddIconAndColor(features::IsChromeRefresh2023()
                               ? vector_icons::kFileDownloadOffChromeRefreshIcon
                               : vector_icons::kFileDownloadOffIcon,
                           kColorDownloadItemIconDangerous);
    case FailState::SERVER_UNAUTHORIZED:
      return DownloadUIModel::BubbleUIInfo()
          .AddSubpageSummary(l10n_util::GetStringUTF16(
              IDS_DOWNLOAD_BUBBLE_INTERRUPTED_SUBPAGE_SUMMARY_FILE_UNAVAILABLE))
          .AddIconAndColor(features::IsChromeRefresh2023()
                               ? vector_icons::kFileDownloadOffChromeRefreshIcon
                               : vector_icons::kFileDownloadOffIcon,
                           kColorDownloadItemIconDangerous);
    // No Retry in these cases.
    case FailState::FILE_TOO_LARGE:
    case FailState::FILE_VIRUS_INFECTED:
    case FailState::FILE_SECURITY_CHECK_FAILED:
    case FailState::FILE_ACCESS_DENIED:
    case FailState::SERVER_FORBIDDEN:
    case FailState::FILE_SAME_AS_SOURCE:
    case FailState::SERVER_BAD_CONTENT:
      return DownloadUIModel::BubbleUIInfo().AddIconAndColor(
          features::IsChromeRefresh2023()
              ? vector_icons::kFileDownloadOffChromeRefreshIcon
              : vector_icons::kFileDownloadOffIcon,
          kColorDownloadItemIconDangerous);
    // Try resume if possible or retry if not in these cases, and in the default
    // case.
    case FailState::NETWORK_INVALID_REQUEST:
    case FailState::NETWORK_FAILED:
    case FailState::NETWORK_TIMEOUT:
    case FailState::NETWORK_DISCONNECTED:
    case FailState::NETWORK_SERVER_DOWN:
    case FailState::FILE_TRANSIENT_ERROR:
    case FailState::USER_SHUTDOWN:
    case FailState::CRASH:
    case FailState::SERVER_CONTENT_LENGTH_MISMATCH:
    case FailState::SERVER_NO_RANGE:
    case FailState::SERVER_CROSS_ORIGIN_REDIRECT:
    case FailState::FILE_FAILED:
    case FailState::FILE_HASH_MISMATCH:
    case FailState::SERVER_FAILED:
    case FailState::SERVER_CERT_PROBLEM:
    case FailState::SERVER_UNREACHABLE:
    case FailState::FILE_TOO_SHORT:
      break;
    // Not possible because the USER_CANCELED fail state does not allow a call
    // into this function
    case FailState::USER_CANCELED:
    // Deprecated
    case FailState::NETWORK_INSTABILITY:
    case FailState::CANNOT_DOWNLOAD:
      NOTREACHED();
      break;
    case FailState::NO_FAILURE:
      return DownloadUIModel::BubbleUIInfo();
  }

  DownloadUIModel::BubbleUIInfo bubble_ui_info =
      DownloadUIModel::BubbleUIInfo()
          .AddIconAndColor(features::IsChromeRefresh2023()
                               ? vector_icons::kFileDownloadOffChromeRefreshIcon
                               : vector_icons::kFileDownloadOffIcon,
                           kColorDownloadItemIconDangerous)
          .AddPrimaryButton(CanResume() ? DownloadCommands::Command::RESUME
                                        : DownloadCommands::Command::RETRY);
  return bubble_ui_info;
}

DownloadUIModel::BubbleUIInfo
DownloadUIModel::GetBubbleUIInfoForInProgressOrComplete() const {
  switch (GetInsecureDownloadStatus()) {
    case download::DownloadItem::InsecureDownloadStatus::BLOCK:
    case download::DownloadItem::InsecureDownloadStatus::WARN:
      // The insecure warning uses the suspicious warning pattern but has a
      // primary button to keep the file.
      return DownloadUIModel::BubbleUIInfo::SuspiciousUiPattern(
                 l10n_util::GetStringUTF16(
                     IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_WARNING_INSECURE),
                 l10n_util::GetStringUTF16(
                     IDS_DOWNLOAD_BUBBLE_CONTINUE_INSECURE_FILE))
          .AddPrimaryButton(DownloadCommands::Command::KEEP);
    case download::DownloadItem::InsecureDownloadStatus::UNKNOWN:
    case download::DownloadItem::InsecureDownloadStatus::SAFE:
    case download::DownloadItem::InsecureDownloadStatus::VALIDATED:
    case download::DownloadItem::InsecureDownloadStatus::SILENT_BLOCK:
      break;
  }

  if (enterprise_connectors::ShouldPromptReviewForDownload(profile(),
                                                           GetDangerType())) {
    switch (GetDangerType()) {
      case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
        return DownloadUIModel::BubbleUIInfo()
            .AddIconAndColor(features::IsChromeRefresh2023()
                                 ? vector_icons::kDangerousChromeRefreshIcon
                                 : vector_icons::kDangerousIcon,
                             kColorDownloadItemIconDangerous)
            .AddPrimaryButton(DownloadCommands::Command::REVIEW);
      case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
        return DownloadUIModel::BubbleUIInfo()
            .AddIconAndColor(features::IsChromeRefresh2023()
                                 ? kDownloadWarningIcon
                                 : vector_icons::kNotSecureWarningIcon,
                             kColorDownloadItemIconWarning)
            .AddSecondaryTextColor(kColorDownloadItemTextWarning)
            .AddPrimaryButton(DownloadCommands::Command::REVIEW);
      case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
        return DownloadUIModel::BubbleUIInfo()
            .AddIconAndColor(features::IsChromeRefresh2023()
                                 ? views::kInfoChromeRefreshIcon
                                 : views::kInfoIcon,
                             kColorDownloadItemIconWarning)
            .AddSecondaryTextColor(kColorDownloadItemTextWarning)
            .AddPrimaryButton(DownloadCommands::Command::REVIEW);
      default:
        break;
    }
  }

  if (ShouldShowTailoredWarning()) {
    return GetBubbleUIInfoForTailoredWarning();
  }

  DownloadUIModel::BubbleUIInfo ui_info;
  switch (GetDangerType()) {
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      if (IsExtensionDownload()) {
        if (base::FeatureList::IsEnabled(
                safe_browsing::kImprovedDownloadBubbleWarnings)) {
          return DownloadUIModel::BubbleUIInfo::SuspiciousUiPattern(
              l10n_util::GetStringFUTF16(
                  IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_UNKNOWN_SOURCE,
                  l10n_util::GetStringUTF16(IDS_EXTENSION_WEB_STORE_TITLE)),
              l10n_util::GetStringUTF16(
                  IDS_DOWNLOAD_BUBBLE_CONTINUE_SUSPICIOUS_FILE));
        }
        return DownloadUIModel::BubbleUIInfo()
            .AddSubpageSummary(l10n_util::GetStringFUTF16(
                IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_UNKNOWN_SOURCE,
                l10n_util::GetStringUTF16(IDS_EXTENSION_WEB_STORE_TITLE)))
            .AddIconAndColor(features::IsChromeRefresh2023()
                                 ? kDownloadWarningIcon
                                 : vector_icons::kNotSecureWarningIcon,
                             kColorDownloadItemIconWarning)
            .AddSecondaryTextColor(kColorDownloadItemTextWarning)
            .AddPrimarySubpageButton(
                l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_DELETE),
                DownloadCommands::Command::DISCARD)
            .AddSecondarySubpageButton(
                l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_CONTINUE),
                DownloadCommands::Command::KEEP, kColorDownloadItemTextWarning);
      } else {
        if (base::FeatureList::IsEnabled(
                safe_browsing::kImprovedDownloadBubbleWarnings)) {
          if (ShouldShowWarningForNoSafeBrowsing(profile())) {
            return GetBubbleUIInfoForFileTypeWarningNoSafeBrowsing();
          }
          return DownloadUIModel::BubbleUIInfo::SuspiciousUiPattern(
              l10n_util::GetStringUTF16(
                  IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_WARNING_DANGEROUS_FILE_TYPE),
              WasSafeBrowsingVerdictObtained(GetDownloadItem())
                  ? l10n_util::GetStringUTF16(
                        IDS_DOWNLOAD_BUBBLE_CONTINUE_SUSPICIOUS_FILE)
                  : l10n_util::GetStringUTF16(
                        IDS_DOWNLOAD_BUBBLE_CONTINUE_UNVERIFIED_FILE));
        }
        return DownloadUIModel::BubbleUIInfo()
            .AddSubpageSummary(
                l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_DANGEROUS_FILE))
            .AddIconAndColor(features::IsChromeRefresh2023()
                                 ? kDownloadWarningIcon
                                 : vector_icons::kNotSecureWarningIcon,
                             ui::kColorSecondaryForeground)
            .AddPrimaryButton(DownloadCommands::Command::KEEP)
            .AddPrimarySubpageButton(
                l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_DELETE),
                DownloadCommands::Command::DISCARD)
            .AddSecondarySubpageButton(
                l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_CONTINUE),
                DownloadCommands::Command::KEEP, ui::kColorSecondaryForeground);
      }
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
      if (base::FeatureList::IsEnabled(
              safe_browsing::kImprovedDownloadBubbleWarnings)) {
        return DownloadUIModel::BubbleUIInfo::DangerousUiPattern(
            l10n_util::GetStringUTF16(
                IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_WARNING_DANGEROUS));
      } else {
        ui_info
            .AddSubpageSummary(l10n_util::GetStringUTF16(
                IDS_DOWNLOAD_BUBBLE_MALICIOUS_URL_BLOCKED))
            .AddIconAndColor(features::IsChromeRefresh2023()
                                 ? vector_icons::kDangerousChromeRefreshIcon
                                 : vector_icons::kDangerousIcon,
                             kColorDownloadItemIconDangerous)
            .AddPrimaryButton(DownloadCommands::Command::DISCARD)
            .AddPrimarySubpageButton(
                l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_DELETE),
                DownloadCommands::Command::DISCARD);
        return ui_info;
      }

    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      if (base::FeatureList::IsEnabled(
              safe_browsing::kImprovedDownloadBubbleWarnings)) {
        return DownloadUIModel::BubbleUIInfo::DangerousUiPattern(
            l10n_util::GetStringUTF16(
                IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_WARNING_DECEPTIVE));
      } else {
        ui_info =
            DownloadUIModel::BubbleUIInfo()
                .AddSubpageSummary(l10n_util::GetStringUTF16(
                    IDS_DOWNLOAD_BUBBLE_MALICIOUS_URL_BLOCKED))
                .AddIconAndColor(features::IsChromeRefresh2023()
                                     ? kDownloadWarningIcon
                                     : vector_icons::kNotSecureWarningIcon,
                                 kColorDownloadItemIconWarning)
                .AddSecondaryTextColor(kColorDownloadItemTextWarning)
                .AddPrimaryButton(DownloadCommands::Command::DISCARD)
                .AddPrimarySubpageButton(
                    l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_DELETE),
                    DownloadCommands::Command::DISCARD);
        return ui_info;
      }

    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
      if (base::FeatureList::IsEnabled(
              safe_browsing::kImprovedDownloadBubbleWarnings)) {
        // This is the same as the message for
        // DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT, etc.
        // TODO(chlily): Combine these cases after other conditionals are
        // removed.
        return DownloadUIModel::BubbleUIInfo::DangerousUiPattern(
            l10n_util::GetStringUTF16(
                IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_WARNING_DANGEROUS));
      } else {
        ui_info =
            DownloadUIModel::BubbleUIInfo()
                .AddSubpageSummary(l10n_util::GetStringUTF16(
                    IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_MALWARE))
                .AddIconAndColor(features::IsChromeRefresh2023()
                                     ? kDownloadWarningIcon
                                     : vector_icons::kNotSecureWarningIcon,
                                 kColorDownloadItemIconDangerous)
                .AddPrimaryButton(DownloadCommands::Command::DISCARD)
                .AddPrimarySubpageButton(
                    l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_DELETE),
                    DownloadCommands::Command::DISCARD);
        return ui_info;
      }
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT: {
      bool request_ap_verdicts = false;
#if BUILDFLAG(FULL_SAFE_BROWSING)
      request_ap_verdicts =
          safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
              profile())
              ->IsUnderAdvancedProtection();
#endif
      if (request_ap_verdicts) {
        return DownloadUIModel::BubbleUIInfo()
            .AddSubpageSummary(l10n_util::GetStringUTF16(
                IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_ADVANCED_PROTECTION))
            .AddIconAndColor(features::IsChromeRefresh2023()
                                 ? kDownloadWarningIcon
                                 : vector_icons::kNotSecureWarningIcon,
                             kColorDownloadItemIconWarning)
            .AddSecondaryTextColor(kColorDownloadItemTextWarning)
            .AddPrimarySubpageButton(
                l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_DELETE),
                DownloadCommands::Command::DISCARD)
            .AddSecondarySubpageButton(
                l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_CONTINUE),
                DownloadCommands::Command::KEEP, kColorDownloadItemTextWarning);
      } else {
        if (base::FeatureList::IsEnabled(
                safe_browsing::kImprovedDownloadBubbleWarnings)) {
          return DownloadUIModel::BubbleUIInfo::SuspiciousUiPattern(
              l10n_util::GetStringUTF16(
                  IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_WARNING_UNCOMMON_FILE),
              l10n_util::GetStringUTF16(
                  IDS_DOWNLOAD_BUBBLE_CONTINUE_SUSPICIOUS_FILE));
        }
        return DownloadUIModel::BubbleUIInfo()
            .AddSubpageSummary(l10n_util::GetStringUTF16(
                IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_UNCOMMON_FILE))
            .AddIconAndColor(features::IsChromeRefresh2023()
                                 ? kDownloadWarningIcon
                                 : vector_icons::kNotSecureWarningIcon,
                             kColorDownloadItemIconWarning)
            .AddSecondaryTextColor(kColorDownloadItemTextWarning)
            .AddPrimaryButton(DownloadCommands::Command::DISCARD)
            .AddPrimarySubpageButton(
                l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_DELETE),
                DownloadCommands::Command::DISCARD)
            .AddSecondarySubpageButton(
                l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_CONTINUE),
                DownloadCommands::Command::KEEP, kColorDownloadItemTextWarning);
      }
    }
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
      return DownloadUIModel::BubbleUIInfo()
          .AddSubpageSummary(l10n_util::GetStringUTF16(
              IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_SENSITIVE_CONTENT_WARNING))
          .AddIconAndColor(features::IsChromeRefresh2023()
                               ? views::kInfoChromeRefreshIcon
                               : views::kInfoIcon,
                           kColorDownloadItemIconWarning)
          .AddSecondaryTextColor(kColorDownloadItemTextWarning)
          .AddPrimaryButton(DownloadCommands::Command::DISCARD)
          .AddPrimarySubpageButton(
              l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_DELETE),
              DownloadCommands::Command::DISCARD)
          .AddSecondarySubpageButton(
              l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_CONTINUE),
              DownloadCommands::Command::KEEP, kColorDownloadItemTextWarning);
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING: {
      ui_info = DownloadUIModel::BubbleUIInfo()
                    .AddIconAndColor(features::IsChromeRefresh2023()
                                         ? kDownloadWarningIcon
                                         : vector_icons::kNotSecureWarningIcon,
                                     kColorDownloadItemIconWarning)
                    .AddSecondaryTextColor(kColorDownloadItemTextWarning);
      std::u16string subpage_text = l10n_util::GetStringFUTF16(
          IsEncryptedArchive()
              ? IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_DEEP_SCANNING_PROMPT_ENCRYPTED_ARCHIVE
              : IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_DEEP_SCANNING_PROMPT_UPDATED,
          u"\n\n");
      ui_info.AddSubpageSummary(subpage_text)
          .AddPrimarySubpageButton(
              l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_SCAN_UPDATED),
              DownloadCommands::Command::DEEP_SCAN)
          .AddSecondarySubpageButton(
              l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_OPEN_UPDATED),
              DownloadCommands::Command::BYPASS_DEEP_SCANNING);
      return ui_info;
    }
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING: {
      ui_info = DownloadUIModel::BubbleUIInfo()
                    .AddIconAndColor(features::IsChromeRefresh2023()
                                         ? kDownloadWarningIcon
                                         : vector_icons::kNotSecureWarningIcon,
                                     kColorDownloadItemIconWarning)
                    .AddSecondaryTextColor(kColorDownloadItemTextWarning);
      std::u16string subpage_text = l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_DEEP_SCANNING_PROMPT_LOCAL_DECRYPTION,
          u"\n\n");
      ui_info.AddSubpageSummary(subpage_text)
          .AddPrimarySubpageButton(
              l10n_util::GetStringUTF16(
                  IDS_DOWNLOAD_BUBBLE_ACCEPT_LOCAL_DECRYPTION),
              // These download commands are not propagated
              // to the DownloadItem. Instead they are handled specially in
              // DownloadBubbleSecurityView::ProcessButtonClick. That makes it
              // okay that the we aren't really prompting for a deep scan.
              // TODO(crbug/1482901): Remove this by creating a dedicated View
              // for the local decryption prompt which directly handles teh
              // button presses.
              DownloadCommands::Command::DEEP_SCAN)
          .AddSecondarySubpageButton(
              l10n_util::GetStringUTF16(
                  IDS_DOWNLOAD_BUBBLE_BYPASS_LOCAL_DECRYPTION),
              DownloadCommands::Command::KEEP);
      return ui_info;
    }
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
      ui_info =
          DownloadUIModel::BubbleUIInfo()
              .AddProgressBar()
              .SetProgressBarLooping()
              .AddSubpageSummary(l10n_util::GetStringUTF16(
                  IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_ASYNC_SCANNING))
              .AddSubpageSecondaryIconAndText(
                  vector_icons::kDocumentScannerIcon,
                  download::DoesDownloadConnectorBlock(profile(), GetURL())
                      ? l10n_util::GetStringUTF16(
                            IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_ASYNC_SCANNING_ENTERPRISE_SECONDARY)
                      : l10n_util::GetStringUTF16(
                            IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_ASYNC_SCANNING_SECONDARY))
              .AddIconAndColor(features::IsChromeRefresh2023()
                                   ? kDownloadWarningIcon
                                   : vector_icons::kNotSecureWarningIcon,
                               kColorDownloadItemIconWarning)
              .AddPrimarySubpageButton(
                  l10n_util::GetStringUTF16(
                      IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_ASYNC_SCANNING_DISCARD),
                  DownloadCommands::Command::DISCARD);
      ui_info.subpage_buttons[0].is_prominent = false;
      if (!download::DoesDownloadConnectorBlock(profile(), GetURL())) {
        ui_info.AddSecondarySubpageButton(
            l10n_util::GetStringUTF16(
                IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_ASYNC_SCANNING_CANCEL),
            DownloadCommands::Command::CANCEL_DEEP_SCAN,
            ui::kColorButtonForeground);
      }
      return ui_info;
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING: {
      std::u16string subpage_text = l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_LOCAL_DECRYPTION_IN_PROGRESS,
          u"\n\n");
      ui_info = DownloadUIModel::BubbleUIInfo()
                    .AddProgressBar()
                    .SetProgressBarLooping()
                    .AddSubpageSummary(subpage_text)
                    .AddIconAndColor(features::IsChromeRefresh2023()
                                         ? kDownloadWarningIcon
                                         : vector_icons::kNotSecureWarningIcon,
                                     kColorDownloadItemIconWarning)
                    // These download commands are not propagated
                    // to the DownloadItem. Instead they are handled specially
                    // in DownloadBubbleSecurityView::ProcessButtonClick. That
                    // means the semantics don't have to line up with the actual
                    // behavior of the download command.
                    // TODO(crbug/1482901): Remove this by creating a dedicated
                    // View for the local decryption prompt which directly
                    // handles teh button presses.
                    .AddPrimarySubpageButton(
                        l10n_util::GetStringUTF16(
                            IDS_DOWNLOAD_BUBBLE_LOCAL_DECRYPTION_CANCEL),
                        DownloadCommands::Command::CANCEL)
                    .AddSecondarySubpageButton(
                        l10n_util::GetStringUTF16(
                            IDS_DOWNLOAD_BUBBLE_BYPASS_LOCAL_DECRYPTION),
                        DownloadCommands::Command::BYPASS_DEEP_SCANNING);
      return ui_info;
    }
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED:
      return DownloadUIModel::BubbleUIInfo()
          .AddIconAndColor(features::IsChromeRefresh2023()
                               ? kDownloadWarningIcon
                               : vector_icons::kNotSecureWarningIcon,
                           kColorDownloadItemIconWarning)
          .AddPrimaryButton(DownloadCommands::Command::OPEN_WHEN_COMPLETE)
          .AddSecondaryTextColor(kColorDownloadItemTextWarning)
          .DisableMainButton();
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_UNSUPPORTED_FILETYPE:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case download::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY:
    case download::DOWNLOAD_DANGER_TYPE_MAX:
      break;
  }

  // Add primary button/quick actions for in-progress (paused or active), and
  // completed downloads
  bool has_progress_bar = GetState() == DownloadItem::IN_PROGRESS;
  BubbleUIInfo bubble_ui_info = DownloadUIModel::BubbleUIInfo();
  if (has_progress_bar) {
    bubble_ui_info.AddProgressBar();
    if (IsPaused()) {
      bubble_ui_info.AddQuickAction(
          DownloadCommands::Command::RESUME,
          l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_RESUME_QUICK_ACTION),
          features::IsChromeRefresh2023()
              ? &vector_icons::kPlayArrowChromeRefreshIcon
              : &vector_icons::kPlayArrowIcon);
      bubble_ui_info.AddQuickAction(
          DownloadCommands::Command::CANCEL,
          l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_CANCEL_QUICK_ACTION),
          features::IsChromeRefresh2023()
              ? &vector_icons::kCancelChromeRefreshIcon
              : &vector_icons::kCancelIcon);
    } else {
      bubble_ui_info.AddQuickAction(
          DownloadCommands::Command::PAUSE,
          l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_PAUSE_QUICK_ACTION),
          features::IsChromeRefresh2023()
              ? &vector_icons::kPauseChromeRefreshIcon
              : &vector_icons::kPauseIcon);
      bubble_ui_info.AddQuickAction(
          DownloadCommands::Command::CANCEL,
          l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_CANCEL_QUICK_ACTION),
          features::IsChromeRefresh2023()
              ? &vector_icons::kCancelChromeRefreshIcon
              : &vector_icons::kCancelIcon);
    }
  } else {
    bubble_ui_info.AddQuickAction(
        DownloadCommands::Command::SHOW_IN_FOLDER,
        l10n_util::GetStringUTF16(
            IDS_DOWNLOAD_BUBBLE_SHOW_IN_FOLDER_QUICK_ACTION),
        features::IsChromeRefresh2023()
            ? &vector_icons::kFolderChromeRefreshIcon
            : &vector_icons::kFolderIcon);
    bubble_ui_info.AddQuickAction(
        DownloadCommands::Command::OPEN_WHEN_COMPLETE,
        l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_OPEN_QUICK_ACTION),
        features::IsChromeRefresh2023()
            ? &vector_icons::kLaunchChromeRefreshIcon
            : &kOpenInNewIcon);
  }
  return bubble_ui_info;
}

DownloadUIModel::BubbleUIInfo
DownloadUIModel::GetBubbleUIInfoForTailoredWarning() const {
  NOTREACHED();
  return DownloadUIModel::BubbleUIInfo();
}

DownloadUIModel::BubbleUIInfo
DownloadUIModel::GetBubbleUIInfoForFileTypeWarningNoSafeBrowsing() const {
  BubbleUIInfo ui_info = BubbleUIInfo::SuspiciousUiPattern(
      l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_WARNING_NO_SAFE_BROWSING),
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_CONTINUE_UNVERIFIED_FILE));
  // Clear the "Learn why Chrome..." link. If the user is not capable of turning
  // on SB, do not show the default link and label.
  ui_info.learn_more_link = absl::nullopt;
  if (CanUserTurnOnSafeBrowsing(profile())) {
    ui_info.AddLearnMoreLink(
        IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_WARNING_SAFE_BROWSING_SETTING_LABEL,
        IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_WARNING_SAFE_BROWSING_SETTING_LINK,
        DownloadCommands::Command::OPEN_SAFE_BROWSING_SETTING);
  }
  return ui_info;
}

DownloadUIModel::BubbleUIInfo DownloadUIModel::GetBubbleUIInfo() const {
  switch (GetState()) {
    case DownloadItem::IN_PROGRESS:
    case DownloadItem::COMPLETE:
      return GetBubbleUIInfoForInProgressOrComplete();
    case DownloadItem::INTERRUPTED: {
      const FailState fail_state = GetLastFailState();
      if (fail_state != FailState::USER_CANCELED) {
        return GetBubbleUIInfoForInterrupted(fail_state);
      }
    }
      [[fallthrough]];
    case DownloadItem::CANCELLED:
    case DownloadItem::MAX_DOWNLOAD_STATE:
      return DownloadUIModel::BubbleUIInfo().AddIconAndColor(
          features::IsChromeRefresh2023()
              ? vector_icons::kFileDownloadOffChromeRefreshIcon
              : vector_icons::kFileDownloadOffIcon,
          ui::kColorSecondaryForeground);
  }
}

bool DownloadUIModel::ShouldShowTailoredWarning() const {
  return false;
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
      if (base::FeatureList::IsEnabled(
              safe_browsing::kImprovedDownloadBubbleWarnings)) {
        if (ShouldShowWarningForNoSafeBrowsing(model_->profile()) ||
            !WasSafeBrowsingVerdictObtained(model_->GetDownloadItem())) {
          // "Unverified download blocked"
          return l10n_util::GetStringUTF16(
              IDS_DOWNLOAD_BUBBLE_STATUS_WARNING_UNVERIFIED);
        }
        // "Suspicious download blocked"
        return l10n_util::GetStringUTF16(
            IDS_DOWNLOAD_BUBBLE_STATUS_WARNING_SUSPICIOUS);
      }
      // "Blocked • Unknown source"
      if (model_->IsExtensionDownload())
        return get_blocked_warning(IDS_DOWNLOAD_BUBBLE_STATUS_UNKNOWN_SOURCE);
      [[fallthrough]];
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      if (base::FeatureList::IsEnabled(
              safe_browsing::kImprovedDownloadBubbleWarnings)) {
        // "Dangerous download blocked"
        return l10n_util::GetStringUTF16(
            IDS_DOWNLOAD_BUBBLE_STATUS_WARNING_DANGEROUS);
      }
      // "Blocked • Dangerous"
      return get_blocked_warning(IDS_DOWNLOAD_BUBBLE_STATUS_DANGEROUS);

    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
      // "Blocked • Encrypted"
      return get_blocked_warning(IDS_DOWNLOAD_BUBBLE_STATUS_ENCRYPTED);
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
      if (base::FeatureList::IsEnabled(
              safe_browsing::kImprovedDownloadBubbleWarnings)) {
        // "Dangerous download blocked"
        return l10n_util::GetStringUTF16(
            IDS_DOWNLOAD_BUBBLE_STATUS_WARNING_DANGEROUS);
      }
      // "Blocked • Malware"
      return get_blocked_warning(IDS_DOWNLOAD_BUBBLE_STATUS_MALWARE);
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
      // "Blocked by Advanced Protection" or ("Suspicious download blocked" or
      // "Blocked • Uncommon file" depending on feature state).
      return request_ap_verdicts
                 ? l10n_util::GetStringUTF16(
                       IDS_DOWNLOAD_BUBBLE_STATUS_ADVANCED_PROTECTION)
                 : (base::FeatureList::IsEnabled(
                        safe_browsing::kImprovedDownloadBubbleWarnings)
                        ? l10n_util::GetStringUTF16(
                              IDS_DOWNLOAD_BUBBLE_STATUS_WARNING_SUSPICIOUS)
                        : get_blocked_warning(
                              IDS_DOWNLOAD_BUBBLE_STATUS_UNCOMMON_FILE));
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
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_UNSUPPORTED_FILETYPE:
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
  } else {
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
      delta_str =
          time_elapsed.InMinutes() == 0
              ? l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_STATUS_DONE)
              : ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                                       ui::TimeFormat::LENGTH_LONG,
                                       time_elapsed);
    }
    return GetBubbleStatusMessageWithBytes(
        ui::FormatBytes(model_->GetTotalBytes()), delta_str,
        /*is_active=*/false);
  }
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
      NOTREACHED();
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

bool DownloadUIModel::IsEncryptedArchive() const {
  return false;
}
