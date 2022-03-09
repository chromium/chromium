// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_CHROME_WEBAUTHN_CREDENTIALS_DELEGATE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_CHROME_WEBAUTHN_CREDENTIALS_DELEGATE_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/webauthn_credentials_delegate.h"

namespace device {
class PublicKeyCredentialUserEntity;
}

class ChromePasswordManagerClient;

// Chrome implementation of WebAuthnCredentialsDelegate.
class ChromeWebAuthnCredentialsDelegate
    : public password_manager::WebAuthnCredentialsDelegate {
 public:
  explicit ChromeWebAuthnCredentialsDelegate(
      ChromePasswordManagerClient* client);
  ~ChromeWebAuthnCredentialsDelegate() override;
  ChromeWebAuthnCredentialsDelegate(const ChromeWebAuthnCredentialsDelegate&) =
      delete;
  ChromeWebAuthnCredentialsDelegate operator=(
      const ChromeWebAuthnCredentialsDelegate&) = delete;
  ChromeWebAuthnCredentialsDelegate(ChromeWebAuthnCredentialsDelegate&&) =
      delete;

  // password_manager::WebAuthnCredentialsDelegate:
  bool IsWebAuthnAutofillEnabled() const override;
  void SelectWebAuthnCredential(std::string backend_id) override;
  const std::vector<autofill::Suggestion>& GetWebAuthnSuggestions()
      const override;
  void RetrieveWebAuthnSuggestions(base::OnceClosure callback) override;

 protected:
  const raw_ptr<ChromePasswordManagerClient> client_;

 private:
  // Callback for providing a list of WebAuthn user entities that can be
  // provided as autofill suggestions.
  void OnCredentialsReceived(
      const std::vector<device::PublicKeyCredentialUserEntity>& credentials);

  // List of autofill suggestions populated from an authenticator from a call
  // to RetrieveWebAuthnSuggestions, and returned to the client via
  // GetWebAuthnSuggestions.
  std::vector<autofill::Suggestion> suggestions_;

  base::OnceClosure retrieve_suggestions_callback_;

  base::WeakPtrFactory<ChromeWebAuthnCredentialsDelegate> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_CHROME_WEBAUTHN_CREDENTIALS_DELEGATE_H_
