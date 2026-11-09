// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/net/enterprise_proxy_tab_helper_delegate.h"

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/core_account_id.h"
#include "url/gurl.h"

namespace enterprise_net {

EnterpriseProxyTabHelperDelegate::EnterpriseProxyTabHelperDelegate()
    : EnterpriseProxyTabHelperDelegate(PlatformDelegate::Create()) {}

EnterpriseProxyTabHelperDelegate::EnterpriseProxyTabHelperDelegate(
    std::unique_ptr<PlatformDelegate> platform_delegate)
    : platform_delegate_(std::move(platform_delegate)) {}

EnterpriseProxyTabHelperDelegate::~EnterpriseProxyTabHelperDelegate() = default;

#if !BUILDFLAG(IS_ANDROID)
// static
std::unique_ptr<EnterpriseProxyTabHelperDelegate::PlatformDelegate>
EnterpriseProxyTabHelperDelegate::PlatformDelegate::Create() {
  // TODO(crbug.com/532559920): Implement desktop re-auth and login flow for
  // enterprise proxy.
  return nullptr;
}
#endif

void EnterpriseProxyTabHelperDelegate::SignIn(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  // This case should not happen for normal tab execution, but can be reached in
  // case of shutdown races. Defensively return instead of CHECK to avoid
  // crashing.
  if (!profile) {
    return;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    return;
  }

  const CoreAccountId primary_account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);

  // If the primary account is already signed in with a valid refresh token
  // (e.g. when the error page was force-triggered for testing or shown due to a
  // transient challenge), reload the page directly instead of starting an OS
  // credential update flow.
  if (!primary_account_id.empty() &&
      identity_manager->HasAccountWithRefreshToken(primary_account_id) &&
      !identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account_id)) {
    web_contents->GetController().Reload(content::ReloadType::NORMAL,
                                         /*check_for_repost=*/false);
    return;
  }

  if (!platform_delegate_) {
    return;
  }

  const GURL destination_url = web_contents->GetVisibleURL();
  if (primary_account_id.empty()) {
    platform_delegate_->StartSignInFlow(profile, web_contents, destination_url);
  } else {
    platform_delegate_->StartReauthFlow(profile, web_contents, destination_url,
                                        primary_account_id);
  }
}

}  // namespace enterprise_net
