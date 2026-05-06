// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/actor/one_time_tokens/actor_one_time_token_filling_service_impl.h"

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
    const std::vector<::actor::PageTarget>& trigger_fields,
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

  // TODO(b/502907994): This is not the correct method to call! It's just a
  // convenient placeholder for now to have some call to the service in place
  // and set up the tests.
  std::vector<one_time_tokens::OneTimeToken> tokens =
      service->GetCachedOneTimeTokens();
  if (tokens.empty()) {
    std::move(callback).Run("");
  } else {
    std::move(callback).Run(tokens[0].value());
  }
}

void ActorOneTimeTokenFillingServiceImpl::FillOtp(
    const tabs::TabHandle tab_handle,
    const std::vector<::actor::PageTarget>& trigger_fields,
    const std::string& otp,
    base::OnceCallback<void(bool)> callback) {
  // TODO(b/502907696): This should use AutofillManager to do real filling.
  std::move(callback).Run(true);
}

}  // namespace autofill
