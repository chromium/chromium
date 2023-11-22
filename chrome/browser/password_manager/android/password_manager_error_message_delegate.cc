// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_error_message_delegate.h"

#include "base/android/jni_android.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/grit/generated_resources.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"

PasswordManagerErrorMessageDelegate::PasswordManagerErrorMessageDelegate(
    std::unique_ptr<PasswordManagerErrorMessageHelperBridge> bridge_)
    : helper_bridge_(std::move(bridge_)) {}

PasswordManagerErrorMessageDelegate::~PasswordManagerErrorMessageDelegate() =
    default;

void PasswordManagerErrorMessageDelegate::DismissPasswordManagerErrorMessage(
    messages::DismissReason dismiss_reason) {
  if (message_) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(message_.get(),
                                                             dismiss_reason);
  }
}

void PasswordManagerErrorMessageDelegate::MaybeDisplayErrorMessage(
    content::WebContents* web_contents,
    PrefService* pref_service,
    password_manager::ErrorMessageFlowType flow_type,
    password_manager::PasswordStoreBackendErrorType error_type,
    base::OnceCallback<void()> dismissal_callback) {
  DCHECK(web_contents);

  if (!helper_bridge_->ShouldShowErrorUI(web_contents)) {
    // Even if no message was technically shown, the owner of `this` should know
    // that it has served its purpose and can be safely destroyed.
    std::move(dismissal_callback).Run();
    return;
  }

  DCHECK(!message_);

  CreateMessage(web_contents, flow_type);
  RecordErrorTypeMetrics(error_type);
  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), web_contents, messages::MessageScopeType::WEB_CONTENTS,
      messages::MessagePriority::kUrgent);
  helper_bridge_->SaveErrorUIShownTimestamp(web_contents);
  dismissal_callback_ = std::move(dismissal_callback);
}

void PasswordManagerErrorMessageDelegate::CreateMessage(
    content::WebContents* web_contents,
    password_manager::ErrorMessageFlowType flow_type) {
  messages::MessageIdentifier message_id =
      messages::MessageIdentifier::PASSWORD_MANAGER_ERROR;
  // Binding with base::Unretained(this) is safe here because
  // PasswordManagerErrorMessageDelegate owns `message_`. Callbacks won't be
  // called after the current object is destroyed.
  // It's safe to give a raw pointer to WebContents to the `callback` because
  // WebContents transitively owns the MessageWrapper so the `message_` can't
  // outlive `web_contents`.
  base::OnceClosure callback = base::BindOnce(
      &PasswordManagerErrorMessageDelegate::HandleSignInButtonClicked,
      base::Unretained(this), web_contents);

  message_ = std::make_unique<messages::MessageWrapper>(
      message_id, std::move(callback),
      base::BindOnce(
          &PasswordManagerErrorMessageDelegate::HandleMessageDismissed,
          base::Unretained(this)));

  int title_message_id =
      flow_type == password_manager::ErrorMessageFlowType::kSaveFlow
          ? IDS_SIGN_IN_TO_SAVE_PASSWORDS
          : IDS_SIGN_IN_TO_USE_PASSWORDS;
  message_->SetTitle(l10n_util::GetStringUTF16(title_message_id));

  std::u16string description =
      l10n_util::GetStringUTF16(IDS_PASSWORD_ERROR_DESCRIPTION);
  message_->SetDescription(description);

  message_->SetPrimaryButtonText(
      l10n_util::GetStringUTF16(IDS_PASSWORD_ERROR_SIGN_IN_BUTTON_TITLE));

  message_->SetIconResourceId(ResourceMapper::MapToJavaDrawableId(
      IDR_ANDORID_MESSAGE_PASSWORD_MANAGER_ERROR));
  message_->DisableIconTint();
}

void PasswordManagerErrorMessageDelegate::HandleMessageDismissed(
    messages::DismissReason dismiss_reason) {
  RecordDismissalReasonMetrics(dismiss_reason);
  message_.reset();
  // Running this callback results in `this` being destroyed, so no other
  // code should be added beyond this point.
  std::move(dismissal_callback_).Run();
}

void PasswordManagerErrorMessageDelegate::HandleSignInButtonClicked(
    content::WebContents* web_contents) {
  helper_bridge_->StartUpdateAccountCredentialsFlow(web_contents);
}

void PasswordManagerErrorMessageDelegate::RecordDismissalReasonMetrics(
    messages::DismissReason dismiss_reason) {
  base::UmaHistogramEnumeration("PasswordManager.ErrorMessageDismissalReason",
                                dismiss_reason, messages::DismissReason::COUNT);
}

void PasswordManagerErrorMessageDelegate::RecordErrorTypeMetrics(
    password_manager::PasswordStoreBackendErrorType error_type) {
  base::UmaHistogramEnumeration("PasswordManager.ErrorMessageDisplayReason",
                                error_type);
}
