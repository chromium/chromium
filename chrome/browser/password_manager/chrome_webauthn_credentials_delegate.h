// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_CHROME_WEBAUTHN_CREDENTIALS_DELEGATE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_CHROME_WEBAUTHN_CREDENTIALS_DELEGATE_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/strong_alias.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/webauthn_credentials_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class WebContents;
}

// Chrome implementation of WebAuthnCredentialsDelegate.
class ChromeWebAuthnCredentialsDelegate
    : public password_manager::WebAuthnCredentialsDelegate {
 public:
  using AndroidHybridAvailable =
      base::StrongAlias<struct AndroidHybridAvailableTag, bool>;

  explicit ChromeWebAuthnCredentialsDelegate(
      content::WebContents* web_contents);
  ~ChromeWebAuthnCredentialsDelegate() override;
  ChromeWebAuthnCredentialsDelegate(const ChromeWebAuthnCredentialsDelegate&) =
      delete;
  ChromeWebAuthnCredentialsDelegate operator=(
      const ChromeWebAuthnCredentialsDelegate&) = delete;

  // password_manager::WebAuthnCredentialsDelegate:
  void LaunchWebAuthnFlow() override;
  void SelectPasskey(const std::string& backend_id) override;
  const absl::optional<std::vector<password_manager::PasskeyCredential>>&
  GetPasskeys() const override;
  bool OfferPasskeysFromAnotherDeviceOption() const override;
  void RetrievePasskeys(base::OnceClosure callback) override;

  // Method for providing a list of WebAuthn user entities that can be provided
  // as autofill suggestions. This is called when a WebAuthn Conditional UI
  // request is received. The `offer_passkey_from_another_device` argument
  // determines whether an autofill option to use a passkey from another device
  // should be offered.
  void OnCredentialsReceived(
      std::vector<password_manager::PasskeyCredential> credentials,
      bool offer_passkey_from_another_device);

  // Lets the delegate know that a WebAuthn request has been aborted, and so
  // WebAuthn options should no longer show up on the autofill popup.
  void NotifyWebAuthnRequestAborted();

#if BUILDFLAG(IS_ANDROID)
  // password_manager::WebAuthnCredentialsDelegate:
  void ShowAndroidHybridSignIn() override;
  bool IsAndroidHybridAvailable() const override;

  // Sets the hybrid availability flag, which can be queried through
  // `IsAndroidHybridAvailable()`.
  void SetAndroidHybridAvailable(AndroidHybridAvailable available);
#endif

 protected:
  const raw_ptr<content::WebContents> web_contents_;

 private:
  // List of passkeys populated from an authenticator from a call to
  // RetrievePasskeys, and returned to the client via GetPasskeys.
  // |passkeys_| is nullopt until populated by a WebAuthn request, and reset
  // to nullopt when the request is cancelled.
  absl::optional<std::vector<password_manager::PasskeyCredential>> passkeys_;
  bool offer_passkey_from_another_device_ = true;

  base::OnceClosure retrieve_passkeys_callback_;

#if BUILDFLAG(IS_ANDROID)
  AndroidHybridAvailable android_hybrid_available_ =
      AndroidHybridAvailable(false);
#endif
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_CHROME_WEBAUTHN_CREDENTIALS_DELEGATE_H_
