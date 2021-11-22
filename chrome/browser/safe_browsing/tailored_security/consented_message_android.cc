// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/tailored_security/consented_message_android.h"

#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/safe_browsing/android/safe_browsing_settings_launcher_android.h"
#include "chrome/grit/generated_resources.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "ui/base/l10n/l10n_util.h"

#include "base/logging.h"
#include "content/public/browser/web_contents.h"

namespace safe_browsing {

TailoredSecurityConsentedModalAndroid::TailoredSecurityConsentedModalAndroid() =
    default;

TailoredSecurityConsentedModalAndroid::
    ~TailoredSecurityConsentedModalAndroid() {
  DismissMessage(messages::DismissReason::UNKNOWN);
}

void TailoredSecurityConsentedModalAndroid::DisplayMessage(
    content::WebContents* web_contents) {
  if (message_) {
    return;
  }
  web_contents_ = web_contents;
  message_ = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::TAILORED_SECURITY_ENABLED,
      base::NullCallback(),
      base::BindOnce(
          &TailoredSecurityConsentedModalAndroid::HandleMessageDismissed,
          base::Unretained(this)));
  message_->SetTitle(
      l10n_util::GetStringUTF16(IDS_TAILORED_SECURITY_CONSENTED_MESSAGE_TITLE));
  message_->SetDescription(l10n_util::GetStringUTF16(
      IDS_TAILORED_SECURITY_CONSENTED_MESSAGE_DESCRIPTION));
  message_->SetPrimaryButtonText(l10n_util::GetStringUTF16(
      IDS_TAILORED_SECURITY_CONSENTED_MESSAGE_OK_BUTTON));
  message_->SetIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_MESSAGE_SAFETY_CHECK));
  message_->SetSecondaryIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_MESSAGE_SETTINGS));
  message_->SetSecondaryActionCallback(base::BindOnce(
      &TailoredSecurityConsentedModalAndroid::HandleSettingsClicked,
      base::Unretained(this)));

  messages::MessageDispatcherBridge::Get()->EnqueueWindowScopedMessage(
      message_.get(), web_contents_->GetTopLevelNativeWindow(),
      messages::MessagePriority::kNormal);
}

void TailoredSecurityConsentedModalAndroid::DismissMessage(
    messages::DismissReason dismiss_reason) {
  if (message_) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(message_.get(),
                                                             dismiss_reason);
  }
}

void TailoredSecurityConsentedModalAndroid::HandleSettingsClicked() {
  // TODO(crbug.com/1257628): Update pref about user action.
  // TODO(crbug.com/1257621): Add histogram to calculate conversion rate.
  ShowSafeBrowsingSettings(web_contents_);
  DismissMessage(messages::DismissReason::DISMISSED_BY_FEATURE);
}

void TailoredSecurityConsentedModalAndroid::HandleMessageDismissed(
    messages::DismissReason dismiss_reason) {
  // TODO(crbug.com/1257628): Update pref about user action.
  message_.reset();
}

}  // namespace safe_browsing
