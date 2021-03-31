// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEDERATED_LEARNING_FLOC_ID_PROVIDER_IMPL_H_
#define CHROME_BROWSER_FEDERATED_LEARNING_FLOC_ID_PROVIDER_IMPL_H_

#include "base/gtest_prod_util.h"
#include "base/scoped_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/timer/timer.h"
#include "chrome/browser/federated_learning/floc_id_provider.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/federated_learning/floc_sorting_lsh_clusters_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"

namespace federated_learning {

class FlocEventLogger;

// A service that regularly computes the floc id and logs it in a user event.
//
// For the first browser session of a profile, we'll start computing the floc
// after the sorting-lsh file is loaded, and another computation will be
// scheduled every X days. When the browser shuts down and starts up again, it
// can remember the last state and can still schedule the computation at X days
// after the last compute time. If we've missed a scheduled update due to the
// browser not being alive, it'll compute after the next session starts, using
// the sorting-lsh-file-loaded as the first compute triggering condition.
//
// The floc will be computed by:
// Step 1: sim-hashing navigation URL domains in the last 7 days. This step aims
// to group together users with similar browsing habit.
// Step 2: applying the sorting-lsh post processing to the sim-hash value. The
// sorting-lsh technique groups similar sim-hash values together to ensure the
// smallest group size / K-anonymity. The mappings / group-size is computed
// server side in chrome-sync, based on logged sim-hash data, and is pushed to
// Chrome on a regular basis through the component updater.
//
// A computed floc will be valid if:
// - 3rd party cookies are NOT blocked.
// - There are at least 3 *eligible* history entries in the last 7 days, where
// eligible means the IP was publicly routable.
// - It's not blocked by the sorting-lsh (with encoded blocklist) file.
//
// If some of those conditions are not met, an invalid floc will be given.
//
// In the event of history deletion, the floc will be invalidated immediately if
// the time range of the deletion overlaps with the time range used to compute
// the existing floc. In the event of cookie deletion, the floc will always be
// invalidated. Note that we only invalidate the floc rather than recomputing,
// because we don't want the floc to change more frequently than the scheduled
// update rate (% rare cases such as when the finch version param has changed
// indicating a new algorithm / experiment, a recompute will be needed).
class FlocIdProviderImpl : public FlocIdProvider,
                           public FlocSortingLshClustersService::Observer,
                           public PrivacySandboxSettings::Observer,
                           public history::HistoryServiceObserver {
 public:
  struct ComputeFlocResult {
    ComputeFlocResult() = default;

    ComputeFlocResult(uint64_t sim_hash, const FlocId& floc_id)
        : sim_hash_computed(true), sim_hash(sim_hash), floc_id(floc_id) {}

    bool sim_hash_computed = false;

    // Sim-hash of the browsing history. This is the baseline value where the
    // |floc_id| field should be derived from. We'll log this field for the
    // server to calculate the sorting-lsh cutting points.
    uint64_t sim_hash = 0;

    // The floc to be exposed to JS API. It's derived from applying the
    // sorting-lsh & blocklist post-processing on the |sim_hash|.
    FlocId floc_id;
  };

  using CanComputeFlocCallback = base::OnceCallback<void(bool)>;
  using ComputeFlocCompletedCallback =
      base::OnceCallback<void(ComputeFlocResult)>;
  using GetRecentlyVisitedURLsCallback =
      history::HistoryService::QueryHistoryCallback;

  FlocIdProviderImpl(PrefService* prefs,
                     PrivacySandboxSettings* privacy_sandbox_settings,
                     history::HistoryService* history_service,
                     std::unique_ptr<FlocEventLogger> floc_event_logger);
  ~FlocIdProviderImpl() override;
  FlocIdProviderImpl(const FlocIdProviderImpl&) = delete;
  FlocIdProviderImpl& operator=(const FlocIdProviderImpl&) = delete;

  blink::mojom::InterestCohortPtr GetInterestCohortForJsApi(
      const GURL& url,
      const base::Optional<url::Origin>& top_frame_origin) const override;

  void MaybeRecordFlocToUkm(ukm::SourceId source_id) override;

 protected:
  // protected virtual for testing.
  virtual void OnComputeFlocCompleted(ComputeFlocResult result);
  virtual void LogFlocComputedEvent(const ComputeFlocResult& result);

 private:
  friend class FlocIdProviderUnitTest;
  friend class FlocIdProviderBrowserTest;

  // KeyedService:
  void Shutdown() override;

  // PrivacySandboxSettings::Observer

  // When the floc-accessible-since time is updated (due to e.g. cookies
  // deletion), we'll either invalidate or keep using the floc. This will
  // depend on the updated time and the begin time of the history used to
  // compute the current floc.
  void OnFlocDataAccessibleSinceUpdated() override;

  // On history deletion, we'll either invalidate or keep using the floc. This
  // will depend on the deletion type and the time range.
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;

  // FlocSortingLshClustersService::Observer
  void OnSortingLshClustersFileReady() override;

  void ComputeFloc();

  void CheckCanComputeFloc(CanComputeFlocCallback callback);
  void OnCheckCanComputeFlocCompleted(ComputeFlocCompletedCallback callback,
                                      bool can_compute_floc);

  bool IsSyncHistoryEnabled() const;
  bool IsPrivacySandboxAllowed() const;

  void IsSwaaNacAccountEnabled(CanComputeFlocCallback callback);

  void GetRecentlyVisitedURLs(GetRecentlyVisitedURLsCallback callback);
  void OnGetRecentlyVisitedURLsCompleted(ComputeFlocCompletedCallback callback,
                                         history::QueryResults results);

  void DidApplySortingLshPostProcessing(ComputeFlocCompletedCallback callback,
                                        uint64_t sim_hash,
                                        base::Time history_begin_time,
                                        base::Time history_end_time,
                                        base::Optional<uint64_t> final_hash,
                                        base::Version version);

  // Abandon any scheduled task, and schedule a new compute-floc task with
  // |delay|.
  void ScheduleFlocComputation(base::TimeDelta delay);

  // The following raw pointer references are guaranteed to outlive this object.
  // |prefs_| is owned by Profile, and it won't be destroyed until the
  // destructor of Profile is called, where all the profile-keyed services
  // including this object will be destroyed. Other services are all created by
  // profile-keyed service factories, and the dependency declared in
  // FlocIdProviderFactory::FlocIdProviderFactory() guarantees that this object
  // will be destroyed first among those services.
  PrefService* prefs_;
  PrivacySandboxSettings* privacy_sandbox_settings_;
  history::HistoryService* history_service_;

  std::unique_ptr<FlocEventLogger> floc_event_logger_;

  // The id to be exposed to the JS API. It will always be in sync with the one
  // stored in prefs.
  FlocId floc_id_;

  // When a floc is computed, we'll record it to the UKM on the next page load.
  // This flag controls whether the recording is needed. Caveat: given that this
  // info does not persist across browser sessions, we could miss the recording
  // when the floc is computed and then the browser is closed before the next
  // page load occurs.
  bool need_ukm_recording_ = false;

  bool floc_computation_in_progress_ = false;

  // True if history-delete occurs during an in-progress computation. When the
  // in-progress one finishes, we would disregard the result (i.e. no loggings
  // or floc update), and compute again. Potentially we could maintain extra
  // states to tell if the history-delete would have impact on the in-progress
  // result, but since this would only happen in rare race situations, we just
  // always recompute to keep things simple.
  bool need_recompute_ = false;

  // Used for the async tasks querying the HistoryService.
  base::CancelableTaskTracker history_task_tracker_;

  // The timer used to schedule a floc computation.
  base::OneShotTimer compute_floc_timer_;

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};

  base::WeakPtrFactory<FlocIdProviderImpl> weak_ptr_factory_{this};
};

}  // namespace federated_learning

#endif  // CHROME_BROWSER_FEDERATED_LEARNING_FLOC_ID_PROVIDER_IMPL_H_
