// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/password_change_from_checkup_delegate.h"

#include <string_view>

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace {

std::string GetPromptToReachForm(std::string_view domain,
                                 std::string_view username) {
  return base::StrCat(
      {"I want to change my password in ", domain,
       ". Help me login to this site and reach the change password form. "
       "You can login using ",
       username,
       ". The task is finished when the change password form is visible."});
}

}  // namespace

PasswordChangeFromCheckupDelegate::PasswordChangeFromCheckupDelegate() =
    default;

PasswordChangeFromCheckupDelegate::~PasswordChangeFromCheckupDelegate() =
    default;

void PasswordChangeFromCheckupDelegate::StartPasswordChangeFlow(
    const password_manager::CredentialUIEntry& credential,
    base::WeakPtr<content::WebContents> web_contents) {
  if (!web_contents) {
    return;
  }

  tabs::TabInterface* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(web_contents.get());
  if (!tab_interface) {
    return;
  }

  originator_ = std::move(web_contents);
  glic::GlicKeyedService* glic_service = GetGlicService();
  if (!glic_service) {
    return;
  }

  // TODO(crbug.com/485620841): Handle Android passwords.
  std::string site_domain(credential.GetURL().host());
  // TODO(crbug.com/485620841): Replace with internal prompt.
  std::string prompt =
      GetPromptToReachForm(site_domain, base::UTF16ToUTF8(credential.username));

  glic::GlicInvokeOptions options(glic::mojom::InvocationSource::kSharedTab);
  options.prompts.push_back(std::move(prompt));
  glic_service->InvokeWithAutoSubmit(
      glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey(), tab_interface,
      std::move(options));
}

glic::GlicKeyedService* PasswordChangeFromCheckupDelegate::GetGlicService() {
  if (!originator_) {
    return nullptr;
  }
  return glic::GlicKeyedService::Get(
      Profile::FromBrowserContext(originator_->GetBrowserContext()));
}
