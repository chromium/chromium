// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/fake_bound_session_refresh_cookie_fetcher.h"

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/signin/public/base/signin_client.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

FakeBoundSessionRefreshCookieFetcher::FakeBoundSessionRefreshCookieFetcher(
    SigninClient* client,
    const GURL& url,
    const std::string& cookie_name,
    absl::optional<base::TimeDelta> unlock_automatically_in)
    : client_(client),
      url_(url),
      cookie_name_(cookie_name),
      unlock_automatically_in_(unlock_automatically_in) {}

FakeBoundSessionRefreshCookieFetcher::~FakeBoundSessionRefreshCookieFetcher() =
    default;

void FakeBoundSessionRefreshCookieFetcher::Start(
    RefreshCookieCompleteCallback callback) {
  DCHECK(!callback_);
  callback_ = std::move(callback);

  if (unlock_automatically_in_.has_value()) {
    const base::TimeDelta kMaxAge = base::Minutes(10);
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeBoundSessionRefreshCookieFetcher::
                           SimulateCompleteRefreshRequest,
                       weak_ptr_factory_.GetWeakPtr(),
                       base::Time::Now() + kMaxAge),
        unlock_automatically_in_.value());
  }
}

void FakeBoundSessionRefreshCookieFetcher::SimulateCompleteRefreshRequest(
    absl::optional<base::Time> cookie_expiration) {
  if (cookie_expiration.has_value()) {
    // Synchronous since tests use `BoundSessionTestCookieManager`.
    OnRefreshCookieCompleted(CreateFakeCookie(cookie_expiration.value()));
  } else {
    std::move(callback_).Run(Result(net::Error::OK, net::HTTP_FORBIDDEN));
  }
}

void FakeBoundSessionRefreshCookieFetcher::OnRefreshCookieCompleted(
    std::unique_ptr<net::CanonicalCookie> cookie) {
  InsertCookieInCookieJar(std::move(cookie));
}

void FakeBoundSessionRefreshCookieFetcher::InsertCookieInCookieJar(
    std::unique_ptr<net::CanonicalCookie> cookie) {
  DCHECK(client_);
  base::OnceCallback<void(net::CookieAccessResult)> callback =
      base::BindOnce(&FakeBoundSessionRefreshCookieFetcher::OnCookieSet,
                     weak_ptr_factory_.GetWeakPtr());
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

void FakeBoundSessionRefreshCookieFetcher::OnCookieSet(
    net::CookieAccessResult access_result) {
  bool success = access_result.status.IsInclude();
  if (!success) {
    std::move(callback_).Run(Result(net::Error::OK, net::HTTP_FORBIDDEN));
  } else {
    std::move(callback_).Run(Result(net::Error::OK, net::HTTP_OK));
  }
  // |This| may be destroyed
}

std::unique_ptr<net::CanonicalCookie>
FakeBoundSessionRefreshCookieFetcher::CreateFakeCookie(
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
