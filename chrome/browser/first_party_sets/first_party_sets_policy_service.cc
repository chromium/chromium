// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"

#include "base/stl_util.h"
#include "base/types/optional_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "services/network/public/mojom/first_party_sets_access_delegate.mojom.h"

namespace first_party_sets {

namespace {

network::mojom::FirstPartySetsReadyEventPtr MakeReadyEvent(
    net::FirstPartySetsContextConfig config) {
  auto ready_event = network::mojom::FirstPartySetsReadyEvent::New();
  ready_event->config = std::move(config);
  return ready_event;
}

}  // namespace

FirstPartySetsPolicyService::FirstPartySetsPolicyService(
    content::BrowserContext* browser_context,
    const base::Value::Dict& policy)
    : browser_context_(browser_context) {
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
  if (config_.has_value()) {
    // Since the list of First-Party Sets is static after initialization and
    // the FirstPartySetsOverrides policy doesn't support dynamic refresh, a
    // profile's `config_` is static as well.
    access_delegate->NotifyReady(MakeReadyEvent(config_->Clone()));
    return;
  }
  access_delegates_.Add(std::move(access_delegate));
}

void FirstPartySetsPolicyService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  access_delegates_.Clear();
  browser_context_ = nullptr;
  weak_factory_.InvalidateWeakPtrs();
}

void FirstPartySetsPolicyService::OnCustomizationsReady(
    net::FirstPartySetsContextConfig config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  config_ = std::move(config);

  // Representation of the current profile to be persisted on disk.
  const std::string browser_context_id =
      Profile::FromBrowserContext(browser_context_)
          ->GetBaseName()
          .AsUTF8Unsafe();

  base::RepeatingCallback<content::BrowserContext*()> browser_context_getter =
      base::BindRepeating(
          [](base::WeakPtr<FirstPartySetsPolicyService> weak_ptr) {
            return weak_ptr ? weak_ptr->browser_context() : nullptr;
          },
          weak_factory_.GetWeakPtr());

  content::FirstPartySetsHandler::GetInstance()
      ->ClearSiteDataOnChangedSetsForContext(
          browser_context_getter, browser_context_id,
          base::OptionalToPtr(config_),
          base::BindOnce(&FirstPartySetsPolicyService::OnSiteDataCleared,
                         weak_factory_.GetWeakPtr()));
}

void FirstPartySetsPolicyService::OnSiteDataCleared() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& delegate : access_delegates_) {
    delegate->NotifyReady(MakeReadyEvent(config_.value().Clone()));
  }
  access_delegates_.Clear();
}

}  // namespace first_party_sets
