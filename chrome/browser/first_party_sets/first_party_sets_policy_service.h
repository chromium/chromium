// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_POLICY_SERVICE_H_
#define CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_POLICY_SERVICE_H_

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/network/public/mojom/first_party_sets_access_delegate.mojom.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace net {
class FirstPartySetsCacheFilter;
class FirstPartySetsContextConfig;
class FirstPartySetEntry;
class SchemefulSite;
}  // namespace net

class Profile;

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
  enum class ServiceState {
    // Related Website Sets is permanently disabled for this profile.
    kPermanentlyDisabled,
    // Related Website Sets is disabled (for now) for this profile. This may
    // change as preferences change.
    kDisabled,
    // Related Website Sets is permanently enabled for this profile.
    kPermanentlyEnabled,
    // Related Website Sets is enabled (for now) for this profile. This may
    // change as preferences change.
    kEnabled,
  };

  explicit FirstPartySetsPolicyService(content::BrowserContext* context);
  FirstPartySetsPolicyService(const FirstPartySetsPolicyService&) = delete;
  FirstPartySetsPolicyService& operator=(const FirstPartySetsPolicyService&) =
      delete;
  ~FirstPartySetsPolicyService() override;

  // Computes the First-Party Set metadata related to the given request context,
  // and invokes `callback` with the result.
  //
  // This may invoke `callback` synchronously.
  void ComputeFirstPartySetMetadata(
      const net::SchemefulSite& site,
      const net::SchemefulSite* top_frame_site,
      base::OnceCallback<void(net::FirstPartySetMetadata)> callback);

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

  // PrivacySandboxSettings::Observer
  void OnFirstPartySetsEnabledChanged(bool enabled) override;

  // Stores the callback to be invoked when this service is ready to do so. Must
  // not be called when FPS is not enabled or the service is already ready.
  void RegisterThrottleResumeCallback(base::OnceClosure resume_callback);

  // KeyedService:
  void Shutdown() override;

  // Invokes `callback` when the first call to `Init` has fully completed, i.e.
  // when this instance first receives its config. If this instance has already
  // received its config, this immediately invokes `callback`.
  //
  // This is intended as a workaround for the inability to use a test-only
  // factory for FirstPartySetsPolicyService instances in tests, so every
  // instance calls into the prod logic to eagerly initialize itself. This
  // method allows tests to wait for that eager initialization to complete, then
  // reset state, and re-run initialization via `Init`.
  void WaitForFirstInitCompleteForTesting(base::OnceClosure callback);

  // Exposes `Init` for use in tests.
  void InitForTesting();

  // Returns true iff the Related Website Sets service is enabled.
  bool is_enabled() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    switch (service_state_) {
      case ServiceState::kPermanentlyDisabled:
      case ServiceState::kDisabled:
        return false;
      case ServiceState::kPermanentlyEnabled:
      case ServiceState::kEnabled:
        return true;
    }
  }

  // Returns true when this instance has received the config thus has been fully
  // initialized.
  bool is_ready() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return config_.has_value();
  }

  void ResetForTesting();

  // Looks up `site` in Chrome's list of First-Party Sets and returns its
  // associated entry if `site` is found.
  //
  // This will return nullopt if:
  // - First-Party Sets is disabled or
  // - the list of First-Party Sets isn't initialized yet or
  // - `site` isn't in Chrome's list of First-Party Sets or
  // - this instance has not received the config yet
  //
  // This also logs metrics that track how often this is queried before this
  // instance has received the config yet.
  std::optional<net::FirstPartySetEntry> FindEntry(
      const net::SchemefulSite& site);

  // Synchronously iterate over the effective First-Party Sets entries in use by
  // this profile (i.e. all the entries that could be returned by `FindEntry`,
  // including the manual set, policy sets, and public sets).
  //
  // Returns early if any of the iterations returns false.
  // Returns false if service is not ready, or First-Party Sets was not yet
  // initialized, or iteration was incomplete;
  // Returns true if all iterations returned true. No guarantees are made re:
  // iteration order.
  //
  // This also logs metrics that track how often this is queried before ready.
  bool ForEachEffectiveSetEntry(
      base::FunctionRef<bool(const net::SchemefulSite&,
                             const net::FirstPartySetEntry&)> f) const;

  // Checks if ownership of `site` is managed by an enterprise.
  //
  // Note: this doesn't consider `site` as managed if it was removed by an
  // enterprise using policy.
  bool IsSiteInManagedSet(const net::SchemefulSite& site) const;

  content::BrowserContext* browser_context() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return &*browser_context_;
  }

  base::WeakPtr<first_party_sets::FirstPartySetsPolicyService> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  // Initialize this instance by getting the config if needed.
  void Init();

  // Sets the `config_` member and provides it to all delegates via NotifyReady.
  void OnReadyToNotifyDelegates(net::FirstPartySetsContextConfig config,
                                net::FirstPartySetsCacheFilter cache_filter);

  // Triggers changes that occur once the FirstPartySetsContextConfig for the
  // profile that created this service is retrieved.
  //
  // Only clears site data if First-Party Sets is enabled when this service
  // is created.
  void OnProfileConfigReady(ServiceState initial_state,
                            net::FirstPartySetsContextConfig config);

  // Like ComputeFirstPartySetMetadata, but passes the result into the provided
  // callback. Must not be called before `config_` has been received.
  void ComputeFirstPartySetMetadataInternal(
      const net::SchemefulSite& site,
      const std::optional<net::SchemefulSite>& top_frame_site,
      base::OnceCallback<void(net::FirstPartySetMetadata)> callback) const;

  // Clears the content settings associated with `profile` that were
  // affected/mediated by First-Party Sets.
  void ClearContentSettings(Profile* profile) const;

  // The remote delegates associated with the profile that created this
  // service.
  mojo::RemoteSet<network::mojom::FirstPartySetsAccessDelegate>
      access_delegates_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The BrowserContext with which this service is associated.
  const raw_ref<content::BrowserContext> browser_context_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Whether FPS is enabled in this context. Note that this may be true even if
  // FPS is globally disabled (e.g. disabled by the embedder).
  //
  // Initialized to `kEnabled` for the sake of tests, so that queries received
  // before service initialization can be accumulated and answered after test
  // setup, rather than answered immediately in the negative.
  ServiceState service_state_ GUARDED_BY_CONTEXT(sequence_checker_) =
      ServiceState::kEnabled;

  // The customizations to the browser's list of First-Party Sets to respect
  // the changes specified by this FirstPartySetsOverrides policy for the
  // profile that created this service.
  std::optional<net::FirstPartySetsContextConfig> config_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The filter used to bypass cache access in the network for this profile.
  std::optional<net::FirstPartySetsCacheFilter> cache_filter_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The queue of callbacks that are waiting for the instance to be initialized.
  base::circular_deque<base::OnceClosure> on_ready_callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Callback used by tests to wait for the ctor's initialization flow to
  // complete.
  std::optional<base::OnceClosure> on_first_init_complete_for_testing_;

  // Keeps track of whether this instance has ever been initialized fully. Must
  // not be reset in `ResetForTesting`.
  bool first_initialization_complete_for_testing_ = false;

  const raw_ref<privacy_sandbox::PrivacySandboxSettings>
      privacy_sandbox_settings_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::ScopedObservation<privacy_sandbox::PrivacySandboxSettings,
                          privacy_sandbox::PrivacySandboxSettings::Observer>
      privacy_sandbox_settings_observer_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<FirstPartySetsPolicyService> weak_factory_{this};
};

}  // namespace first_party_sets

#endif  // CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_POLICY_SERVICE_H_
