// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_POLICY_SERVICE_H_
#define CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_POLICY_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "services/network/public/mojom/first_party_sets_access_delegate.mojom.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace first_party_sets {

// A profile keyed service for per-BrowserContext First-Party Sets state.
//
// This service always exists for a BrowserContext, regardless of whether the
// First-Party Sets feature is enabled globally or for this particular
// BrowserContext.
class FirstPartySetsPolicyService
    : public KeyedService,
      public privacy_sandbox::PrivacySandboxSettings::Observer {
 public:
  explicit FirstPartySetsPolicyService(content::BrowserContext* context);
  FirstPartySetsPolicyService(const FirstPartySetsPolicyService&) = delete;
  FirstPartySetsPolicyService& operator=(const FirstPartySetsPolicyService&) =
      delete;
  ~FirstPartySetsPolicyService() override;

  // Stores `access_delegate` in a RemoteSet for later IPC calls on it when this
  // service is ready to do so.
  //
  // NotifyReady will be called on `access_delegate` in the following cases:
  // - when site-data is cleared
  // - upon OnFirstPartySetsEnabledChanged observations (if site-data has
  //   already been, or didn't need to be, cleared) and if `config` is ready
  // - by this method if `config_` has already been computed
  //
  // SetEnabled will be called on `access_delegate` when the First-Party Sets
  // enabled pref changes, as observed by OnFirstPartySetsEnabledChanged.
  void AddRemoteAccessDelegate(
      mojo::Remote<network::mojom::FirstPartySetsAccessDelegate>
          access_delegate);

  // PrivacySandboxSettings::Observer:
  void OnFirstPartySetsEnabledChanged(bool enabled) override;

  // KeyedService:
  void Shutdown() override;

  // Triggers changes that occur once the FirstPartySetsContextConfig for the
  // profile that created this service is retrieved.
  //
  // Only clears site data if First-Party Sets is enabled when this service
  // is created.
  //
  // This method is exposed publicly for testing.
  void OnProfileConfigReady(bool initially_enabled,
                            net::FirstPartySetsContextConfig config);

  void ResetForTesting();

  content::BrowserContext* browser_context() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return browser_context_;
  }

  void SetConfigForTesting(net::FirstPartySetsContextConfig config) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    config_ = std::move(config);
  }

 private:
  // Sets the `config_` member and provides it to all delegates via NotifyReady.
  void OnReadyToNotifyDelegates(net::FirstPartySetsContextConfig config);

  // The remote delegates associated with the profile that created this
  // service.
  mojo::RemoteSet<network::mojom::FirstPartySetsAccessDelegate>
      access_delegates_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The BrowserContext with which this service is associated. Set to nullptr in
  // `Shutdown()`.
  raw_ptr<content::BrowserContext> browser_context_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The customizations to the browser's list of First-Party Sets to respect
  // the changes specified by this FirstPartySetsOverrides policy for the
  // profile that created this service.
  absl::optional<net::FirstPartySetsContextConfig> config_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::ScopedObservation<privacy_sandbox::PrivacySandboxSettings,
                          privacy_sandbox::PrivacySandboxSettings::Observer>
      privacy_sandbox_settings_observer_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<FirstPartySetsPolicyService> weak_factory_{this};
};

}  // namespace first_party_sets

#endif  // CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_POLICY_SERVICE_H_
