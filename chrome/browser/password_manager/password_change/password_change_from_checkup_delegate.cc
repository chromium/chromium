// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/password_change_from_checkup_delegate.h"

#include <string_view>

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {

std::string GetPromptToReachForm(std::string_view domain,
                                 std::string_view username) {
  return base::StrCat(
      {"I want to change my password for ", domain,
       ". Please help me complete the password change flow starting from "
       "this current page. You can login using ",
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
  originator_ = std::move(web_contents);

  actor::ActorKeyedService* actor_service = actor::ActorKeyedService::Get(
      Profile::FromBrowserContext(originator_->GetBrowserContext()));
  if (actor_service && actor_service->GetActiveTasksCount() > 0) {
    return;
  }

  tabs::TabInterface* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(originator_.get());
  if (!tab_interface) {
    return;
  }

  glic::GlicKeyedService* glic_service = GetGlicService();
  if (!glic_service) {
    return;
  }

  // TODO(crbug.com/485620841): Handle non-web URLs for Android passwords.
  const GURL& credential_url = credential.GetURL();
  std::string site_domain(credential_url.host());

  // TODO(crbug.com/485620841): Replace with internal prompt.
  std::string prompt =
      GetPromptToReachForm(site_domain, base::UTF16ToUTF8(credential.username));
  content::OpenURLParams open_url_params(
      credential_url.GetWithEmptyPath(), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
      /*is_renderer_initiated=*/false);

  content::WebContents* new_contents =
      originator_->OpenURL(open_url_params, /*navigation_handle_callback=*/{});

  if (!new_contents) {
    return;
  }

  glic::GlicInvokeOptions options(glic::mojom::InvocationSource::kSharedTab);
  options.prompts.push_back(std::move(prompt));
  options.additional_context = glic::mojom::AdditionalContext::New();

  sessions::SessionTabHelper* session_tab_helper =
      sessions::SessionTabHelper::FromWebContents(new_contents);
  if (session_tab_helper) {
    options.additional_context->tab_id = session_tab_helper->session_id().id();
  }

  tabs::TabInterface* new_tab_interface =
      tabs::TabInterface::MaybeGetFromContents(new_contents);

  if (!new_tab_interface) {
    return;
  }

  glic_service->InvokeWithAutoSubmit(
      glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
      new_tab_interface, std::move(options));
}

glic::GlicKeyedService* PasswordChangeFromCheckupDelegate::GetGlicService() {
  if (!originator_) {
    return nullptr;
  }
  return glic::GlicKeyedService::Get(
      Profile::FromBrowserContext(originator_->GetBrowserContext()));
}
