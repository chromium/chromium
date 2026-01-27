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
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service_utils.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using PasswordStoreBackendErrorType =
    password_manager::PasswordStoreBackendErrorType;

// Increase the timeout for the unlock message to 45s from the default 10s.
constexpr base::TimeDelta kDurationForKeyUnlockMessage = base::Seconds(45);

std::string GetErrorMessageName(PasswordStoreBackendErrorType error_type) {
  switch (error_type) {
    case PasswordStoreBackendErrorType::kAuthErrorResolvable:
      return "AuthErrorResolvable";
    case PasswordStoreBackendErrorType::kAuthErrorUnresolvable:
      return "AuthErrorUnresolvable";
    case PasswordStoreBackendErrorType::kKeyRetrievalRequired:
      return "KeyRetrievalRequired";
    case PasswordStoreBackendErrorType::kEmptySecurityDomain:
      return "EmptySecurityDomain";
    case PasswordStoreBackendErrorType::kIrretrievableSecurityDomain:
      return "IrretrievableSecurityDomain";
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

bool ShouldSaveMessageTimeStamp(PasswordStoreBackendErrorType error_type,
                                messages::DismissReason dismiss_reason) {
  if (error_type != PasswordStoreBackendErrorType::kKeyRetrievalRequired) {
    // For all other errors, the time has already been saved.
    return false;
  }
  // Always check the feature after the error type to enroll only Trusted Vault
  // users into the experiment.
  if (!base::FeatureList::IsEnabled(
          syncer::kSyncTrustedVaultErrorMessageDuration)) {
    // If the feature is not active, the time has already been saved.
    return false;
  }
  switch (dismiss_reason) {
    case messages::DismissReason::PRIMARY_ACTION:
    case messages::DismissReason::SECONDARY_ACTION:
    case messages::DismissReason::GESTURE:
    case messages::DismissReason::CLOSE_BUTTON:
      // The user dismissed the message, save the stamp to not show it again.
      return true;
    case messages::DismissReason::TIMER:
    case messages::DismissReason::DISMISSED_BY_FEATURE:
    case messages::DismissReason::TAB_SWITCHED:
    case messages::DismissReason::TAB_DESTROYED:
    case messages::DismissReason::ACTIVITY_DESTROYED:
    case messages::DismissReason::SCOPE_DESTROYED:
    case messages::DismissReason::UNKNOWN:
    case messages::DismissReason::COUNT:
      return false;
  }
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
  // TODO(crbug.com/379762002): Replace all the switches with passing-in
  // an already customized "handler".
  switch (error_type) {
    case PasswordStoreBackendErrorType::kAuthErrorResolvable:
    case PasswordStoreBackendErrorType::kAuthErrorUnresolvable:
    case PasswordStoreBackendErrorType::kKeyRetrievalRequired:
    case PasswordStoreBackendErrorType::kEmptySecurityDomain:
    case PasswordStoreBackendErrorType::kIrretrievableSecurityDomain:
      SetVerifyItIsYouMessageContent(message_.get(), flow_type);
      break;
    case PasswordStoreBackendErrorType::kUncategorized:
    case PasswordStoreBackendErrorType::kKeychainError:
      // Other error types aren't supported.
      NOTREACHED();
  }

  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), web_contents, messages::MessageScopeType::WEB_CONTENTS,
      messages::MessagePriority::kUrgent);
  if (error_type != PasswordStoreBackendErrorType::kKeyRetrievalRequired ||
      !base::FeatureList::IsEnabled(
          syncer::kSyncTrustedVaultErrorMessageDuration)) {
    helper_bridge_->SaveErrorUIShownTimestamp(web_contents);
  }
}

bool PasswordManagerErrorMessageDelegate::ShouldShowErrorUI(
    content::WebContents* web_contents,
    password_manager::PasswordStoreBackendErrorType error_type) {
  switch (error_type) {
    case PasswordStoreBackendErrorType::kAuthErrorResolvable:
    case PasswordStoreBackendErrorType::kAuthErrorUnresolvable:
    case PasswordStoreBackendErrorType::kKeyRetrievalRequired:
    case PasswordStoreBackendErrorType::kEmptySecurityDomain:
    case PasswordStoreBackendErrorType::kIrretrievableSecurityDomain:
      return helper_bridge_->ShouldShowSignInErrorUI(web_contents);
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
          weak_ptr_factory_.GetWeakPtr(), web_contents, error_type)
          .Then(std::move(dismissal_callback));

  RecordErrorTypeMetrics(error_type);

  auto message = std::make_unique<messages::MessageWrapper>(
      message_id, std::move(action_callback),
      std::move(post_dismissal_callback));
  if (error_type == PasswordStoreBackendErrorType::kKeyRetrievalRequired &&
      base::FeatureList::IsEnabled(
          syncer::kSyncTrustedVaultErrorMessageDuration)) {
    message->SetDuration(kDurationForKeyUnlockMessage.InMilliseconds());
  }
  return message;
}

void PasswordManagerErrorMessageDelegate::HandleMessageDismissed(
    content::WebContents* web_contents,
    PasswordStoreBackendErrorType error_type,
    messages::DismissReason dismiss_reason) {
  if (ShouldSaveMessageTimeStamp(error_type, dismiss_reason)) {
    helper_bridge_->SaveErrorUIShownTimestamp(web_contents);
  }
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
    case PasswordStoreBackendErrorType::kEmptySecurityDomain:
    case PasswordStoreBackendErrorType::kIrretrievableSecurityDomain:
      helper_bridge_->StartTrustedVaultKeyRetrievalFlow(
          web_contents, trusted_vault::TrustedVaultUserActionTriggerForUMA::
                            kPasswordManagerErrorMessage);
      break;
    case PasswordStoreBackendErrorType::kUncategorized:
    case PasswordStoreBackendErrorType::kKeychainError:
      // Other error types aren't supported.
      NOTREACHED();
  }
}
