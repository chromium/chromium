// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate.h"

#include "base/base64.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "content/public/common/content_features.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/webauthn/authenticator_request_scheduler.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/webauthn/android/conditional_ui_delegate_android.h"
#endif

ChromeWebAuthnCredentialsDelegate::ChromeWebAuthnCredentialsDelegate(
    ChromePasswordManagerClient* client)
    : client_(client) {}

ChromeWebAuthnCredentialsDelegate::~ChromeWebAuthnCredentialsDelegate() =
    default;

bool ChromeWebAuthnCredentialsDelegate::IsWebAuthnAutofillEnabled() const {
  return base::FeatureList::IsEnabled(features::kWebAuthConditionalUI);
}

void ChromeWebAuthnCredentialsDelegate::LaunchWebAuthnFlow() {
#if !BUILDFLAG(IS_ANDROID)
  ChromeAuthenticatorRequestDelegate* authenticator_delegate =
      AuthenticatorRequestScheduler::GetRequestDelegate(
          client_->web_contents());
  if (!authenticator_delegate) {
    return;
  }
  authenticator_delegate->dialog_model()->TransitionToModalWebAuthnRequest();
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ChromeWebAuthnCredentialsDelegate::SelectWebAuthnCredential(
    std::string backend_id) {
  // `backend_id` is the base64-encoded credential ID. See
  // `OnCredentialsReceived()` for where these are encoded.
  absl::optional<std::vector<uint8_t>> selected_credential_id =
      base::Base64Decode(backend_id);
  DCHECK(selected_credential_id);

#if BUILDFLAG(IS_ANDROID)
  auto* credentials_delegate =
      ConditionalUiDelegateAndroid::GetConditionalUiDelegate(
          client_->web_contents());
  if (!credentials_delegate) {
    std::move(retrieve_suggestions_callback_).Run();
    return;
  }
  credentials_delegate->OnWebAuthnAccountSelected(*selected_credential_id);
#else
  ChromeAuthenticatorRequestDelegate* authenticator_delegate =
      AuthenticatorRequestScheduler::GetRequestDelegate(
          client_->web_contents());
  if (!authenticator_delegate) {
    return;
  }
  authenticator_delegate->dialog_model()->OnAccountPreselected(
      *selected_credential_id);
#endif  // BUILDFLAG(IS_ANDROID)
}

const std::vector<autofill::Suggestion>&
ChromeWebAuthnCredentialsDelegate::GetWebAuthnSuggestions() const {
  return suggestions_;
}

void ChromeWebAuthnCredentialsDelegate::RetrieveWebAuthnSuggestions(
    base::OnceClosure callback) {
  retrieve_suggestions_callback_ = std::move(callback);

#if BUILDFLAG(IS_ANDROID)
  auto* credentials_delegate =
      ConditionalUiDelegateAndroid::GetConditionalUiDelegate(
          client_->web_contents());
  if (!credentials_delegate) {
    std::move(retrieve_suggestions_callback_).Run();
    return;
  }
  credentials_delegate->RetrieveWebAuthnCredentials(
      base::BindOnce(&ChromeWebAuthnCredentialsDelegate::OnCredentialsReceived,
                     weak_ptr_factory_.GetWeakPtr()));
#else
  ChromeAuthenticatorRequestDelegate* authenticator_delegate =
      AuthenticatorRequestScheduler::GetRequestDelegate(
          client_->web_contents());
  if (!authenticator_delegate) {
    std::move(retrieve_suggestions_callback_).Run();
    return;
  }
  authenticator_delegate->dialog_model()->GetCredentialListForConditionalUi(
      base::BindOnce(&ChromeWebAuthnCredentialsDelegate::OnCredentialsReceived,
                     weak_ptr_factory_.GetWeakPtr()));
#endif
}

void ChromeWebAuthnCredentialsDelegate::OnCredentialsReceived(
    const std::vector<device::DiscoverableCredentialMetadata>& credentials) {
  std::vector<autofill::Suggestion> suggestions;
  for (const auto& credential : credentials) {
    std::u16string name;
    if (credential.user.display_name &&
        !credential.user.display_name->empty()) {
      name = base::UTF8ToUTF16(*credential.user.display_name);
    } else {
      // TODO(crbug.com/1329958): i18n this string.
      name = u"Unknown account";
    }
    autofill::Suggestion suggestion(std::move(name));
    if (credential.user.name) {
      suggestion.label = base::UTF8ToUTF16(*credential.user.name);
    }
    suggestion.icon = "fingerprint";
    suggestion.frontend_id = autofill::POPUP_ITEM_ID_WEBAUTHN_CREDENTIAL;
    suggestion.payload = base::Base64Encode(credential.cred_id);
    suggestions.push_back(std::move(suggestion));
  }
  suggestions_ = std::move(suggestions);
  std::move(retrieve_suggestions_callback_).Run();
}
