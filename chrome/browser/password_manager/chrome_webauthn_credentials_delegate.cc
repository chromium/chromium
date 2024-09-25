// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate.h"

#include <optional>

#include "base/base64.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "content/public/browser/web_contents.h"
#include "device/fido/fido_types.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/webauthn/authenticator_request_dialog_controller.h"
#include "chrome/browser/webauthn/authenticator_request_scheduler.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/webauthn/android/webauthn_request_delegate_android.h"
#endif

namespace {
using device::AuthenticatorType;
#if !BUILDFLAG(IS_ANDROID)
// `AuthenticatorRequestDialogModel` observed from `authenticator_observation_`
// may notify this class too soon, causing a flicker. Delay calling
// `passkey_selected_callback_` at least 300ms to avoid the flicker.
// TODO(crbug.com/332619045): Move this to a UI layer.
constexpr base::TimeDelta kFlickerDuration = base::Milliseconds(300);

bool IsGpmPasskeyAuthenticatorType(AuthenticatorType type) {
  return type == AuthenticatorType::kEnclave ||
         type == AuthenticatorType::kChromeOSPasskeys;
}

#endif  // !BUILDFLAG(IS_ANDROID)
}  // namespace

using password_manager::PasskeyCredential;
using OnPasskeySelectedCallback =
    password_manager::WebAuthnCredentialsDelegate::OnPasskeySelectedCallback;

ChromeWebAuthnCredentialsDelegate::ChromeWebAuthnCredentialsDelegate(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

ChromeWebAuthnCredentialsDelegate::~ChromeWebAuthnCredentialsDelegate() =
    default;

void ChromeWebAuthnCredentialsDelegate::LaunchSecurityKeyOrHybridFlow() {
#if !BUILDFLAG(IS_ANDROID)
  ChromeAuthenticatorRequestDelegate* authenticator_delegate =
      AuthenticatorRequestScheduler::GetRequestDelegate(web_contents_);
  if (!authenticator_delegate) {
    return;
  }
  authenticator_delegate->dialog_controller()
      ->TransitionToModalWebAuthnRequest();
#else
  if (WebAuthnRequestDelegateAndroid* delegate =
          WebAuthnRequestDelegateAndroid::GetRequestDelegate(web_contents_)) {
    delegate->ShowHybridSignIn();
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ChromeWebAuthnCredentialsDelegate::SelectPasskey(
    const std::string& backend_id,
    OnPasskeySelectedCallback callback) {
  // `backend_id` is the base64-encoded credential ID. See `PasskeyCredential`
  // for where these are encoded.
  std::optional<std::vector<uint8_t>> selected_credential_id =
      base::Base64Decode(backend_id);
  DCHECK(selected_credential_id);

#if BUILDFLAG(IS_ANDROID)
  std::move(callback).Run();
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
    std::move(callback).Run();
    return;
  }
  if (passkey_selected_callback_) {
    // The user tapped on another passkey while the enclave was loading. Ignore
    // the tap.
    // TODO(crbug.com/344950143): Disable the rows that are not supposed to be
    // clicked.
    return;
  }
  passkey_selected_callback_ = std::move(callback);
  authenticator_observation_.Observe(authenticator_delegate->dialog_model());
  AuthenticatorType credential_source =
      authenticator_delegate->dialog_controller()->OnAccountPreselected(
          *selected_credential_id);
  // If the credential is not from a GPM authenticator, we do not need to
  // observe the model anymore.
  if (!IsGpmPasskeyAuthenticatorType(credential_source)) {
    authenticator_observation_.Reset();
    std::move(passkey_selected_callback_).Run();
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

const std::optional<std::vector<PasskeyCredential>>&
ChromeWebAuthnCredentialsDelegate::GetPasskeys() const {
  return passkeys_;
}

base::WeakPtr<password_manager::WebAuthnCredentialsDelegate>
ChromeWebAuthnCredentialsDelegate::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool ChromeWebAuthnCredentialsDelegate::HasPendingPasskeySelection() {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  return !passkey_selected_callback_.is_null();
#endif  // BUILDFLAG(IS_ANDROID)
}

#if !BUILDFLAG(IS_ANDROID)
void ChromeWebAuthnCredentialsDelegate::OnStepTransition() {
  AuthenticatorRequestDialogModel* model =
      authenticator_observation_.GetSource();
  CHECK(model) << "The model just stepped but not registered as source.";
  if (!passkey_selected_callback_ || !model->preselected_cred.has_value() ||
      !IsGpmPasskeyAuthenticatorType(model->preselected_cred.value().source)) {
    return;
  }
  // Do not dismiss the autofill popup when the AuthenticatorRequestDialogModel
  // says that UI is disabled.
  if (!model->ui_disabled_) {
    authenticator_observation_.Reset();
    flickering_timer_.Start(FROM_HERE, kFlickerDuration,
                            std::move(passkey_selected_callback_));
  }
}
#endif  // !BUILDFLAG(IS_ANDROID)

bool ChromeWebAuthnCredentialsDelegate::IsSecurityKeyOrHybridFlowAvailable()
    const {
  return security_key_or_hybrid_flow_available_.value();
}

void ChromeWebAuthnCredentialsDelegate::RetrievePasskeys(
    base::OnceClosure callback) {
  passkey_retrieval_timer_ = std::make_unique<base::ElapsedTimer>();
  if (passkeys_.has_value()) {
    RecordPasskeyRetrievalDelay();
    // Entries were already populated from the WebAuthn request.
    std::move(callback).Run();
    return;
  }

  retrieve_passkeys_callback_ = std::move(callback);
}

void ChromeWebAuthnCredentialsDelegate::OnCredentialsReceived(
    std::vector<PasskeyCredential> credentials,
    SecurityKeyOrHybridFlowAvailable security_key_or_hybrid_flow_available) {
  passkeys_ = std::move(credentials);
  security_key_or_hybrid_flow_available_ =
      security_key_or_hybrid_flow_available;
  if (retrieve_passkeys_callback_) {
    RecordPasskeyRetrievalDelay();
    std::move(retrieve_passkeys_callback_).Run();
  }
}

void ChromeWebAuthnCredentialsDelegate::NotifyWebAuthnRequestAborted() {
  passkeys_ = std::nullopt;
  if (retrieve_passkeys_callback_) {
    std::move(retrieve_passkeys_callback_).Run();
  }
#if !BUILDFLAG(IS_ANDROID)
  // Also dismiss the autofill popup if it is being displayed and a webauthn
  // request is loading.
  if (passkey_selected_callback_) {
    flickering_timer_.Start(FROM_HERE, kFlickerDuration,
                            std::move(passkey_selected_callback_));
  }
  authenticator_observation_.Reset();
#endif  // BUILDFLAG(IS_ANDROID)
}

void ChromeWebAuthnCredentialsDelegate::RecordPasskeyRetrievalDelay() {
  if (passkey_retrieval_timer_) {
    base::UmaHistogramTimes("PasswordManager.PasskeyRetrievalWaitDuration",
                            passkey_retrieval_timer_->Elapsed());
    passkey_retrieval_timer_.reset();
  }
}
