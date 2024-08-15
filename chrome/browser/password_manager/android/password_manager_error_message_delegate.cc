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

namespace {

using PasswordStoreBackendErrorType =
    password_manager::PasswordStoreBackendErrorType;

std::string GetErrorMessageName(PasswordStoreBackendErrorType error_type) {
  switch (error_type) {
    case PasswordStoreBackendErrorType::kAuthErrorResolvable:
      return "AuthErrorResolvable";
    case PasswordStoreBackendErrorType::kAuthErrorUnresolvable:
      return "AuthErrorUnresolvable";
    case PasswordStoreBackendErrorType::kKeyRetrievalRequired:
      return "KeyRetrievalRequired";
    case PasswordStoreBackendErrorType::kGMSCoreOutdatedSavingPossible:
      return "GMSCoreOutdatedSavingPossible";
    case PasswordStoreBackendErrorType::kGMSCoreOutdatedSavingDisabled:
      return "GMSCoreOutdatedSavingDisabled";
    case PasswordStoreBackendErrorType::kUncategorized:
    case PasswordStoreBackendErrorType::kKeychainError:
      // Other error types aren't supported.
      NOTREACHED();
  }
}

void RecordDismissalReasonMetrics(PasswordStoreBackendErrorType error_type,
                                  messages::DismissReason dismiss_reason) {
  base::UmaHistogramEnumeration("PasswordManager.ErrorMessageDismissalReason." +
                                    GetErrorMessageName(error_type),
                                dismiss_reason, messages::DismissReason::COUNT);
}

void RecordErrorTypeMetrics(PasswordStoreBackendErrorType error_type) {
  base::UmaHistogramEnumeration("PasswordManager.ErrorMessageDisplayReason",
                                error_type);
}

void SetVerifyItIsYouMessageContent(
    messages::MessageWrapper* message,
    password_manager::ErrorMessageFlowType flow_type) {
  message->SetTitle(l10n_util::GetStringUTF16(IDS_VERIFY_IT_IS_YOU));

  std::u16string description = l10n_util::GetStringUTF16(
      flow_type == password_manager::ErrorMessageFlowType::kSaveFlow
          ? IDS_PASSWORD_ERROR_DESCRIPTION_SIGN_UP
          : IDS_PASSWORD_ERROR_DESCRIPTION_SIGN_IN);
  message->SetDescription(description);

  message->SetPrimaryButtonText(
      l10n_util::GetStringUTF16(IDS_PASSWORD_ERROR_VERIFY_BUTTON_TITLE));

  message->SetIconResourceId(ResourceMapper::MapToJavaDrawableId(
      IDR_ANDORID_MESSAGE_PASSWORD_MANAGER_ERROR));

  message->DisableIconTint();
}

void SetUpdateGmsCoreMessageContent(messages::MessageWrapper* message,
                                    PasswordStoreBackendErrorType error_type) {
  CHECK(error_type ==
            PasswordStoreBackendErrorType::kGMSCoreOutdatedSavingPossible ||
        error_type ==
            PasswordStoreBackendErrorType::kGMSCoreOutdatedSavingDisabled);

  message->SetPrimaryButtonText(
      l10n_util::GetStringUTF16(IDS_UPDATE_GMS_BUTTON_TITLE));

  if (error_type ==
      PasswordStoreBackendErrorType::kGMSCoreOutdatedSavingPossible) {
    message->SetTitle(l10n_util::GetStringUTF16(IDS_UPDATE_GMS));
    message->SetDescription(
        l10n_util::GetStringUTF16(IDS_UPDATE_GMS_TO_SAVE_PASSWORDS_TO_ACCOUNT));
    message->SetIconResourceId(ResourceMapper::MapToJavaDrawableId(
        IDR_ANDROID_PASSWORD_MANAGER_LOGO_24DP));
  } else {
    message->SetTitle(l10n_util::GetStringUTF16(IDS_UPDATE_TO_SAVE_PASSWORDS));
    message->SetDescription(
        l10n_util::GetStringUTF16(IDS_UPDATE_GMS_TO_SAVE_PASSWORDS));
    message->SetIconResourceId(
        ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_IC_ERROR));
  }
  message->DisableIconTint();
}

}  // namespace

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
    PasswordStoreBackendErrorType error_type,
    base::OnceCallback<void()> dismissal_callback) {
  DCHECK(web_contents);

  if (!ShouldShowErrorUI(web_contents, error_type)) {
    // Even if no message was technically shown, the owner of `this` should know
    // that it has served its purpose and can be safely destroyed.
    std::move(dismissal_callback).Run();
    return;
  }

  DCHECK(!message_);
  message_ =
      CreateMessage(web_contents, error_type, std::move(dismissal_callback));
  error_type_ = error_type;
  switch (error_type) {
    case PasswordStoreBackendErrorType::kAuthErrorResolvable:
    case PasswordStoreBackendErrorType::kAuthErrorUnresolvable:
    case PasswordStoreBackendErrorType::kKeyRetrievalRequired:
      SetVerifyItIsYouMessageContent(message_.get(), flow_type);
      break;
    case PasswordStoreBackendErrorType::kGMSCoreOutdatedSavingPossible:
    case PasswordStoreBackendErrorType::kGMSCoreOutdatedSavingDisabled:
      SetUpdateGmsCoreMessageContent(message_.get(), error_type);
      break;
    case PasswordStoreBackendErrorType::kUncategorized:
    case PasswordStoreBackendErrorType::kKeychainError:
      // Other error types aren't supported.
      NOTREACHED_IN_MIGRATION();
  }

  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), web_contents, messages::MessageScopeType::WEB_CONTENTS,
      messages::MessagePriority::kUrgent);
  helper_bridge_->SaveErrorUIShownTimestamp(web_contents);
}

bool PasswordManagerErrorMessageDelegate::ShouldShowErrorUI(
    content::WebContents* web_contents,
    password_manager::PasswordStoreBackendErrorType error_type) {
  switch (error_type) {
    case PasswordStoreBackendErrorType::kAuthErrorResolvable:
    case PasswordStoreBackendErrorType::kAuthErrorUnresolvable:
    case PasswordStoreBackendErrorType::kKeyRetrievalRequired:
      return helper_bridge_->ShouldShowSignInErrorUI(web_contents);
    case PasswordStoreBackendErrorType::kGMSCoreOutdatedSavingPossible:
    case PasswordStoreBackendErrorType::kGMSCoreOutdatedSavingDisabled:
      return helper_bridge_->ShouldShowUpdateGMSCoreErrorUI(web_contents);
    case PasswordStoreBackendErrorType::kUncategorized:
    case PasswordStoreBackendErrorType::kKeychainError:
      // Other error types aren't supported.
      NOTREACHED();
  }
}

std::unique_ptr<messages::MessageWrapper>
PasswordManagerErrorMessageDelegate::CreateMessage(
    content::WebContents* web_contents,
    PasswordStoreBackendErrorType error_type,
    base::OnceCallback<void()> dismissal_callback) {
  messages::MessageIdentifier message_id =
      messages::MessageIdentifier::PASSWORD_MANAGER_ERROR;

  base::OnceClosure action_callback = base::BindOnce(
      &PasswordManagerErrorMessageDelegate::HandleActionButtonClicked,
      weak_ptr_factory_.GetWeakPtr(), web_contents, error_type);

  messages::MessageWrapper::DismissCallback post_dismissal_callback =
      base::BindOnce(
          &PasswordManagerErrorMessageDelegate::HandleMessageDismissed,
          weak_ptr_factory_.GetWeakPtr())
          .Then(std::move(dismissal_callback));

  RecordErrorTypeMetrics(error_type);

  return std::make_unique<messages::MessageWrapper>(
      message_id, std::move(action_callback),
      std::move(post_dismissal_callback));
}

void PasswordManagerErrorMessageDelegate::HandleMessageDismissed(
    messages::DismissReason dismiss_reason) {
  RecordDismissalReasonMetrics(error_type_, dismiss_reason);
  message_.reset();
}

void PasswordManagerErrorMessageDelegate::HandleActionButtonClicked(
    content::WebContents* web_contents,
    PasswordStoreBackendErrorType error) {
  switch (error) {
    case PasswordStoreBackendErrorType::kAuthErrorResolvable:
    case PasswordStoreBackendErrorType::kAuthErrorUnresolvable:
      helper_bridge_->StartUpdateAccountCredentialsFlow(web_contents);
      break;
    case PasswordStoreBackendErrorType::kKeyRetrievalRequired:
      helper_bridge_->StartTrustedVaultKeyRetrievalFlow(web_contents);
      break;
    case PasswordStoreBackendErrorType::kGMSCoreOutdatedSavingPossible:
    case PasswordStoreBackendErrorType::kGMSCoreOutdatedSavingDisabled:
      helper_bridge_->LaunchGmsUpdate(web_contents);
      break;
    case PasswordStoreBackendErrorType::kUncategorized:
    case PasswordStoreBackendErrorType::kKeychainError:
      // Other error types aren't supported.
      NOTREACHED_IN_MIGRATION();
  }
}
