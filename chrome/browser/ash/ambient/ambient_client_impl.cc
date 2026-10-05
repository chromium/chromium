// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/ambient/ambient_client_impl.h"

#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/image_downloader.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/channel/channel_info.h"
#include "chromeos/ash/components/demo_mode/utils/demo_session_utils.h"
#include "chromeos/ash/components/signin/identity_manager_provider.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/channel.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

constexpr net::NetworkTrafficAnnotationTag kAmbientClientNetworkTag =
    net::DefineNetworkTrafficAnnotation("ambient_client", R"(
        semantics {
          sender: "Ambient photo"
          description:
            "Get ambient photo from url to store limited number of photos in "
            "the device cache. This is used to show the screensaver when the "
            "user is idle. The url can be Backdrop service to provide pictures"
            " from internal gallery, weather/time photos served by Google, or "
            "user selected album from Google photos."
          trigger:
            "Triggered by a photo refresh timer, after the device has been "
            "idle and the battery is charging."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
         cookies_allowed: NO
         setting:
           "This feature is off by default and can be overridden by users."
         policy_exception_justification:
           "This feature is set by user settings.ambient_mode.enabled pref. "
           "The user setting is per device and cannot be overriden by admin."
        })");

bool HasPrimaryAccount(const AccountId& account_id) {
  if (!CHECK_DEREF(user_manager::UserManager::Get()->FindUser(account_id))
           .is_profile_created()) {
    return false;
  }

  auto* identity_manager = ash::IdentityManagerProvider::Get().Find(account_id);
  if (!identity_manager)
    return false;

  return identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
}

bool IsEmailDomainSupported(const AccountId& account_id) {
  const std::string email = account_id.GetUserEmail();
  CHECK(!email.empty(), base::NotFatalUntil::M160);

  constexpr char kGmailDomain[] = "gmail.com";
  constexpr char kGooglemailDomain[] = "googlemail.com";
  return (gaia::ExtractDomainName(email) == kGmailDomain ||
          gaia::ExtractDomainName(email) == kGooglemailDomain ||
          gaia::IsGoogleInternalAccountEmail(email));
}

}  // namespace

AmbientClientImpl::AmbientClientImpl() = default;

AmbientClientImpl::~AmbientClientImpl() = default;

bool AmbientClientImpl::IsAmbientModeAllowed() {
  if (is_allowed_for_testing_.has_value()) {
    return is_allowed_for_testing_.value();
  }

  if (ash::demo_mode::IsDeviceInDemoMode()) {
    return false;
  }

  const session_manager::Session* const active_session =
      session_manager::SessionManager::Get()->GetActiveSession();
  if (!active_session) {
    return false;
  }
  // TODO(crbug.com/278643115): Take the account_id from the callers.
  const AccountId& account_id = active_session->account_id();
  if (!CHECK_DEREF(user_manager::UserManager::Get()->FindUser(account_id))
           .HasGaiaAccount()) {
    return false;
  }

  if (account_id !=
      CHECK_DEREF(session_manager::SessionManager::Get()->GetPrimarySession())
          .account_id()) {
    return false;
  }

  // When this check is removed to start supporting enterprise users,
  // please update kAmbientClientNetworkTag and network annotation tags
  // in ash/ambient package to reflect that this is an enterprise feature.
  if (!IsEmailDomainSupported(account_id)) {
    return false;
  }

  // Primary account might be missing during unittests.
  if (!HasPrimaryAccount(account_id)) {
    return false;
  }

  auto* profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          account_id));
  if (!profile) {
    return false;
  }

  if (profile->IsOffTheRecord())
    return false;

  return true;
}

void AmbientClientImpl::SetAmbientModeAllowedForTesting(bool allowed) {
  is_allowed_for_testing_ = allowed;
}

void AmbientClientImpl::RequestAccessToken(GetAccessTokenCallback callback) {
  // TODO(crbug.com/278643115): Take the account_id from the callers.
  const session_manager::Session* const active_session =
      session_manager::SessionManager::Get()->GetActiveSession();
  CHECK(active_session, base::NotFatalUntil::M160);

  signin::IdentityManager* identity_manager =
      ash::IdentityManagerProvider::Get().Find(active_session->account_id());
  CHECK(identity_manager, base::NotFatalUntil::M160);

  CoreAccountInfo account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  auto fetcher_id = base::UnguessableToken::Create();
  auto access_token_fetcher =
      identity_manager->CreateAccessTokenFetcherForAccount(
          account_info.account_id, signin::OAuthConsumerId::kAmbientMode,
          base::BindOnce(&AmbientClientImpl::OnGetAccessToken,
                         weak_factory_.GetWeakPtr(), std::move(callback),
                         fetcher_id, account_info.gaia),
          signin::AccessTokenFetcher::Mode::kImmediate);

  token_fetchers_.insert({fetcher_id, std::move(access_token_fetcher)});
}

void AmbientClientImpl::DownloadImage(
    const std::string& url,
    ash::ImageDownloader::DownloadCallback callback) {
  // TODO(crbug.com/278643115): Take the account_id from the callers.
  const session_manager::Session* const active_session =
      session_manager::SessionManager::Get()->GetActiveSession();
  CHECK(active_session, base::NotFatalUntil::M160);
  // Bind the account now; the active session may change before the token
  // arrives.
  RequestAccessToken(base::BindOnce(
      [](const std::string& url, const AccountId& account_id,
         ash::ImageDownloader::DownloadCallback callback, const GaiaId& gaia_id,
         const std::string& access_token, const base::Time& expiration_time) {
        if (access_token.empty()) {
          std::move(callback).Run({});
          return;
        }
        net::HttpRequestHeaders headers;
        headers.SetHeader("Authorization", "Bearer " + access_token);
        ash::ImageDownloader::Get()->Download(
            GURL(url), kAmbientClientNetworkTag, account_id, headers,
            std::move(callback));
      },
      url, active_session->account_id(), std::move(callback)));
}

scoped_refptr<network::SharedURLLoaderFactory>
AmbientClientImpl::GetURLLoaderFactory() {
  // TODO(crbug.com/278643115): Take the account_id from the callers.
  const session_manager::Session* const active_session =
      session_manager::SessionManager::Get()->GetActiveSession();
  CHECK(active_session, base::NotFatalUntil::M160);
  auto* profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          active_session->account_id()));
  CHECK(profile, base::NotFatalUntil::M160);

  return profile->GetURLLoaderFactory();
}

scoped_refptr<network::SharedURLLoaderFactory>
AmbientClientImpl::GetSigninURLLoaderFactory() {
  content::BrowserContext* browser_context =
      ash::BrowserContextHelper::Get()->GetSigninBrowserContext();
  CHECK(browser_context);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  CHECK(profile);
  return profile->GetURLLoaderFactory();
}

void AmbientClientImpl::RequestWakeLockProvider(
    mojo::PendingReceiver<device::mojom::WakeLockProvider> receiver) {
  content::GetDeviceService().BindWakeLockProvider(std::move(receiver));
}

bool AmbientClientImpl::ShouldUseProdServer() {
  if (ash::features::IsAmbientModeDevUseProdEnabled())
    return true;

  auto channel = ash::GetChannel();
  return channel == version_info::Channel::STABLE ||
         channel == version_info::Channel::BETA;
}

void AmbientClientImpl::OnGetAccessToken(
    GetAccessTokenCallback callback,
    base::UnguessableToken fetcher_id,

    const GaiaId& gaia_id,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  if (error.state() == GoogleServiceAuthError::NONE) {
    std::move(callback).Run(gaia_id, access_token_info.token,
                            access_token_info.expiration_time);
  } else {
    LOG(ERROR) << "Failed to retrieve token, error: " << error.ToString();
    std::move(callback).Run(/*gaia_id=*/GaiaId(),
                            /*access_token=*/std::string(),
                            /*expiration_time=*/base::Time::Now());
  }

  token_fetchers_.erase(fetcher_id);
}
