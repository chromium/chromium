// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/tailored_security/consented_message_android.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/android/safe_browsing_settings_navigation_android.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_outcome.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace safe_browsing {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(SyncedEsbOutcome)
enum class SyncedEsbOutcome {
  kShown = 0,
  // The user clicked the OK button on the enable dialog.
  kAcceptedOk = 1,
  // The user clicked the Turn On button on the disable dialog.
  kAcceptedTurnOn = 2,
  kDismissed = 3,
  kAcceptedFailed = 4,
  kMaxValue = kAcceptedFailed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/safe_browsing/enums.xml:SyncedEsbOutcome)

const char kSyncedEsbDialogOkButtonClicked[] =
    "SafeBrowsing.SyncedEsbDialog.OkButtonClicked";
const char kSyncedEsbDialogTurnOnButtonClicked[] =
    "SafeBrowsing.SyncedEsbDialog.TurnOnButtonClicked";

void LogOutcome(TailoredSecurityOutcome outcome, bool enable) {
  std::string histogram =
      enable ? "SafeBrowsing.TailoredSecurityConsentedEnabledMessageOutcome"
             : "SafeBrowsing.TailoredSecurityConsentedDisabledMessageOutcome";
  base::UmaHistogramEnumeration(histogram, outcome);
  base::RecordAction(
      base::UserMetricsAction(GetUserActionString(outcome, enable)));
}

void LogSyncedEsbDialogOutcome(bool enable, SyncedEsbOutcome outcome) {
  std::string histogram =
      enable ? "SafeBrowsing.SyncedEsbDialogEnabledMessageOutcome"
             : "SafeBrowsing.SyncedEsbDialogDisabledMessageOutcome";
  base::UmaHistogramEnumeration(histogram, outcome);
}

}  // namespace

TailoredSecurityConsentedModalAndroid::TailoredSecurityConsentedModalAndroid(
    content::WebContents* web_contents,
    bool enable,
    base::OnceClosure dismiss_callback,
    bool is_requested_by_synced_esb)
    : web_contents_(web_contents),
      window_android_(web_contents->GetTopLevelNativeWindow()),
      dismiss_callback_(std::move(dismiss_callback)),
      is_enable_message_(enable),
      is_requested_by_synced_esb_(is_requested_by_synced_esb) {
  message_ = std::make_unique<messages::MessageWrapper>(
      is_enable_message_
          ? messages::MessageIdentifier::TAILORED_SECURITY_ENABLED
          : messages::MessageIdentifier::TAILORED_SECURITY_DISABLED,
      base::BindOnce(
          &TailoredSecurityConsentedModalAndroid::HandleMessageAccepted,
          base::Unretained(this)),
      base::BindOnce(
          &TailoredSecurityConsentedModalAndroid::HandleMessageDismissed,
          base::Unretained(this)));
  std::u16string title, description;
  int icon_resource_id;
  if (is_enable_message_) {
    title = l10n_util::GetStringUTF16(
        base::FeatureList::IsEnabled(safe_browsing::kEsbAsASyncedSetting)
            ? IDS_TAILORED_SECURITY_CONSENTED_ENHANCED_PROTECTION_ENABLE_MESSAGE_TITLE
            : IDS_TAILORED_SECURITY_CONSENTED_ENABLE_MESSAGE_TITLE);

    if (base::FeatureList::IsEnabled(safe_browsing::kEsbAsASyncedSetting)) {
      signin::IdentityManager* identity_manager =
          IdentityManagerFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents->GetBrowserContext()));
      if (identity_manager &&
          identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
        std::string email_address =
            identity_manager
                ->FindExtendedAccountInfoByAccountId(
                    identity_manager->GetPrimaryAccountId(
                        signin::ConsentLevel::kSignin))
                .email;
        description = base::UTF8ToUTF16(email_address);
      } else {
        description = l10n_util::GetStringUTF16(
            IDS_TAILORED_SECURITY_CONSENTED_ENABLE_MESSAGE_DESCRIPTION);
      }
    } else {
      description = l10n_util::GetStringUTF16(
          IDS_TAILORED_SECURITY_CONSENTED_ENABLE_MESSAGE_DESCRIPTION);
    }
    icon_resource_id =
        ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_MESSAGE_SHIELD_BLUE);
    // Need to disable tint here because it removes a shade of blue from the
    // shield which distorts the image.
    message_->DisableIconTint();
  } else {
    title = l10n_util::GetStringUTF16(
        base::FeatureList::IsEnabled(safe_browsing::kEsbAsASyncedSetting)
            ? IDS_TAILORED_SECURITY_CONSENTED_ESB_OFF_MESSAGE_TITLE
            : IDS_TAILORED_SECURITY_CONSENTED_DISABLE_MESSAGE_TITLE);
    description = l10n_util::GetStringUTF16(
        IDS_TAILORED_SECURITY_CONSENTED_DISABLE_MESSAGE_DESCRIPTION);
    icon_resource_id =
        base::FeatureList::IsEnabled(safe_browsing::kEsbAsASyncedSetting)
            ? ResourceMapper::MapToJavaDrawableId(
                  IDR_ANDROID_INFO_OUTLINE_LOGO_24DP)
            : ResourceMapper::MapToJavaDrawableId(
                  IDR_ANDROID_MESSAGE_SHIELD_GRAY);
    message_->DisableIconTint();
  }
  message_->SetTitle(title);
  if (!base::FeatureList::IsEnabled(safe_browsing::kEsbAsASyncedSetting) ||
      is_enable_message_) {
    message_->SetDescription(description);
  }
  message_->SetPrimaryButtonText(l10n_util::GetStringUTF16(
      base::FeatureList::IsEnabled(safe_browsing::kEsbAsASyncedSetting)
          ? (is_enable_message_
                 ? IDS_TAILORED_SECURITY_CONSENTED_MESSAGE_OK_BUTTON
                 : IDS_TAILORED_SECURITY_CONSENTED_PROMOTION_MESSAGE_TURN_ON)
          : IDS_TAILORED_SECURITY_CONSENTED_MESSAGE_OK_BUTTON));
  message_->SetIconResourceId(icon_resource_id);
  if (!base::FeatureList::IsEnabled(safe_browsing::kEsbAsASyncedSetting)) {
    message_->SetSecondaryIconResourceId(
        ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_MESSAGE_SETTINGS));
    message_->SetSecondaryActionCallback(base::BindRepeating(
        &TailoredSecurityConsentedModalAndroid::HandleSettingsClicked,
        base::Unretained(this)));
  }

  messages::MessageDispatcherBridge::Get()->EnqueueWindowScopedMessage(
      message_.get(), window_android_, messages::MessagePriority::kNormal);
  if (is_requested_by_synced_esb_) {
    LogSyncedEsbDialogOutcome(is_enable_message_, SyncedEsbOutcome::kShown);
  } else {
    LogOutcome(TailoredSecurityOutcome::kShown, is_enable_message_);
  }
}

TailoredSecurityConsentedModalAndroid::
    ~TailoredSecurityConsentedModalAndroid() {
  DismissMessageInternal(messages::DismissReason::UNKNOWN);
}

void TailoredSecurityConsentedModalAndroid::DismissMessageInternal(
    messages::DismissReason dismiss_reason) {
  if (!message_)
    return;
  messages::MessageDispatcherBridge::Get()->DismissMessage(message_.get(),
                                                           dismiss_reason);
}

void TailoredSecurityConsentedModalAndroid::HandleSettingsClicked() {
  ShowSafeBrowsingSettings(window_android_,
                           SettingsAccessPoint::kTailoredSecurity);
  LogOutcome(TailoredSecurityOutcome::kSettings, is_enable_message_);
  DismissMessageInternal(messages::DismissReason::SECONDARY_ACTION);
}

void TailoredSecurityConsentedModalAndroid::HandleMessageDismissed(
    messages::DismissReason dismiss_reason) {
  LogOutcome(TailoredSecurityOutcome::kDismissed, is_enable_message_);
  if (is_requested_by_synced_esb_) {
    LogSyncedEsbDialogOutcome(is_enable_message_, SyncedEsbOutcome::kDismissed);
    base::UmaHistogramEnumeration(
        "SafeBrowsing.SyncedEsbDialogEnabledMessageDismissReason",
        dismiss_reason);
  }
  message_.reset();
  if (dismiss_callback_) {
    // The callback may delete `this`. Do not add code after running it.
    std::move(dismiss_callback_).Run();
  }
}

void TailoredSecurityConsentedModalAndroid::HandleMessageAccepted() {
  // TODO(crbug.com/389996059): Check if the action came from ChAI, only use
  // LogOutcome to record ChAI actions.
  LogOutcome(TailoredSecurityOutcome::kAccepted, is_enable_message_);
  if (base::FeatureList::IsEnabled(safe_browsing::kEsbAsASyncedSetting)) {
    if (!web_contents_) {
      // WebContents has been destroyed. The handler failed to finish running.
      // Log this situation.
      LogSyncedEsbDialogOutcome(is_enable_message_,
                                SyncedEsbOutcome::kAcceptedFailed);
      return;
    }
    Profile* profile =
        Profile::FromBrowserContext(web_contents_->GetBrowserContext());
    SetSafeBrowsingState(profile->GetPrefs(),
                         SafeBrowsingState::ENHANCED_PROTECTION);
    LogSyncedEsbDialogOutcome(is_enable_message_,
                              is_enable_message_
                                  ? SyncedEsbOutcome::kAcceptedOk
                                  : SyncedEsbOutcome::kAcceptedTurnOn);
    base::RecordAction(base::UserMetricsAction(
        is_enable_message_ ? kSyncedEsbDialogOkButtonClicked
                           : kSyncedEsbDialogTurnOnButtonClicked));
  }
}

}  // namespace safe_browsing
