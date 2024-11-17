// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/tailored_security/consented_message_android.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/safe_browsing/android/safe_browsing_settings_navigation_android.h"
#include "chrome/grit/generated_resources.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_outcome.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace safe_browsing {

namespace {

void LogOutcome(TailoredSecurityOutcome outcome, bool enable) {
  std::string histogram =
      enable ? "SafeBrowsing.TailoredSecurityConsentedEnabledMessageOutcome"
             : "SafeBrowsing.TailoredSecurityConsentedDisabledMessageOutcome";
  base::UmaHistogramEnumeration(histogram, outcome);
  base::RecordAction(
      base::UserMetricsAction(GetUserActionString(outcome, enable)));
}

}  // namespace

TailoredSecurityConsentedModalAndroid::TailoredSecurityConsentedModalAndroid(
    content::WebContents* web_contents,
    bool enable,
    base::OnceClosure dismiss_callback)
    : window_android_(web_contents->GetTopLevelNativeWindow()),
      dismiss_callback_(std::move(dismiss_callback)),
      is_enable_message_(enable) {
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
        IDS_TAILORED_SECURITY_CONSENTED_ENABLE_MESSAGE_TITLE);
    description = l10n_util::GetStringUTF16(
        IDS_TAILORED_SECURITY_CONSENTED_ENABLE_MESSAGE_DESCRIPTION);
    icon_resource_id =
        ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_MESSAGE_SHIELD_BLUE);
    // Need to disable tint here because it removes a shade of blue from the
    // shield which distorts the image.
    message_->DisableIconTint();
  } else {
    title = l10n_util::GetStringUTF16(
        IDS_TAILORED_SECURITY_CONSENTED_DISABLE_MESSAGE_TITLE);
    description = l10n_util::GetStringUTF16(
        IDS_TAILORED_SECURITY_CONSENTED_DISABLE_MESSAGE_DESCRIPTION);
    icon_resource_id =
        ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_MESSAGE_SHIELD_GRAY);
    message_->DisableIconTint();
  }
  message_->SetTitle(title);
  message_->SetDescription(description);
  message_->SetPrimaryButtonText(l10n_util::GetStringUTF16(
      IDS_TAILORED_SECURITY_CONSENTED_MESSAGE_OK_BUTTON));
  message_->SetIconResourceId(icon_resource_id);
  message_->SetSecondaryIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_MESSAGE_SETTINGS));
  message_->SetSecondaryActionCallback(base::BindRepeating(
      &TailoredSecurityConsentedModalAndroid::HandleSettingsClicked,
      base::Unretained(this)));

  messages::MessageDispatcherBridge::Get()->EnqueueWindowScopedMessage(
      message_.get(), window_android_, messages::MessagePriority::kNormal);
  LogOutcome(TailoredSecurityOutcome::kShown, is_enable_message_);
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
  message_.reset();
  if (dismiss_callback_)
    std::move(dismiss_callback_).Run();
}

void TailoredSecurityConsentedModalAndroid::HandleMessageAccepted() {
  LogOutcome(TailoredSecurityOutcome::kAccepted, is_enable_message_);
}

}  // namespace safe_browsing
