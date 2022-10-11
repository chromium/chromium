// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"

#include "chrome/browser/first_party_sets/first_party_sets_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/remote.h"
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

const base::Value::Dict* GetOverridesPolicyForProfile(
    const PrefService* prefs) {
  return prefs ? &prefs->GetDict(first_party_sets::kFirstPartySetsOverrides)
               : nullptr;
}

bool GetEnabledPolicyForProfile(const PrefService* prefs) {
  return prefs &&
         prefs->GetBoolean(prefs::kPrivacySandboxFirstPartySetsEnabled);
}

}  // namespace

FirstPartySetsPolicyService::FirstPartySetsPolicyService(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(browser_context);

  if (!base::FeatureList::IsEnabled(features::kFirstPartySets)) {
    config_ = net::FirstPartySetsContextConfig();
    return;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context);
  // profile is guaranteed to be non-null since we create this service with a
  // non-null `context`.
  DCHECK(profile);
  // Checks that `profile` isn't a system profile or a guest profile before
  // returning a pointer to the value of the First-Party Sets Overrides policy.
  if (profile->IsSystemProfile() || profile->IsGuestSession()) {
    config_ = net::FirstPartySetsContextConfig();
    return;
  }

  // Immediately send `policy` to the FirstPartySetsHandler to retrieve its
  // associated FirstPartySetsContextConfig. We can do this since the value of
  // the FirstPartySets Overrides policy doesn't dynamically refresh, and all
  // delegates for `context` will have the same `policy` and thus the same
  // config.
  PrefService* prefs = profile->GetPrefs();
  content::FirstPartySetsHandler::GetInstance()->GetContextConfigForPolicy(
      GetOverridesPolicyForProfile(prefs),
      base::BindOnce(&FirstPartySetsPolicyService::OnProfileConfigReady,
                     weak_factory_.GetWeakPtr(),
                     // We should only clear site data if First-Party Sets is
                     // enabled when the service is created, to allow users to
                     // play with the FPS enabled setting without affecting
                     // user experience during the browser session.
                     GetEnabledPolicyForProfile(prefs)));
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
  }
  access_delegates_.Add(std::move(access_delegate));
}

void FirstPartySetsPolicyService::OnFirstPartySetsEnabledChanged(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/1366846) Add metrics here to track whether the pref is ever
  // enabled before the config is ready to be to be sent to the delegates.
  for (auto& delegate : access_delegates_) {
    delegate->SetEnabled(enabled);
  }
}

void FirstPartySetsPolicyService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  access_delegates_.Clear();
  browser_context_ = nullptr;
  weak_factory_.InvalidateWeakPtrs();
}

void FirstPartySetsPolicyService::OnProfileConfigReady(
    bool initially_enabled,
    net::FirstPartySetsContextConfig config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!initially_enabled) {
    OnReadyToNotifyDelegates(std::move(config));
    return;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context_);
  if (!profile->IsRegularProfile() || profile->IsGuestSession()) {
    // TODO(https://crbug.com/1348572): regular profiles and guest sessions
    // aren't mutually exclusive on ChromeOS.
    OnReadyToNotifyDelegates(std::move(config));
    return;
  }

  // Representation of the current profile to be persisted on disk.
  const std::string browser_context_id = profile->GetBaseName().AsUTF8Unsafe();

  base::RepeatingCallback<content::BrowserContext*()> browser_context_getter =
      base::BindRepeating(
          [](base::WeakPtr<FirstPartySetsPolicyService> weak_ptr) {
            return weak_ptr ? weak_ptr->browser_context() : nullptr;
          },
          weak_factory_.GetWeakPtr());

  content::FirstPartySetsHandler::GetInstance()
      ->ClearSiteDataOnChangedSetsForContext(
          browser_context_getter, browser_context_id, std::move(config),
          base::BindOnce(&FirstPartySetsPolicyService::OnReadyToNotifyDelegates,
                         weak_factory_.GetWeakPtr()));
}

void FirstPartySetsPolicyService::OnReadyToNotifyDelegates(
    net::FirstPartySetsContextConfig config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  config_ = std::move(config);
  for (auto& delegate : access_delegates_) {
    delegate->NotifyReady(MakeReadyEvent(config_.value().Clone()));
  }
}

void FirstPartySetsPolicyService::ResetForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  access_delegates_.Clear();
  config_.reset();
}

}  // namespace first_party_sets
