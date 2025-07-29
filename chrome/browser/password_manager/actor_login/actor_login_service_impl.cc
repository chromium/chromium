// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/actor_login_service_impl.h"

#include "base/functional/bind.h"
#include "base/task/current_thread.h"
#include "chrome/browser/password_manager/actor_login/internal/actor_login_delegate_impl.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_delegate.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace actor_login {

namespace {

ActorLoginDelegate* GetOrCreateDelegate(content::WebContents* web_contents) {
  password_manager::ContentPasswordManagerDriver* driver =
      password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
          web_contents->GetPrimaryMainFrame());
  return ActorLoginDelegateImpl::GetOrCreate(web_contents, driver->client());
}

}  // namespace

ActorLoginServiceImpl::ActorLoginServiceImpl() {
  actor_login_delegate_factory_ = base::BindRepeating(&GetOrCreateDelegate);
}

ActorLoginServiceImpl::~ActorLoginServiceImpl() = default;

void ActorLoginServiceImpl::GetCredentials(tabs::TabInterface* tab,
                                           CredentialsOrErrorReply callback) {
  CHECK(tab);

  content::WebContents* web_contents = tab->GetContents();
  if (!web_contents) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  base::unexpected(
                                      ActorLoginError::kInvalidTabInterface)));
    return;
  }

  // A single instance per WebContents to ensure that all service method calls
  // for a tab are managed through the same delegate instance.
  ActorLoginDelegate* delegate =
      actor_login_delegate_factory_.Run(web_contents);

  // Delegate the call to the `WebContents`-scoped delegate.
  delegate->GetCredentials(std::move(callback));
}

void ActorLoginServiceImpl::AttemptLogin(
    tabs::TabInterface* tab,
    const Credential& credential,
    LoginStatusResultOrErrorReply callback) {
  CHECK(tab);

  content::WebContents* web_contents = tab->GetContents();
  if (!web_contents) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  base::unexpected(
                                      ActorLoginError::kInvalidTabInterface)));
    return;
  }

  // A single instance per WebContents to ensure that all service method calls
  // for a tab are managed through the same delegate instance.
  ActorLoginDelegate* delegate =
      actor_login_delegate_factory_.Run(web_contents);

  // Delegate the call to the `WebContents`-scoped delegate.
  delegate->AttemptLogin(credential, std::move(callback));
}

void ActorLoginServiceImpl::SetActorLoginDelegateFactoryForTesting(
    base::RepeatingCallback<ActorLoginDelegate*(content::WebContents*)>
        factory) {
  actor_login_delegate_factory_ = factory;
}

}  // namespace actor_login
