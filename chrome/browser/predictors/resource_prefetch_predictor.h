// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_H_
#define CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_util.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/sqlite_proto/key_value_data.h"
#include "net/base/network_anonymization_key.h"
#include "services/network/public/mojom/fetch_api.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

class PredictorsHandler;
class Profile;

namespace predictors {

struct OriginRequestSummary;
struct PageRequestSummary;
struct LcppDataInputs;

namespace internal {
struct LastVisitTimeCompare {
  template <typename T>
  bool operator()(const T& lhs, const T& rhs) const {
    return lhs.last_visit_time() < rhs.last_visit_time();
  }
};

}  // namespace internal

class TestObserver;
class ResourcePrefetcherManager;

// Stores all values needed to trigger a preconnect/preresolve job to a single
// origin.
struct PreconnectRequest {
  // |network_anonymization_key| specifies the key that network requests for the
  // preconnected URL are expected to use. If a request is issued with a
  // different key, it may not use the preconnected socket. It has no effect
  // when |num_sockets| == 0.
  PreconnectRequest(
      const url::Origin& origin,
      int num_sockets,
      const net::NetworkAnonymizationKey& network_anonymization_key);
  PreconnectRequest(const PreconnectRequest&) = default;
  PreconnectRequest(PreconnectRequest&&) = default;
  PreconnectRequest& operator=(const PreconnectRequest&) = default;
  PreconnectRequest& operator=(PreconnectRequest&&) = default;

  url::Origin origin;
  // A zero-value means that we need to preresolve a host only.
  int num_sockets = 0;
  bool allow_credentials = true;
  net::NetworkAnonymizationKey network_anonymization_key;
};

struct PrefetchRequest {
  PrefetchRequest(const GURL& url,
                  const net::NetworkAnonymizationKey& network_anonymization_key,
                  network::mojom::RequestDestination destination);

  PrefetchRequest(const PrefetchRequest&) = default;
  PrefetchRequest(PrefetchRequest&&) = default;
  PrefetchRequest& operator=(const PrefetchRequest&) = default;
  PrefetchRequest& operator=(PrefetchRequest&&) = default;

  GURL url;
  net::NetworkAnonymizationKey network_anonymization_key;
  network::mojom::RequestDestination destination;
};

// Stores a result of pre* prediction. The |requests| vector is the main
// result for preconnects, while the |prefetch_requests| vector is the main
// result for prefetches. Other fields are used for metrics reporting.
struct PreconnectPrediction {
  PreconnectPrediction();
  PreconnectPrediction(const PreconnectPrediction& other);
  PreconnectPrediction(PreconnectPrediction&& other);

  PreconnectPrediction& operator=(const PreconnectPrediction& other);
  PreconnectPrediction& operator=(PreconnectPrediction&& other);
  ~PreconnectPrediction();

  bool is_redirected = false;
  std::string host;
  std::vector<PreconnectRequest> requests;
  std::vector<PrefetchRequest> prefetch_requests;
};

// Stores a result of a prediction from the optimization guide.
struct OptimizationGuidePrediction {
  OptimizationGuidePrediction();
  OptimizationGuidePrediction(const OptimizationGuidePrediction& other);
  ~OptimizationGuidePrediction();

  optimization_guide::OptimizationGuideDecision decision;
  PreconnectPrediction preconnect_prediction;
  std::vector<GURL> predicted_subresources;
  std::optional<base::TimeTicks> optimization_guide_prediction_arrived;
};

// Contains logic for learning what can be prefetched and for kicking off
// speculative prefetching.
// - The class is a profile keyed service owned by the profile.
// - All the non-static methods of this class need to be called on the UI
//   thread.
//
// The overall flow of the resource prefetching algorithm is as follows:
//
// * LoadingPredictorObserver - Listens for URL requests, responses and
//   redirects (client-side redirects are not supported) on the IO thread (via
//   ResourceDispatcherHostDelegate) and posts tasks to the
//   LoadingDataCollector on the UI thread. This is owned by the ProfileIOData
//   for the profile.
// * ResourcePrefetchPredictorTables - Persists ResourcePrefetchPredictor data
//   to a sql database. Runs entirely on the DB sequence provided by the client
//   to the constructor of this class. Owned by the PredictorDatabase.
// * ResourcePrefetchPredictor - Learns about resource requirements per URL in
//   the UI thread through the LoadingPredictorObserver and persists it to disk
//   in the DB sequence through the ResourcePrefetchPredictorTables. It
//   initiates resource prefetching using the ResourcePrefetcherManager. Owned
//   by profile.
class ResourcePrefetchPredictor : public history::HistoryServiceObserver {
 public:
  // Used for reporting redirect prediction success/failure in histograms.
  // NOTE: This enumeration is used in histograms, so please do not add entries
  // in the middle.
  enum class RedirectStatus {
    NO_REDIRECT,
    NO_REDIRECT_BUT_PREDICTED,
    REDIRECT_NOT_PREDICTED,
    REDIRECT_WRONG_PREDICTED,
    REDIRECT_CORRECTLY_PREDICTED,
    MAX
  };

  using RedirectDataMap =
      sqlite_proto::KeyValueData<RedirectData, internal::LastVisitTimeCompare>;
  using OriginDataMap =
      sqlite_proto::KeyValueData<OriginData, internal::LastVisitTimeCompare>;

  ResourcePrefetchPredictor(const LoadingPredictorConfig& config,
                            Profile* profile);

  ResourcePrefetchPredictor(const ResourcePrefetchPredictor&) = delete;
  ResourcePrefetchPredictor& operator=(const ResourcePrefetchPredictor&) =
      delete;

  ~ResourcePrefetchPredictor() override;

  // Starts initialization by posting a task to the DB sequence of the
  // ResourcePrefetchPredictorTables to read the predictor database. Virtual for
  // testing.
  virtual void StartInitialization();
  virtual void Shutdown();

  // Returns true if preconnect data exists for the |main_frame_url|.
  virtual bool IsUrlPreconnectable(const GURL& main_frame_url) const;

  // Sets the |observer| to be notified when the resource prefetch predictor
  // data changes. Previously registered observer will be discarded. Call
  // this with nullptr parameter to de-register observer.
  void SetObserverForTesting(TestObserver* observer);

  // Returns true iff there is OriginData that can be used for a |url| and fills
  // |prediction| with origins and hosts that need to be preconnected and
  // preresolved respectively. |prediction| pointer may be nullptr to get return
  // value only.
  virtual bool PredictPreconnectOrigins(const GURL& url,
                                        PreconnectPrediction* prediction) const;

  // Called by the collector after a page has finished loading resources and
  // assembled a PageRequestSummary.
  virtual void RecordPageRequestSummary(const PageRequestSummary& summary);

  // Record LCP element locators after a page has finished loading and LCP has
  // been determined.
  void LearnLcpp(const std::optional<url::Origin>& initiator_origin,
                 const GURL& url,
                 const LcppDataInputs& inputs);

  // Deletes all URLs from the predictor database and caches.
  void DeleteAllUrls();

  // Returns LcppStat for the `url`, or std::nullopt on failure.
  std::optional<LcppStat> GetLcppStat(
      const std::optional<url::Origin>& initiator_origin,
      const GURL& url) const;

 private:
  friend class LoadingPredictor;
  friend class ::PredictorsHandler;
  friend class LoadingDataCollector;
  friend class ResourcePrefetchPredictorTest;
  friend class PredictorInitializer;

  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, DeleteUrls);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest,
                           DeleteAllUrlsUninitialized);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest,
                           LazilyInitializeEmpty);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest,
                           LazilyInitializeWithData);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest,
                           NavigationLowHistoryCount);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, NavigationUrlInDB);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, NavigationUrlNotInDB);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest,
                           NavigationUrlNotInDBAndDBFull);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest,
                           NavigationManyResourcesWithDifferentOrigins);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, RedirectUrlNotInDB);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, RedirectUrlInDB);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, OnMainFrameRequest);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, OnMainFrameRedirect);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest,
                           OnSubresourceResponse);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, GetCorrectPLT);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest,
                           PopulatePrefetcherRequest);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, GetRedirectOrigin);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, GetPrefetchData);
  FRIEND_TEST_ALL_PREFIXES(
      ResourcePrefetchPredictorPreconnectToRedirectTargetTest,
      TestPredictPreconnectOrigins);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest,
                           TestPredictPreconnectOrigins_RedirectsToNewOrigin);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest,
                           TestPrecisionRecallHistograms);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest,
                           TestPrefetchingDurationHistogram);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest,
                           TestRecordFirstContentfulPaint);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, LearnLcpp);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, LearnFontUrls);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, LearnSubresourceUrls);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest,
                           WhenLcppDataIsCorrupted_ResetData);

  enum InitializationState {
    NOT_INITIALIZED = 0,
    INITIALIZING = 1,
    INITIALIZED = 2
  };

  // Returns true iff one of the following conditions is true
  // * |redirect_data| contains confident redirect origin for |entry_origin|
  //   and assigns it to the |redirect_origin|
  //
  // * |redirect_data| doesn't contain an entry for |entry_origin| and assigns
  //   |entry_origin| to the |redirect_origin|.
  static bool GetRedirectOrigin(const url::Origin& entry_origin,
                                const RedirectDataMap& redirect_data,
                                url::Origin* redirect_origin);

  // Returns true if a redirect endpoint is available. Appends the redirect
  // domains to |prediction->requests|. Sets |prediction->host| if it's empty.
  bool GetRedirectEndpointsForPreconnect(
      const url::Origin& entry_origin,
      const RedirectDataMap& redirect_data,
      PreconnectPrediction* prediction) const;

  // Callback for the task to read the predictor database. Takes ownership of
  // all arguments.
  void CreateCaches(std::unique_ptr<RedirectDataMap> host_redirect_data,
                    std::unique_ptr<OriginDataMap> origin_data,
                    std::unique_ptr<LcppDataMap> lcpp_data);

  // Called during initialization when history is read and the predictor
  // database has been read.
  void OnHistoryAndCacheLoaded();

  // Deletes data for the input |urls| and their corresponding hosts from the
  // predictor database and caches.
  void DeleteUrls(const history::URLRows& urls);

  // Try to ensure that DataMaps are available, and returns true iff they are
  // available now.
  bool TryEnsureRecordingPrecondition();

  // Updates information about final redirect destination for the |key| in
  // |host_redirect_data_| and correspondingly updates the predictor database.
  void LearnRedirect(const std::string& key, const GURL& final_redirect);

  void LearnOrigins(
      const std::string& host,
      const GURL& main_frame_origin,
      const std::map<url::Origin, OriginRequestSummary>& summaries);

  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;
  void OnHistoryServiceLoaded(
      history::HistoryService* history_service) override;

  // Used to connect to HistoryService or register for service loaded
  // notificatioan.
  void ConnectToHistoryService();

  // Used for testing to inject mock tables.
  void set_mock_tables_for_testing(
      scoped_refptr<ResourcePrefetchPredictorTables> tables) {
    tables_ = tables;
    use_lcpp_mock_table_for_testing_ = true;
  }

  const raw_ptr<Profile, DanglingUntriaged> profile_;
  raw_ptr<TestObserver> observer_;
  const LoadingPredictorConfig config_;
  InitializationState initialization_state_;
  scoped_refptr<ResourcePrefetchPredictorTables> tables_;
  base::CancelableTaskTracker history_lookup_consumer_;

  std::unique_ptr<RedirectDataMap> host_redirect_data_;
  std::unique_ptr<OriginDataMap> origin_data_;
  std::unique_ptr<LcppDataMap> lcpp_data_;
  bool use_lcpp_mock_table_for_testing_ = false;

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};

  // Indicates if all predictors data should be deleted after the
  // initialization is completed.
  bool delete_all_data_requested_ = false;

  base::WeakPtrFactory<ResourcePrefetchPredictor> weak_factory_{this};
};

// An interface used to notify that data in the ResourcePrefetchPredictor
// has changed. All methods are invoked on the UI thread.
class TestObserver {
 public:
  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  // De-registers itself from |predictor_| on destruction.
  virtual ~TestObserver();

  virtual void OnPredictorInitialized() {}

  virtual void OnNavigationLearned(const PageRequestSummary& summary) {}

  virtual void OnLcppLearned() {}

 protected:
  // |predictor| must be non-NULL and has to outlive the TestObserver.
  // Also the predictor must not have a TestObserver set.
  explicit TestObserver(ResourcePrefetchPredictor* predictor);

 private:
  raw_ptr<ResourcePrefetchPredictor> predictor_;
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_H_
