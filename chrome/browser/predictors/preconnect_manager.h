// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_PRECONNECT_MANAGER_H_
#define CHROME_BROWSER_PREDICTORS_PRECONNECT_MANAGER_H_

#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/id_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/predictors/proxy_lookup_client_impl.h"
#include "chrome/browser/predictors/resolve_host_client_impl.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "net/base/network_anonymization_key.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

namespace network {
namespace mojom {
class NetworkContext;
}
}  // namespace network

namespace predictors {

struct PreconnectRequest;

struct PreconnectedRequestStats {
  PreconnectedRequestStats(const url::Origin& origin, bool was_preconnected);
  PreconnectedRequestStats(const PreconnectedRequestStats& other);
  ~PreconnectedRequestStats();

  url::Origin origin;
  bool was_preconnected;
};

struct PreconnectStats {
  explicit PreconnectStats(const GURL& url);

  // Stats must be moved only.
  PreconnectStats(const PreconnectStats&) = delete;
  PreconnectStats& operator=(const PreconnectStats&) = delete;

  ~PreconnectStats();

  GURL url;
  base::TimeTicks start_time;
  std::vector<PreconnectedRequestStats> requests_stats;
};

// Stores the status of all preconnects associated with a given |url|.
struct PreresolveInfo {
  PreresolveInfo(const GURL& url, size_t count);

  PreresolveInfo(const PreresolveInfo&) = delete;
  PreresolveInfo& operator=(const PreresolveInfo&) = delete;

  ~PreresolveInfo();

  bool is_done() const { return queued_count == 0 && inflight_count == 0; }

  GURL url;
  size_t queued_count;
  size_t inflight_count = 0;
  bool was_canceled = false;
  std::unique_ptr<PreconnectStats> stats;
};

// Stores all data need for running a preresolve and a subsequent optional
// preconnect for a |url|.
struct PreresolveJob {
  PreresolveJob(const GURL& url,
                int num_sockets,
                bool allow_credentials,
                net::NetworkAnonymizationKey network_anonymization_key,
                PreresolveInfo* info);

  PreresolveJob(const PreresolveJob&) = delete;
  PreresolveJob& operator=(const PreresolveJob&) = delete;

  PreresolveJob(PreconnectRequest preconnect_request, PreresolveInfo* info);
  PreresolveJob(PreresolveJob&& other);

  ~PreresolveJob();

  bool need_preconnect() const {
    return num_sockets > 0 && !(info && info->was_canceled);
  }

  GURL url;
  int num_sockets;
  bool allow_credentials;
  net::NetworkAnonymizationKey network_anonymization_key;
  // Raw pointer usage is fine here because even though PreresolveJob can
  // outlive PreresolveInfo. It's only accessed on PreconnectManager class
  // context and PreresolveInfo lifetime is tied to PreconnectManager.
  // May be equal to nullptr in case of detached job.
  raw_ptr<PreresolveInfo, DanglingUntriaged> info;
  std::unique_ptr<ResolveHostClientImpl> resolve_host_client;
  std::unique_ptr<ProxyLookupClientImpl> proxy_lookup_client;
  base::TimeTicks creation_time;
};

// PreconnectManager is responsible for preresolving and preconnecting to
// origins based on the input list of URLs.
//  - The input list of URLs is associated with a main frame url that can be
//  used for cancelling.
//  - Limits the total number of preresolves in flight.
//  - Preresolves an URL before preconnecting to it to have a better control on
//  number of speculative dns requests in flight.
//  - When stopped, waits for the pending preresolve requests to finish without
//  issuing preconnects for them.
//  - All methods of the class must be called on the UI thread.
class PreconnectManager {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when a preconnect to |preconnect_url| is initiated for |url|.
    virtual void PreconnectInitiated(const GURL& url,
                                     const GURL& preconnect_url) = 0;

    // Called when all preresolve jobs for the |stats->url| are finished. Note
    // that some preconnect jobs can be still in progress, because they are
    // fire-and-forget.
    // Is called on the UI thread.
    virtual void PreconnectFinished(std::unique_ptr<PreconnectStats> stats) = 0;
  };

  // An observer for testing.
  class Observer {
   public:
    virtual ~Observer() {}

    virtual void OnPreconnectUrl(const GURL& url,
                                 int num_sockets,
                                 bool allow_credentials) {}

    virtual void OnPreresolveFinished(
        const GURL& url,
        const net::NetworkAnonymizationKey& network_anonymization_key,
        bool success) {}
    virtual void OnProxyLookupFinished(
        const GURL& url,
        const net::NetworkAnonymizationKey& network_anonymization_key,
        bool success) {}
  };

  static const size_t kMaxInflightPreresolves = 3;

  PreconnectManager(base::WeakPtr<Delegate> delegate,
                    content::BrowserContext* browser_context);

  PreconnectManager(const PreconnectManager&) = delete;
  PreconnectManager& operator=(const PreconnectManager&) = delete;

  virtual ~PreconnectManager();

  // Starts preconnect and preresolve jobs associated with |url|.
  virtual void Start(const GURL& url, std::vector<PreconnectRequest> requests);

  // Starts special preconnect and preresolve jobs that are not cancellable and
  // don't report about their completion. They are considered more important
  // than trackable requests thus they are put in the front of the jobs queue.
  //
  // |network_anonymization_key| specifies the key that the corresponding
  // network requests are expected to use. If a request is issued with a
  // different key, it may not use the prefetched DNS entry or preconnected
  // socket.
  virtual void StartPreresolveHost(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key);
  virtual void StartPreresolveHosts(
      const std::vector<GURL>& urls,
      const net::NetworkAnonymizationKey& network_anonymization_key);
  virtual void StartPreconnectUrl(
      const GURL& url,
      bool allow_credentials,
      net::NetworkAnonymizationKey network_anonymization_key);

  // No additional jobs associated with the |url| will be queued after this.
  virtual void Stop(const GURL& url);

  base::WeakPtr<PreconnectManager> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void SetNetworkContextForTesting(
      network::mojom::NetworkContext* network_context) {
    network_context_ = network_context;
  }

  void SetObserverForTesting(Observer* observer) { observer_ = observer; }

 private:
  using PreresolveJobMap = base::IDMap<std::unique_ptr<PreresolveJob>>;
  using PreresolveJobId = PreresolveJobMap::KeyType;
  friend class PreconnectManagerTest;

  void PreconnectUrl(
      const GURL& url,
      int num_sockets,
      bool allow_credentials,
      const net::NetworkAnonymizationKey& network_anonymization_key) const;
  std::unique_ptr<ResolveHostClientImpl> PreresolveUrl(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      ResolveHostCallback callback) const;
  std::unique_ptr<ProxyLookupClientImpl> LookupProxyForUrl(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      ProxyLookupCallback callback) const;

  // Whether the PreconnectManager should be performing preloading operations
  // or if preloading is disabled.
  bool IsEnabled();
  void TryToLaunchPreresolveJobs();
  void OnPreresolveFinished(PreresolveJobId job_id, bool success);
  void OnProxyLookupFinished(PreresolveJobId job_id, bool success);
  void FinishPreresolveJob(PreresolveJobId job_id, bool success);
  void AllPreresolvesForUrlFinished(PreresolveInfo* info);

  // NOTE: Returns a non-null pointer outside of unittesting contexts.
  network::mojom::NetworkContext* GetNetworkContext() const;

  base::WeakPtr<Delegate> delegate_;
  const raw_ptr<content::BrowserContext> browser_context_;
  std::list<PreresolveJobId> queued_jobs_;
  PreresolveJobMap preresolve_jobs_;
  std::map<GURL, std::unique_ptr<PreresolveInfo>> preresolve_info_;
  size_t inflight_preresolves_count_ = 0;

  // Only used in tests.
  raw_ptr<network::mojom::NetworkContext> network_context_ = nullptr;
  raw_ptr<Observer> observer_ = nullptr;

  base::WeakPtrFactory<PreconnectManager> weak_factory_{this};
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_PRECONNECT_MANAGER_H_
