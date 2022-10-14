// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_POLICY_SERVICE_H_
#define CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_POLICY_SERVICE_H_

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/network/public/mojom/first_party_sets_access_delegate.mojom.h"

class PrefService;

namespace content {
class BrowserContext;
}  // namespace content

namespace net {
class FirstPartySetsCacheFilter;
class FirstPartySetsContextConfig;
class SchemefulSite;
}  // namespace net

namespace first_party_sets {

// A profile keyed service for per-BrowserContext First-Party Sets state.
//
// This service always exists for a BrowserContext, regardless of whether the
// First-Party Sets feature is enabled globally or for this particular
// BrowserContext.
class FirstPartySetsPolicyService : public KeyedService {
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

  // Triggers changes to `access_delegates` that should occur when the
  // First-Party Sets enabled pref changes.
  void OnFirstPartySetsEnabledChanged(bool enabled);

  // Invoke the callback synchronously to resume navigation if the instance is
  // ready; or stores the callback to be invoked when this service is ready to
  // do so.
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
  // reset state, and re-run initialization via `InitForTesting`.
  void WaitForFirstInitCompleteForTesting(base::OnceClosure callback);

  // Testing-only method that allows injecting different logic to get the
  // config.
  void InitForTesting(
      base::FunctionRef<
          void(PrefService*,
               base::OnceCallback<void(net::FirstPartySetsContextConfig)>)>
          get_config);

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
  absl::optional<net::FirstPartySetEntry> FindEntry(
      const net::SchemefulSite& site);

  // Checks if ownership of `site` is managed by an enterprise.
  //
  // Note: this doesn't consider `site` as managed if it was removed by an
  // enterprise using policy.
  bool IsSiteInManagedSet(const net::SchemefulSite& site) const;

  content::BrowserContext* browser_context() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return browser_context_;
  }

 private:
  // Initialize this instance by getting the config via `get_config` if needed.
  void Init(base::FunctionRef<
            void(PrefService*,
                 base::OnceCallback<void(net::FirstPartySetsContextConfig)>)>
                get_config);

  // Sets the `config_` member and provides it to all delegates via NotifyReady.
  void OnReadyToNotifyDelegates(net::FirstPartySetsContextConfig config,
                                net::FirstPartySetsCacheFilter cache_filter);

  // Triggers changes that occur once the FirstPartySetsContextConfig for the
  // profile that created this service is retrieved.
  //
  // Only clears site data if First-Party Sets is enabled when this service
  // is created.
  void OnProfileConfigReady(bool initially_enabled,
                            net::FirstPartySetsContextConfig config);

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

  // The filter used to bypass cache access in the network for this profile.
  absl::optional<net::FirstPartySetsCacheFilter> cache_filter_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The queue of callbacks that are waiting for the instance to be initialized.
  base::circular_deque<base::OnceClosure> on_ready_callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Callback used by tests to wait for the ctor's initialization flow to
  // complete.
  absl::optional<base::OnceClosure> on_first_init_complete_for_testing_;

  // Keeps track of whether this instance has ever been initialized fully. Must
  // not be reset in `ResetForTesting`.
  bool first_initialization_complete_for_testing_ = false;

  // Tracks the number of queries to the First-Party Sets in the browser process
  // are received before the `global_sets_` are initialized.
  int num_queries_before_sets_ready_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<FirstPartySetsPolicyService> weak_factory_{this};
};

}  // namespace first_party_sets

#endif  // CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_POLICY_SERVICE_H_
