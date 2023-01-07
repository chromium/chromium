// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_fetcher.h"

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/signin/public/base/signin_client.h"
#include "google_apis/gaia/gaia_urls.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

BoundSessionCookieFetcher::BoundSessionCookieFetcher(
    SigninClient* client,
    SetCookieCompleteCallback callback)
    : client_(client), callback_(std::move(callback)) {
  constexpr base::TimeDelta kFakeNetworkRequestEquivalentDelay(
      base::Milliseconds(100));
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&BoundSessionCookieFetcher::StartSettingCookie,
                     weak_ptr_factory_.GetWeakPtr()),
      kFakeNetworkRequestEquivalentDelay);
}

void BoundSessionCookieFetcher::StartSettingCookie() {
  constexpr char kSIDTSCookieName[] = "__Secure-1PSIDTS";
  constexpr char kFakeCookieValue[] = "FakeCookieValue";
  const base::TimeDelta kMaxAge = base::Minutes(10);

  base::Time now = base::Time::Now();
  base::Time expiration = now + kMaxAge;
  GURL google_url = GaiaUrls::GetInstance()->secure_google_url();

  // Create fake SIDTS cookie until the server endpoint is available.
  std::unique_ptr<net::CanonicalCookie> new_cookie =
      net::CanonicalCookie::CreateSanitizedCookie(
          /*url=*/google_url, /*name=*/kSIDTSCookieName,
          /*value=*/kFakeCookieValue,
          /*domain=*/google_url.host(), /*path=*/"/",
          /*creation_time=*/now, /*expiration_time=*/expiration,
          /*last_access_time=*/now, /*secure=*/true,
          /*http_only=*/true, net::CookieSameSite::UNSPECIFIED,
          net::CookiePriority::COOKIE_PRIORITY_HIGH,
          /*same_party=*/true, /*partition_key=*/absl::nullopt);

  DCHECK(new_cookie);
  base::OnceCallback<void(net::CookieAccessResult)> callback =
      base::BindOnce(&BoundSessionCookieFetcher::OnCookieSet,
                     weak_ptr_factory_.GetWeakPtr(), new_cookie->ExpiryDate());
  net::CookieOptions options;
  options.set_include_httponly();
  // Permit it to set a SameSite cookie if it wants to.
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());
  client_->GetCookieManager()->SetCanonicalCookie(
      *new_cookie, google_url, options,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(callback),
          net::CookieAccessResult(net::CookieInclusionStatus(
              net::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR))));
}

void BoundSessionCookieFetcher::OnCookieSet(
    const base::Time& expiry_date,
    net::CookieAccessResult access_result) {
  bool success = access_result.status.IsInclude();
  if (!success) {
    std::move(callback_).Run(absl::nullopt);
  } else {
    std::move(callback_).Run(expiry_date);
  }
  // |This| may be destroyed
}

BoundSessionCookieFetcher::~BoundSessionCookieFetcher() = default;
