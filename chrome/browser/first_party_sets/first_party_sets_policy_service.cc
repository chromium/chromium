// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"

#include "content/public/browser/first_party_sets_handler.h"
#include "services/network/public/mojom/first_party_sets_access_delegate.mojom.h"

namespace first_party_sets {

namespace {

network::mojom::FirstPartySetsReadyEventPtr MakeReadyEvent(
    FirstPartySetsPolicyService::PolicyCustomization customizations) {
  auto ready_event = network::mojom::FirstPartySetsReadyEvent::New();
  ready_event->customizations = std::move(customizations);
  return ready_event;
}

}  // namespace

FirstPartySetsPolicyService::FirstPartySetsPolicyService(
    content::BrowserContext* context,
    const base::Value::Dict& policy) {
  policy_ = policy.Clone();
  // Immediately send `policy` to the FirstPartySetsHandler to retrieve its
  // associated "ProfileCustomization". We can do this since the value of the
  // FirstPartySets Overrides policy doesn't dynamically refresh, and all
  // delegates for `context` will have the same `policy` and thus the same
  // customizations.
  content::FirstPartySetsHandler::GetInstance()->GetCustomizationForPolicy(
      policy_,
      base::BindOnce(&FirstPartySetsPolicyService::OnCustomizationsReady,
                     weak_factory_.GetWeakPtr()));
}

FirstPartySetsPolicyService::~FirstPartySetsPolicyService() = default;

void FirstPartySetsPolicyService::AddRemoteAccessDelegate(
    mojo::Remote<network::mojom::FirstPartySetsAccessDelegate>
        access_delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (customizations_.has_value()) {
    // Since the list of First-Party Sets is static after initialization and
    // the FirstPartySetsOverrides policy doesn't support dynamic refresh, a
    // profile's `customizations_` is static as well.
    access_delegate->NotifyReady(MakeReadyEvent(customizations_.value()));
    return;
  }
  access_delegates_.Add(std::move(access_delegate));
}

void FirstPartySetsPolicyService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  access_delegates_.Clear();
  weak_factory_.InvalidateWeakPtrs();
}

void FirstPartySetsPolicyService::OnCustomizationsReady(
    PolicyCustomization customizations) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  customizations_ = customizations;
  for (auto& delegate : access_delegates_) {
    delegate->NotifyReady(MakeReadyEvent(customizations_.value()));
  }
  access_delegates_.Clear();
}

}  // namespace first_party_sets
