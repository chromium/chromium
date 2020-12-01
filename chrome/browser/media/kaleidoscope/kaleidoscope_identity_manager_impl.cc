// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/kaleidoscope/kaleidoscope_identity_manager_impl.h"

#include "base/callback.h"
#include "chrome/browser/media/kaleidoscope/kaleidoscope_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/channel_info.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/google_api_keys.h"

namespace {

constexpr char kChromeMediaRecommendationsOAuth2Scope[] =
    "https://www.googleapis.com/auth/chrome-media-recommendations";

}  // namespace

KaleidoscopeIdentityManagerImpl::KaleidoscopeIdentityManagerImpl(
    mojo::PendingReceiver<media::mojom::KaleidoscopeIdentityManager> receiver,
    content::WebUI* web_ui)
    : KaleidoscopeIdentityManagerImpl(std::move(receiver),
                                      Profile::FromWebUI(web_ui)) {
  DCHECK(web_ui);
  web_ui_ = web_ui;
}

KaleidoscopeIdentityManagerImpl::KaleidoscopeIdentityManagerImpl(
    mojo::PendingReceiver<media::mojom::KaleidoscopeIdentityManager> receiver,
    Profile* profile)
    : credentials_(media::mojom::Credentials::New()),
      profile_(profile),
      receiver_(this, std::move(receiver)) {
  DCHECK(profile);

  // If this is Google Chrome then we should use the official API key.
  if (google_apis::IsGoogleChromeAPIKeyUsed()) {
    bool is_stable_channel =
        chrome::GetChannel() == version_info::Channel::STABLE;
    credentials_->api_key = is_stable_channel
                                ? google_apis::GetAPIKey()
                                : google_apis::GetNonStableAPIKey();
  }

  identity_manager_ = IdentityManagerFactory::GetForProfile(profile);
  identity_manager_->AddObserver(this);
}

KaleidoscopeIdentityManagerImpl::~KaleidoscopeIdentityManagerImpl() {
  identity_manager_->RemoveObserver(this);
}

void KaleidoscopeIdentityManagerImpl::GetCredentials(
    GetCredentialsCallback cb) {
  // If the profile is incognito then disable Kaleidoscope.
  if (profile_->IsOffTheRecord()) {
    std::move(cb).Run(nullptr,
                      media::mojom::CredentialsResult::kFailedIncognito);
    return;
  }

  // If the profile is a child then disable Kaleidoscope.
  if (profile_->IsSupervised() || profile_->IsChild()) {
    std::move(cb).Run(nullptr, media::mojom::CredentialsResult::kFailedChild);
    return;
  }

  // If the administrator has disabled Kaleidoscope then stop.
  auto* prefs = profile_->GetPrefs();
  if (!prefs->GetBoolean(kaleidoscope::prefs::kKaleidoscopePolicyEnabled)) {
    std::move(cb).Run(nullptr,
                      media::mojom::CredentialsResult::kDisabledByPolicy);
    return;
  }

  // If the user is not signed in, return the credentials without an access
  // token. Sync consent is not required to use Kaleidoscope.
  if (!identity_manager_->HasPrimaryAccount(
          signin::ConsentLevel::kNotRequired)) {
    std::move(cb).Run(credentials_.Clone(),
                      media::mojom::CredentialsResult::kSuccess);
    return;
  }

  pending_callbacks_.push_back(std::move(cb));

  // Get an OAuth token for the backend API. This token will be limited to just
  // our backend scope. Destroying |token_fetcher_| will cancel the fetch so
  // unretained is safe here.
  signin::ScopeSet scopes = {kChromeMediaRecommendationsOAuth2Scope};
  token_fetcher_ = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
      "kaleidoscope_service", identity_manager_, scopes,
      base::BindOnce(&KaleidoscopeIdentityManagerImpl::OnAccessTokenAvailable,
                     base::Unretained(this)),
      signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
      signin::ConsentLevel::kNotRequired);
}

void KaleidoscopeIdentityManagerImpl::OnAccessTokenAvailable(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  DCHECK(token_fetcher_);
  token_fetcher_.reset();

  if (error.state() == GoogleServiceAuthError::State::NONE) {
    credentials_->access_token = access_token_info.token;
    credentials_->expiry_time = access_token_info.expiration_time;
  }

  for (auto& callback : pending_callbacks_) {
    std::move(callback).Run(credentials_.Clone(),
                            media::mojom::CredentialsResult::kSuccess);
  }

  pending_callbacks_.clear();
}

void KaleidoscopeIdentityManagerImpl::SignIn() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  NOTREACHED() << "SignIn() shouldn't be called on Chrome OS.";
#else
  // The identity manager may not be created in a context where it has access
  // to the UI. The client should not request a sign-in in that case.
  DCHECK(web_ui_);

  content::WebContents* web_contents = web_ui_->GetWebContents();
  DCHECK(web_contents);

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  DCHECK(browser);

  chrome::ShowBrowserSignin(
      browser, signin_metrics::AccessPoint::ACCESS_POINT_KALEIDOSCOPE,
      signin::ConsentLevel::kNotRequired);
  signin_metrics::RecordSigninImpressionUserActionForAccessPoint(
      signin_metrics::AccessPoint::ACCESS_POINT_KALEIDOSCOPE);
#endif  // defined(OS_CHROMEOS)
}

void KaleidoscopeIdentityManagerImpl::AddObserver(
    mojo::PendingRemote<media::mojom::KaleidoscopeIdentityObserver> observer) {
  identity_observers_.Add(std::move(observer));
}

void KaleidoscopeIdentityManagerImpl::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  for (auto& observer : identity_observers_)
    observer->OnSignedIn();
}

void KaleidoscopeIdentityManagerImpl::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  for (auto& observer : identity_observers_)
    observer->OnSignedOut();
}
