// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "content/public/common/content_features.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/webauthn/authenticator_request_scheduler.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#endif  // !defined(OS_ANDROID)

ChromeWebAuthnCredentialsDelegate::ChromeWebAuthnCredentialsDelegate(
    ChromePasswordManagerClient* client)
    : client_(client) {}

bool ChromeWebAuthnCredentialsDelegate::IsWebAuthnAutofillEnabled() const {
  return base::FeatureList::IsEnabled(features::kWebAuthConditionalUI);
}

void ChromeWebAuthnCredentialsDelegate::SelectWebAuthnCredential(
    std::string backend_id) {
#if !defined(OS_ANDROID)
  ChromeAuthenticatorRequestDelegate* authenticator_delegate =
      AuthenticatorRequestScheduler::GetRequestDelegate(
          client_->web_contents());
  if (!authenticator_delegate) {
    return;
  }
  authenticator_delegate->dialog_model()->OnAccountPreselected(
      std::vector<uint8_t>(backend_id.begin(), backend_id.end()));
#endif  // !defined(OS_ANDROID)
}

std::vector<autofill::Suggestion>
ChromeWebAuthnCredentialsDelegate::GetWebAuthnSuggestions() const {
#if defined(OS_ANDROID)
  return {};
#else
  ChromeAuthenticatorRequestDelegate* authenticator_delegate =
      AuthenticatorRequestScheduler::GetRequestDelegate(
          client_->web_contents());
  if (!authenticator_delegate) {
    return {};
  }
  std::vector<autofill::Suggestion> suggestions;
  for (const auto& credential :
       authenticator_delegate->dialog_model()->users()) {
    std::u16string name;
    if (credential.display_name && !credential.display_name->empty()) {
      name = base::UTF8ToUTF16(*credential.display_name);
    } else {
      // TODO(crbug.com/1179014): go through UX review, choose a string, and
      // i18n it.
      name = u"Unknown account";
    }
    autofill::Suggestion suggestion(std::move(name));
    if (credential.name) {
      suggestion.label = base::UTF8ToUTF16(*credential.name);
    }
    suggestion.icon = "fingerprint";
    suggestion.frontend_id = autofill::POPUP_ITEM_ID_WEBAUTHN_CREDENTIAL;
    suggestion.backend_id =
        std::string(credential.id.begin(), credential.id.end());
    suggestions.push_back(std::move(suggestion));
  }
  return suggestions;
#endif
}
