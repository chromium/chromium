// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/actor/one_time_tokens/actor_one_time_token_filling_service_impl.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/autofill/one_time_token_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_service.h"
#include "components/tabs/public/tab_interface.h"

namespace autofill {

ActorOneTimeTokenFillingServiceImpl::ActorOneTimeTokenFillingServiceImpl(
    Profile* profile)
    : profile_(profile) {}

ActorOneTimeTokenFillingServiceImpl::~ActorOneTimeTokenFillingServiceImpl() =
    default;

void ActorOneTimeTokenFillingServiceImpl::RetrieveOtp(
    const tabs::TabHandle tab_handle,
    const std::vector<FieldGlobalId>& trigger_field_ids,
    base::OnceCallback<void(std::string)> callback) {
  tabs::TabInterface* tab = tab_handle.Get();
  if (!tab || !tab->GetContents()) {
    std::move(callback).Run("");
    return;
  }

  // TODO(b/502907994): Do we want to check for incognito profiles here?
  // Gemini should not be available in incognito, but should we check just to
  // be sure (and future-proof)?
  one_time_tokens::OneTimeTokenService* service =
      autofill::OneTimeTokenServiceFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run("");
    return;
  }

  // Note: OneTimeTokenService caches tokens for 3 minutes. It does not clear
  // them upon use. If a user triggers a "Resend OTP" flow within those 3
  // minutes, this will return the originally cached token rather than waiting
  // for the new one. This relies on the assumption that previously sent tokens
  // typically remain valid for the duration of the cache.
  std::optional<one_time_tokens::OneTimeToken> most_recent_token;
  for (const auto& token : service->GetCachedOneTimeTokens()) {
    if (token.type() == one_time_tokens::OneTimeTokenType::kGmail) {
      if (!most_recent_token ||
          token.on_device_arrival_time() >
              most_recent_token->on_device_arrival_time()) {
        most_recent_token = token;
      }
    }
  }

  if (most_recent_token) {
    subscription_ = {};
    // If there is a pending request, its callback is superseded. We run the
    // previous callback with an empty string so the old caller can gracefully
    // time out rather than hanging indefinitely.
    if (retrieve_otp_callback_) {
      std::move(retrieve_otp_callback_).Run("");
    }
    std::move(callback).Run(most_recent_token->value());
    return;
  }

  // If there is a pending request, its callback is superseded. We run the
  // previous callback with an empty string so the old caller can gracefully
  // time out rather than hanging indefinitely.
  if (retrieve_otp_callback_) {
    std::move(retrieve_otp_callback_).Run("");
  }
  retrieve_otp_callback_ = std::move(callback);

  // Subscribe to OneTimeTokenService with 1-minute timeout.
  subscription_ = service->Subscribe(
      one_time_tokens::OneTimeTokenSource::kGmail,
      base::Time::Now() + base::Minutes(1),
      base::BindRepeating(
          &ActorOneTimeTokenFillingServiceImpl::OnOneTimeTokenReceived,
          weak_ptr_factory_.GetWeakPtr()));
}

void ActorOneTimeTokenFillingServiceImpl::OnOneTimeTokenReceived(
    one_time_tokens::OneTimeTokenSource source,
    base::expected<one_time_tokens::OneTimeToken,
                   one_time_tokens::OneTimeTokenRetrievalError> result) {
  if (!retrieve_otp_callback_) {
    return;
  }

  subscription_ = {};

  std::move(retrieve_otp_callback_)
      .Run(result.has_value() ? result->value() : "");
}

void ActorOneTimeTokenFillingServiceImpl::FillOtp(
    const tabs::TabHandle tab_handle,
    const std::vector<FieldGlobalId>& trigger_field_ids,
    const std::string& otp,
    base::OnceCallback<void(bool)> callback) {
  // TODO(b/502907696): This should use AutofillManager to do real filling.
  std::move(callback).Run(true);
}

}  // namespace autofill
