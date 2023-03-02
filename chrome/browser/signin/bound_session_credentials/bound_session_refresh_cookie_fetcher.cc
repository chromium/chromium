// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/signin/public/base/signin_client.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

BoundSessionRefreshCookieFetcher::BoundSessionRefreshCookieFetcher(
    SigninClient* client,
    const GURL& url,
    const std::string& cookie_name)
    : client_(client), url_(url), cookie_name_(cookie_name) {}

BoundSessionRefreshCookieFetcher::~BoundSessionRefreshCookieFetcher() = default;

void BoundSessionRefreshCookieFetcher::Start(
    RefreshCookieCompleteCallback callback) {
  const base::TimeDelta kMaxAge = base::Minutes(10);
  DCHECK(!callback_);
  callback_ = std::move(callback);

  constexpr base::TimeDelta kFakeNetworkRequestEquivalentDelay(
      base::Milliseconds(100));
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &BoundSessionRefreshCookieFetcher::OnRefreshCookieCompleted,
          weak_ptr_factory_.GetWeakPtr(),
          CreateFakeCookie(base::Time::Now() + kMaxAge)),
      kFakeNetworkRequestEquivalentDelay);
}

void BoundSessionRefreshCookieFetcher::OnRefreshCookieCompleted(
    std::unique_ptr<net::CanonicalCookie> cookie) {
  InsertCookieInCookieJar(std::move(cookie));
}

void BoundSessionRefreshCookieFetcher::InsertCookieInCookieJar(
    std::unique_ptr<net::CanonicalCookie> cookie) {
  DCHECK(client_);
  base::OnceCallback<void(net::CookieAccessResult)> callback =
      base::BindOnce(&BoundSessionRefreshCookieFetcher::OnCookieSet,
                     weak_ptr_factory_.GetWeakPtr(), cookie->ExpiryDate());
  net::CookieOptions options;
  options.set_include_httponly();
  // Permit it to set a SameSite cookie if it wants to.
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());
  client_->GetCookieManager()->SetCanonicalCookie(
      *cookie, url_, options,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(callback),
          net::CookieAccessResult(net::CookieInclusionStatus(
              net::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR))));
}

void BoundSessionRefreshCookieFetcher::OnCookieSet(
    base::Time expiry_date,
    net::CookieAccessResult access_result) {
  bool success = access_result.status.IsInclude();
  if (!success) {
    std::move(callback_).Run(absl::nullopt);
  } else {
    std::move(callback_).Run(expiry_date);
  }
  // |This| may be destroyed
}

std::unique_ptr<net::CanonicalCookie>
BoundSessionRefreshCookieFetcher::CreateFakeCookie(
    base::Time cookie_expiration) {
  constexpr char kFakeCookieValue[] = "FakeCookieValue";

  base::Time now = base::Time::Now();
  // Create fake SIDTS cookie until the server endpoint is available.
  std::unique_ptr<net::CanonicalCookie> new_cookie =
      net::CanonicalCookie::CreateSanitizedCookie(
          /*url=*/url_, /*name=*/cookie_name_,
          /*value=*/kFakeCookieValue,
          /*domain=*/url_.host(), /*path=*/"/",
          /*creation_time=*/now, /*expiration_time=*/cookie_expiration,
          /*last_access_time=*/now, /*secure=*/true,
          /*http_only=*/true, net::CookieSameSite::UNSPECIFIED,
          net::CookiePriority::COOKIE_PRIORITY_HIGH,
          /*same_party=*/true, /*partition_key=*/absl::nullopt);

  DCHECK(new_cookie);
  return new_cookie;
}
