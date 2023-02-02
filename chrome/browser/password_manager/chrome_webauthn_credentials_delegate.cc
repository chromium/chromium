// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate.h"

#include "base/base64.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/webauthn/authenticator_request_scheduler.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/webauthn/android/webauthn_request_delegate_android.h"
#endif

using password_manager::PasskeyCredential;

ChromeWebAuthnCredentialsDelegate::ChromeWebAuthnCredentialsDelegate(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

ChromeWebAuthnCredentialsDelegate::~ChromeWebAuthnCredentialsDelegate() =
    default;

void ChromeWebAuthnCredentialsDelegate::LaunchWebAuthnFlow() {
#if !BUILDFLAG(IS_ANDROID)
  ChromeAuthenticatorRequestDelegate* authenticator_delegate =
      AuthenticatorRequestScheduler::GetRequestDelegate(web_contents_);
  if (!authenticator_delegate) {
    return;
  }
  authenticator_delegate->dialog_model()->TransitionToModalWebAuthnRequest();
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ChromeWebAuthnCredentialsDelegate::SelectPasskey(
    const std::string& backend_id) {
  // `backend_id` is the base64-encoded credential ID. See
  // `OnCredentialsReceived()` for where these are encoded.
  absl::optional<std::vector<uint8_t>> selected_credential_id =
      base::Base64Decode(backend_id);
  DCHECK(selected_credential_id);

#if BUILDFLAG(IS_ANDROID)
  auto* request_delegate =
      WebAuthnRequestDelegateAndroid::GetRequestDelegate(web_contents_);
  if (!request_delegate) {
    return;
  }
  request_delegate->OnWebAuthnAccountSelected(*selected_credential_id);
#else
  ChromeAuthenticatorRequestDelegate* authenticator_delegate =
      AuthenticatorRequestScheduler::GetRequestDelegate(web_contents_);
  if (!authenticator_delegate) {
    return;
  }
  authenticator_delegate->dialog_model()->OnAccountPreselected(
      *selected_credential_id);
#endif  // BUILDFLAG(IS_ANDROID)
}

const absl::optional<std::vector<PasskeyCredential>>&
ChromeWebAuthnCredentialsDelegate::GetPasskeys() const {
  return passkeys_;
}

void ChromeWebAuthnCredentialsDelegate::RetrievePasskeys(
    base::OnceClosure callback) {
  if (passkeys_.has_value()) {
    // Entries were already populated from the WebAuthn request.
    std::move(callback).Run();
    return;
  }

  retrieve_passkeys_callback_ = std::move(callback);
}

void ChromeWebAuthnCredentialsDelegate::OnCredentialsReceived(
    const std::vector<device::DiscoverableCredentialMetadata>& credentials) {
  std::vector<PasskeyCredential> passkeys;

  for (const auto& credential : credentials) {
    std::u16string name;
    if (credential.user.name && !credential.user.name->empty()) {
      name = base::UTF8ToUTF16(*credential.user.name);
    } else {
      name = l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN);
    }
    passkeys.emplace_back(
        PasskeyCredential::Username(name),
        PasskeyCredential::BackendId(base::Base64Encode(credential.cred_id)));
  }

  passkeys_ = std::move(passkeys);

  if (retrieve_passkeys_callback_) {
    std::move(retrieve_passkeys_callback_).Run();
  }
}

void ChromeWebAuthnCredentialsDelegate::NotifyWebAuthnRequestAborted() {
  passkeys_ = absl::nullopt;
  if (retrieve_passkeys_callback_) {
    std::move(retrieve_passkeys_callback_).Run();
  }
}
