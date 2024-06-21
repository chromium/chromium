// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/fake_bound_session_refresh_cookie_fetcher.h"

#include <optional>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"
#include "components/signin/public/base/signin_client.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

FakeBoundSessionRefreshCookieFetcher::FakeBoundSessionRefreshCookieFetcher(
    network::mojom::CookieManager* cookie_manager,
    const GURL& url,
    base::flat_set<std::string> cookie_names,
    std::optional<base::TimeDelta> unlock_automatically_in)
    : cookie_manager_(cookie_manager),
      url_(url),
      cookie_names_(std::move(cookie_names)),
      unlock_automatically_in_(unlock_automatically_in) {
  CHECK(cookie_manager_);
}

FakeBoundSessionRefreshCookieFetcher::~FakeBoundSessionRefreshCookieFetcher() =
    default;

void FakeBoundSessionRefreshCookieFetcher::Start(
    RefreshCookieCompleteCallback callback,
    std::optional<std::string> sec_session_challenge_response) {
  DCHECK(!callback_);
  callback_ = std::move(callback);
  sec_session_challenge_response_ = std::move(sec_session_challenge_response);

  if (unlock_automatically_in_.has_value()) {
    const base::TimeDelta kMaxAge = base::Minutes(10);
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeBoundSessionRefreshCookieFetcher::
                           SimulateCompleteRefreshRequest,
                       weak_ptr_factory_.GetWeakPtr(),
                       BoundSessionRefreshCookieFetcher::Result::kSuccess,
                       base::Time::Now() + kMaxAge),
        unlock_automatically_in_.value());
  }
}

bool FakeBoundSessionRefreshCookieFetcher::IsChallengeReceived() const {
  return false;
}

std::optional<std::string>
FakeBoundSessionRefreshCookieFetcher::TakeSecSessionChallengeResponseIfAny() {
  std::optional<std::string> response =
      std::move(sec_session_challenge_response_);
  sec_session_challenge_response_.reset();
  return response;
}

void FakeBoundSessionRefreshCookieFetcher::set_sec_session_challenge_response(
    std::string sec_session_challenge_response) {
  sec_session_challenge_response_ = std::move(sec_session_challenge_response);
}

void FakeBoundSessionRefreshCookieFetcher::SimulateCompleteRefreshRequest(
    BoundSessionRefreshCookieFetcher::Result result,
    std::optional<base::Time> cookie_expiration) {
  if (result == BoundSessionRefreshCookieFetcher::Result::kSuccess) {
    CHECK(cookie_expiration);
    // Synchronous since tests use `BoundSessionTestCookieManager`.
    std::vector<std::unique_ptr<net::CanonicalCookie>> new_cookies;
    for (const auto& cookie_name : cookie_names_) {
      new_cookies.emplace_back(
          CreateFakeCookie(cookie_name, cookie_expiration.value()));
    }
    OnRefreshCookieCompleted(std::move(new_cookies));
  } else {
    std::move(callback_).Run(result);
  }
}

void FakeBoundSessionRefreshCookieFetcher::OnRefreshCookieCompleted(
    std::vector<std::unique_ptr<net::CanonicalCookie>> cookies) {
  ResetCallbackCounter();
  for (auto& cookie : cookies) {
    InsertCookieInCookieJar(std::move(cookie));
  }
}

void FakeBoundSessionRefreshCookieFetcher::InsertCookieInCookieJar(
    std::unique_ptr<net::CanonicalCookie> cookie) {
  DCHECK(cookie_manager_);
  base::OnceCallback<void(net::CookieAccessResult)> callback =
      base::BindOnce(&FakeBoundSessionRefreshCookieFetcher::OnCookieSet,
                     weak_ptr_factory_.GetWeakPtr());
  net::CookieOptions options;
  options.set_include_httponly();
  // Permit it to set a SameSite cookie if it wants to.
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());
  cookie_manager_->SetCanonicalCookie(
      *cookie, url_, options,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(callback),
          net::CookieAccessResult(net::CookieInclusionStatus(
              net::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR))));
}

void FakeBoundSessionRefreshCookieFetcher::OnCookieSet(
    net::CookieAccessResult access_result) {
  callback_counter_++;
  if (callback_counter_ != cookie_names_.size()) {
    return;
  }

  bool success = access_result.status.IsInclude();
  if (!success) {
    std::move(callback_).Run(
        BoundSessionRefreshCookieFetcher::Result::kServerPersistentError);
  } else {
    std::move(callback_).Run(
        BoundSessionRefreshCookieFetcher::Result::kSuccess);
  }
  // |This| may be destroyed
}

void FakeBoundSessionRefreshCookieFetcher::ResetCallbackCounter() {
  callback_counter_ = 0;
}

std::unique_ptr<net::CanonicalCookie>
FakeBoundSessionRefreshCookieFetcher::CreateFakeCookie(
    const std::string& cookie_name,
    base::Time cookie_expiration) {
  constexpr char kFakeCookieValue[] = "FakeCookieValue";

  base::Time now = base::Time::Now();
  // Create fake SIDTS cookie until the server endpoint is available.
  std::unique_ptr<net::CanonicalCookie> new_cookie =
      net::CanonicalCookie::CreateSanitizedCookie(
          /*url=*/url_, /*name=*/cookie_name,
          /*value=*/kFakeCookieValue,
          /*domain=*/url_.host(), /*path=*/"/",
          /*creation_time=*/now, /*expiration_time=*/cookie_expiration,
          /*last_access_time=*/now, /*secure=*/true,
          /*http_only=*/true, net::CookieSameSite::UNSPECIFIED,
          net::CookiePriority::COOKIE_PRIORITY_HIGH,
          /*partition_key=*/std::nullopt, /*status=*/nullptr);

  DCHECK(new_cookie);
  return new_cookie;
}
