// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetch_predictor.h"

#include <map>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/predictors/loading_data_collector.h"
#include "chrome/browser/predictors/predictor_database.h"
#include "chrome/browser/predictors/predictor_database_factory.h"
#include "chrome/browser/predictors/predictors_features.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/url_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

using content::BrowserThread;

namespace predictors {

namespace {

const float kMinOriginConfidenceToTriggerPreconnect = 0.75f;
const float kMinOriginConfidenceToTriggerPreresolve = 0.2f;

float ComputeRedirectConfidence(const predictors::RedirectStat& redirect) {
  return (redirect.number_of_hits() + 0.0) /
         (redirect.number_of_hits() + redirect.number_of_misses());
}

void InitializeOriginStatFromOriginRequestSummary(
    OriginStat* origin,
    const OriginRequestSummary& summary) {
  origin->set_origin(summary.origin.GetURL().spec());
  origin->set_number_of_hits(1);
  origin->set_average_position(summary.first_occurrence + 1);
  origin->set_always_access_network(summary.always_access_network);
  origin->set_accessed_network(summary.accessed_network);
}

void InitializeOnDBSequence(
    ResourcePrefetchPredictor::RedirectDataMap* host_redirect_data,
    ResourcePrefetchPredictor::OriginDataMap* origin_data) {
  host_redirect_data->InitializeOnDBSequence();
  origin_data->InitializeOnDBSequence();
}

GURL CreateRedirectURL(const std::string& scheme,
                       const std::string& host,
                       std::uint16_t port) {
  return GURL(scheme + "://" + host + ":" + base::NumberToString(port));
}

}  // namespace

PreconnectRequest::PreconnectRequest(
    const url::Origin& origin,
    int num_sockets,
    const net::NetworkIsolationKey& network_isolation_key)
    : origin(origin),
      num_sockets(num_sockets),
      network_isolation_key(network_isolation_key) {
  DCHECK_GE(num_sockets, 0);
}

PreconnectPrediction::PreconnectPrediction() = default;
PreconnectPrediction::PreconnectPrediction(
    const PreconnectPrediction& prediction) = default;
PreconnectPrediction::~PreconnectPrediction() = default;

////////////////////////////////////////////////////////////////////////////////
// ResourcePrefetchPredictor static functions.

bool ResourcePrefetchPredictor::GetRedirectOrigin(
    const url::Origin& entry_origin,
    const RedirectDataMap& redirect_data,
    url::Origin* redirect_origin) {
  DCHECK(redirect_origin);

  RedirectData data;
  bool exists = redirect_data.TryGetData(entry_origin.host(), &data);
  if (!exists) {
    // Fallback to fetching URLs based on the incoming URL/host. By default
    // the predictor is confident that there is no redirect.
    *redirect_origin = entry_origin;
    return true;
  }

  DCHECK_GT(data.redirect_endpoints_size(), 0);
  if (data.redirect_endpoints_size() > 1) {
    // The predictor observed multiple redirect destinations recently. Redirect
    // endpoint is ambiguous. The predictor predicts a redirect only if it
    // believes that the redirect is "permanent", i.e. subsequent navigations
    // will lead to the same destination.
    return false;
  }

  // The threshold is higher than the threshold for resources because the
  // redirect misprediction causes the waste of whole prefetch.
  const float kMinRedirectConfidenceToTriggerPrefetch = 0.9f;
  const int kMinRedirectHitsToTriggerPrefetch = 2;

  // The predictor doesn't apply a minimum-number-of-hits threshold to
  // the no-redirect case because the no-redirect is a default assumption.
  const RedirectStat& redirect = data.redirect_endpoints(0);
  bool redirect_origin_matches_entry_origin =
      redirect.url() == entry_origin.host() &&
      redirect.url_port() == entry_origin.port();

  if (ComputeRedirectConfidence(redirect) <
          kMinRedirectConfidenceToTriggerPrefetch ||
      (redirect.number_of_hits() < kMinRedirectHitsToTriggerPrefetch &&
       !redirect_origin_matches_entry_origin)) {
    return false;
  }

  // Create a GURL from |redirect|, and get the origin from it. Origins can
  // be created be directly passing in scheme, host, and port, but the class
  // DCHECKs if any of them are invalid, and best not to DCHECK when loading bad
  // data from disk. GURL does not DCHECK on bad input, so safest to rely on its
  // logic, though more computationally expensive.

  GURL redirect_url;
  // Old entries may have no scheme or port.
  if (redirect.has_url_scheme() && redirect.has_url_port()) {
    redirect_url = CreateRedirectURL(redirect.url_scheme(), redirect.url(),
                                     redirect.url_port());
  }

  // If there was no scheme or port, or they don't make for a valid URL (most
  // likely due to using 0 or an empty scheme as default values), default to
  // HTTPS / port 443.
  if (!redirect_url.is_valid())
    redirect_url = CreateRedirectURL("https", redirect.url(), 443);

  if (!redirect_url.is_valid())
    return false;

  *redirect_origin = url::Origin::Create(redirect_url);
  return true;
}

bool ResourcePrefetchPredictor::GetRedirectEndpointsForPreconnect(
    const url::Origin& entry_origin,
    const RedirectDataMap& redirect_data,
    PreconnectPrediction* prediction) const {
  if (!base::FeatureList::IsEnabled(
          features::kLoadingPreconnectToRedirectTarget)) {
    return false;
  }
  DCHECK(!prediction || prediction->requests.empty());

  RedirectData data;
  if (!redirect_data.TryGetData(entry_origin.host(), &data))
    return false;

  // The thresholds here are lower than the thresholds used above in
  // GetRedirectOrigin() method. Here the overhead of a negative prediction is
  // that the browser preconnects to one incorrectly predicted origin. In
  // GetRedirectOrigin(), the overhead of wrong prediction is much higher
  // (multiple incorrect preconnects).
  const float kMinRedirectConfidenceToTriggerPrefetch = 0.1f;

  bool at_least_one_redirect_endpoint_added = false;
  for (const auto& redirect : data.redirect_endpoints()) {
    if (ComputeRedirectConfidence(redirect) <
        kMinRedirectConfidenceToTriggerPrefetch) {
      continue;
    }

    // Assume HTTPS and port 443 by default.
    std::string redirect_scheme =
        redirect.url_scheme().empty() ? "https" : redirect.url_scheme();
    int redirect_port = redirect.has_url_port() ? redirect.url_port() : 443;

    const url::Origin redirect_origin = url::Origin::CreateFromNormalizedTuple(
        redirect_scheme, redirect.url(), redirect_port);

    if (redirect_origin == entry_origin) {
      continue;
    }

    // Add the endpoint to which the predictor has seen redirects to.
    // Set network isolation key same as the origin of the redirect target.
    if (prediction) {
      prediction->requests.emplace_back(
          redirect_origin, 1 /* num_scokets */,
          net::NetworkIsolationKey(redirect_origin, redirect_origin));
    }
    at_least_one_redirect_endpoint_added = true;
  }

  if (prediction && prediction->host.empty() &&
      at_least_one_redirect_endpoint_added) {
    prediction->host = entry_origin.host();
  }

  return at_least_one_redirect_endpoint_added;
}

////////////////////////////////////////////////////////////////////////////////
// ResourcePrefetchPredictor.

ResourcePrefetchPredictor::ResourcePrefetchPredictor(
    const LoadingPredictorConfig& config,
    Profile* profile)
    : profile_(profile),
      observer_(nullptr),
      config_(config),
      initialization_state_(NOT_INITIALIZED),
      tables_(PredictorDatabaseFactory::GetForProfile(profile)
                  ->resource_prefetch_tables()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ResourcePrefetchPredictor::~ResourcePrefetchPredictor() {}

void ResourcePrefetchPredictor::StartInitialization() {
  TRACE_EVENT0("browser", "ResourcePrefetchPredictor::StartInitialization");

  if (initialization_state_ != NOT_INITIALIZED)
    return;
  initialization_state_ = INITIALIZING;

  // Create local caches using the database as loaded.
  auto host_redirect_data = std::make_unique<RedirectDataMap>(
      tables_, tables_->host_redirect_table(), config_.max_hosts_to_track,
      base::TimeDelta::FromSeconds(config_.flush_data_to_disk_delay_seconds));
  auto origin_data = std::make_unique<OriginDataMap>(
      tables_, tables_->origin_table(), config_.max_hosts_to_track,
      base::TimeDelta::FromSeconds(config_.flush_data_to_disk_delay_seconds));

  // Get raw pointers to pass to the first task. Ownership of the unique_ptrs
  // will be passed to the reply task.
  auto task = base::BindOnce(InitializeOnDBSequence, host_redirect_data.get(),
                             origin_data.get());
  auto reply = base::BindOnce(
      &ResourcePrefetchPredictor::CreateCaches, weak_factory_.GetWeakPtr(),
      std::move(host_redirect_data), std::move(origin_data));

  tables_->GetTaskRunner()->PostTaskAndReply(FROM_HERE, std::move(task),
                                             std::move(reply));
}

bool ResourcePrefetchPredictor::IsUrlPreconnectable(
    const GURL& main_frame_url) const {
  return PredictPreconnectOrigins(main_frame_url, nullptr);
}

void ResourcePrefetchPredictor::SetObserverForTesting(TestObserver* observer) {
  observer_ = observer;
}

void ResourcePrefetchPredictor::Shutdown() {
  history_service_observer_.RemoveAll();
}

void ResourcePrefetchPredictor::RecordPageRequestSummary(
    std::unique_ptr<PageRequestSummary> summary) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Make sure initialization is done or start initialization if necessary.
  if (initialization_state_ == NOT_INITIALIZED) {
    StartInitialization();
    return;
  } else if (initialization_state_ == INITIALIZING) {
    return;
  } else if (initialization_state_ != INITIALIZED) {
    NOTREACHED() << "Unexpected initialization_state_: "
                 << initialization_state_;
    return;
  }

  LearnRedirect(summary->initial_url.host(), summary->main_frame_url,
                host_redirect_data_.get());
  LearnOrigins(summary->main_frame_url.host(),
               summary->main_frame_url.GetOrigin(), summary->origins);

  if (observer_)
    observer_->OnNavigationLearned(*summary);
}

bool ResourcePrefetchPredictor::PredictPreconnectOrigins(
    const GURL& url,
    PreconnectPrediction* prediction) const {
  DCHECK(!prediction || prediction->requests.empty());
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (initialization_state_ != INITIALIZED)
    return false;

  url::Origin url_origin = url::Origin::Create(url);
  url::Origin redirect_origin;
  bool has_any_prediction = GetRedirectEndpointsForPreconnect(
      url_origin, *host_redirect_data_, prediction);
  if (!GetRedirectOrigin(url_origin, *host_redirect_data_, &redirect_origin)) {
    // GetRedirectOrigin() may return false if it's not confident about the
    // redirect target or the navigation target. Calling
    // GetRedirectEndpointsForPreconnect() ensures we add all possible redirect
    // targets to the preconnect prediction.
    return has_any_prediction;
  }

  OriginData data;
  if (!origin_data_->TryGetData(redirect_origin.host(), &data)) {
    return has_any_prediction;
  }

  if (prediction) {
    prediction->host = redirect_origin.host();
    prediction->is_redirected = (redirect_origin != url_origin);
  }

  net::NetworkIsolationKey network_isolation_key(redirect_origin,
                                                 redirect_origin);

  for (const OriginStat& origin : data.origins()) {
    float confidence = static_cast<float>(origin.number_of_hits()) /
                       (origin.number_of_hits() + origin.number_of_misses());
    if (confidence < kMinOriginConfidenceToTriggerPreresolve)
      continue;

    has_any_prediction = true;
    if (prediction) {
      if (confidence > kMinOriginConfidenceToTriggerPreconnect) {
        prediction->requests.emplace_back(
            url::Origin::Create(GURL(origin.origin())), 1,
            network_isolation_key);
      } else {
        prediction->requests.emplace_back(
            url::Origin::Create(GURL(origin.origin())), 0,
            network_isolation_key);
      }
    }
  }

  return has_any_prediction;
}

void ResourcePrefetchPredictor::CreateCaches(
    std::unique_ptr<RedirectDataMap> host_redirect_data,
    std::unique_ptr<OriginDataMap> origin_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(INITIALIZING, initialization_state_);

  DCHECK(host_redirect_data);
  DCHECK(origin_data);

  host_redirect_data_ = std::move(host_redirect_data);
  origin_data_ = std::move(origin_data);

  ConnectToHistoryService();
}

void ResourcePrefetchPredictor::OnHistoryAndCacheLoaded() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(INITIALIZING, initialization_state_);

  initialization_state_ = INITIALIZED;
  if (delete_all_data_requested_) {
    DeleteAllUrls();
    delete_all_data_requested_ = false;
  }
  if (observer_)
    observer_->OnPredictorInitialized();
}

void ResourcePrefetchPredictor::DeleteAllUrls() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (initialization_state_ != INITIALIZED) {
    delete_all_data_requested_ = true;
    return;
  }

  host_redirect_data_->DeleteAllData();
  origin_data_->DeleteAllData();
}

void ResourcePrefetchPredictor::DeleteUrls(const history::URLRows& urls) {
  std::vector<std::string> hosts_to_delete;

  for (const auto& it : urls)
    hosts_to_delete.emplace_back(it.url().host());

  host_redirect_data_->DeleteData(hosts_to_delete);
  origin_data_->DeleteData(hosts_to_delete);
}

void ResourcePrefetchPredictor::LearnRedirect(const std::string& key,
                                              const GURL& final_redirect,
                                              RedirectDataMap* redirect_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If the primary key is too long reject it.
  if (key.length() > ResourcePrefetchPredictorTables::kMaxStringLength)
    return;

  RedirectData data;
  bool exists = redirect_data->TryGetData(key, &data);
  if (!exists) {
    data.set_primary_key(key);
    data.set_last_visit_time(base::Time::Now().ToInternalValue());
    RedirectStat* redirect_to_add = data.add_redirect_endpoints();
    redirect_to_add->set_url(final_redirect.host());
    redirect_to_add->set_number_of_hits(1);
    redirect_to_add->set_url_scheme(final_redirect.scheme());
    redirect_to_add->set_url_port(final_redirect.EffectiveIntPort());
  } else {
    data.set_last_visit_time(base::Time::Now().ToInternalValue());

    bool need_to_add = true;
    for (RedirectStat& redirect : *(data.mutable_redirect_endpoints())) {
      const bool host_mismatch = redirect.url() != final_redirect.host();

      // When the existing scheme in database is empty, then difference in
      // schemes is not considered a scheme mismatch. This case is treated
      // specially since scheme was added later to the database, and previous
      // entries would have empty scheme. In such case, we do not consider this
      // as a mismatch, and simply update the scheme in the database.
      const bool url_scheme_mismatch =
          !redirect.url_scheme().empty() &&
          redirect.url_scheme() != final_redirect.scheme();

      // When the existing port in database is empty, then difference in
      // ports is not considered a mismatch. This case is treated
      // specially since port was added later to the database, and previous
      // entries would have empty value. In such case, we simply update the port
      // in the database.
      const bool url_port_mismatch =
          redirect.has_url_port() &&
          redirect.url_port() != final_redirect.EffectiveIntPort();

      if (!host_mismatch && !url_scheme_mismatch && !url_port_mismatch) {
        // No mismatch.
        need_to_add = false;
        redirect.set_number_of_hits(redirect.number_of_hits() + 1);
        redirect.set_consecutive_misses(0);

        // If existing scheme or port in database are empty, then update them.
        if (redirect.url_scheme().empty())
          redirect.set_url_scheme(final_redirect.scheme());
        if (!redirect.has_url_port())
          redirect.set_url_port(final_redirect.EffectiveIntPort());
      } else {
        // A real mismatch.
        redirect.set_number_of_misses(redirect.number_of_misses() + 1);
        redirect.set_consecutive_misses(redirect.consecutive_misses() + 1);
      }
    }

    if (need_to_add) {
      RedirectStat* redirect_to_add = data.add_redirect_endpoints();
      redirect_to_add->set_url(final_redirect.host());
      redirect_to_add->set_number_of_hits(1);
      redirect_to_add->set_url_scheme(final_redirect.scheme());
      redirect_to_add->set_url_port(final_redirect.EffectiveIntPort());
    }
  }

  // Trim the redirects after the update.
  ResourcePrefetchPredictorTables::TrimRedirects(
      &data, config_.max_redirect_consecutive_misses);

  if (data.redirect_endpoints_size() == 0)
    redirect_data->DeleteData({key});
  else
    redirect_data->UpdateData(key, data);
}

void ResourcePrefetchPredictor::LearnOrigins(
    const std::string& host,
    const GURL& main_frame_origin,
    const std::map<url::Origin, OriginRequestSummary>& summaries) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (host.size() > ResourcePrefetchPredictorTables::kMaxStringLength)
    return;

  OriginData data;
  bool exists = origin_data_->TryGetData(host, &data);
  if (!exists) {
    data.set_host(host);
    data.set_last_visit_time(base::Time::Now().ToInternalValue());
    size_t origins_size = summaries.size();
    auto ordered_origins =
        std::vector<const OriginRequestSummary*>(origins_size);
    for (const auto& kv : summaries) {
      size_t index = kv.second.first_occurrence;
      DCHECK_LT(index, origins_size);
      ordered_origins[index] = &kv.second;
    }

    for (const OriginRequestSummary* summary : ordered_origins) {
      auto* origin_to_add = data.add_origins();
      InitializeOriginStatFromOriginRequestSummary(origin_to_add, *summary);
    }
  } else {
    data.set_last_visit_time(base::Time::Now().ToInternalValue());

    std::map<url::Origin, int> old_index;
    int old_size = static_cast<int>(data.origins_size());
    for (int i = 0; i < old_size; ++i) {
      bool is_new =
          old_index
              .insert({url::Origin::Create(GURL(data.origins(i).origin())), i})
              .second;
      DCHECK(is_new);
    }

    // Update the old origins.
    for (int i = 0; i < old_size; ++i) {
      auto* old_origin = data.mutable_origins(i);
      url::Origin origin = url::Origin::Create(GURL(old_origin->origin()));
      auto it = summaries.find(origin);
      if (it == summaries.end()) {
        // miss
        old_origin->set_number_of_misses(old_origin->number_of_misses() + 1);
        old_origin->set_consecutive_misses(old_origin->consecutive_misses() +
                                           1);
      } else {
        // hit: update.
        const auto& new_origin = it->second;
        old_origin->set_always_access_network(new_origin.always_access_network);
        old_origin->set_accessed_network(new_origin.accessed_network);

        int position = new_origin.first_occurrence + 1;
        int total =
            old_origin->number_of_hits() + old_origin->number_of_misses();
        old_origin->set_average_position(
            ((old_origin->average_position() * total) + position) /
            (total + 1));
        old_origin->set_number_of_hits(old_origin->number_of_hits() + 1);
        old_origin->set_consecutive_misses(0);
      }
    }

    // Add new origins.
    for (const auto& kv : summaries) {
      if (old_index.find(kv.first) != old_index.end())
        continue;

      auto* origin_to_add = data.add_origins();
      InitializeOriginStatFromOriginRequestSummary(origin_to_add, kv.second);
    }
  }

  // Trim and Sort.
  ResourcePrefetchPredictorTables::TrimOrigins(&data,
                                               config_.max_consecutive_misses);
  ResourcePrefetchPredictorTables::SortOrigins(&data, main_frame_origin.spec());
  if (data.origins_size() > static_cast<int>(config_.max_origins_per_entry)) {
    data.mutable_origins()->DeleteSubrange(
        config_.max_origins_per_entry,
        data.origins_size() - config_.max_origins_per_entry);
  }

  // Update the database.
  if (data.origins_size() == 0)
    origin_data_->DeleteData({host});
  else
    origin_data_->UpdateData(host, data);
}

void ResourcePrefetchPredictor::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(initialization_state_ == INITIALIZED);

  if (deletion_info.IsAllHistory())
    DeleteAllUrls();
  else
    DeleteUrls(deletion_info.deleted_rows());
}

void ResourcePrefetchPredictor::OnHistoryServiceLoaded(
    history::HistoryService* history_service) {
  if (initialization_state_ == INITIALIZING) {
    OnHistoryAndCacheLoaded();
  }
}

void ResourcePrefetchPredictor::ConnectToHistoryService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(INITIALIZING, initialization_state_);

  // Register for HistoryServiceLoading if it is not ready.
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (!history_service)
    return;
  DCHECK(!history_service_observer_.IsObserving(history_service));
  history_service_observer_.Add(history_service);
  if (history_service->BackendLoaded()) {
    // HistoryService is already loaded. Continue with Initialization.
    OnHistoryAndCacheLoaded();
  }
}

////////////////////////////////////////////////////////////////////////////////
// TestObserver.

TestObserver::~TestObserver() {
  predictor_->SetObserverForTesting(nullptr);
}

TestObserver::TestObserver(ResourcePrefetchPredictor* predictor)
    : predictor_(predictor) {
  predictor_->SetObserverForTesting(this);
}

}  // namespace predictors
