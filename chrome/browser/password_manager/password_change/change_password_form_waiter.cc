// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/change_password_form_waiter.h"

#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/web_contents.h"

namespace {

using password_manager::PasswordFormCache;

PasswordFormCache& GetFormCache(content::WebContents* web_contents) {
  auto* client = static_cast<password_manager::PasswordManagerClient*>(
      ChromePasswordManagerClient::FromWebContents(web_contents));
  CHECK(client);
  CHECK(client->GetPasswordManager());

  auto* cache = client->GetPasswordManager()->GetPasswordFormCache();
  CHECK(cache);
  return *cache;
}

}  // namespace

ChangePasswordFormWaiter::ChangePasswordFormWaiter(
    content::WebContents* web_contents,
    PasswordFormFoundCallback callback)
    : web_contents_(web_contents->GetWeakPtr()),
      callback_(std::move(callback)) {
  GetFormCache(web_contents).SetObserver(weak_ptr_factory_.GetWeakPtr());
  if (web_contents->IsDocumentOnLoadCompletedInPrimaryMainFrame()) {
    DocumentOnLoadCompletedInPrimaryMainFrame();
  } else {
    Observe(web_contents);
  }
}

ChangePasswordFormWaiter::~ChangePasswordFormWaiter() {
  if (web_contents_) {
    GetFormCache(web_contents_.get()).ResetObserver();
  }
}

void ChangePasswordFormWaiter::OnPasswordFormParsed(
    password_manager::PasswordFormManager* form_manager) {
  CHECK(callback_);
  CHECK(form_manager);

  const password_manager::PasswordForm* parsed_form =
      form_manager->GetParsedObservedForm();
  CHECK(parsed_form);

  // New password field and password confirmation fields are indicators of
  // a change password form.
  if (!parsed_form->new_password_element_renderer_id ||
      !parsed_form->confirmation_password_element_renderer_id) {
    return;
  }

  // Do not invoke anything after calling the `callback_` as object might be
  // destroyed immediately after.
  std::move(callback_).Run(form_manager);
}

void ChangePasswordFormWaiter::DocumentOnLoadCompletedInPrimaryMainFrame() {
  if (timeout_timer_.IsRunning()) {
    return;
  }
  timeout_timer_.Start(
      FROM_HERE, ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout,
      this, &ChangePasswordFormWaiter::OnTimeout);
}

void ChangePasswordFormWaiter::OnTimeout() {
  CHECK(callback_);
  std::move(callback_).Run(nullptr);
}
