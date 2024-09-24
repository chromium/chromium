// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_PREFETCH_MANAGER_H_
#define CHROME_BROWSER_PREDICTORS_PREFETCH_MANAGER_H_

#include <list>
#include <map>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "net/base/network_anonymization_key.h"
#include "services/network/public/mojom/url_loader.mojom-forward.h"
#include "url/gurl.h"

class Profile;

namespace blink {
class ThrottlingURLLoader;
}

namespace network {
namespace mojom {
class URLLoaderClient;
}
class SharedURLLoaderFactory;
struct URLLoaderCompletionStatus;
}

namespace predictors {

struct PrefetchRequest;
struct PrefetchInfo;
struct PrefetchJob;

static constexpr size_t kMaxInflightPrefetches = 3;

struct PrefetchStats {
  explicit PrefetchStats(const GURL& url);
  ~PrefetchStats();

  PrefetchStats(const PrefetchStats&) = delete;
  PrefetchStats& operator=(const PrefetchStats&) = delete;

  GURL url;
  base::TimeTicks start_time;
  // TODO(falken): Add stats about what was requested to measure
  // the accuracy.
};

// PrefetchManager prefetches input lists of URLs.
//  - The input list of URLs is associated with a main frame url that can be
//  used for cancelling.
//  - Limits the total number of prefetches in flight.
//  - All methods of the class must be called on the UI thread.
//
//  This class is very similar to PreconnectManager, which does
//  preresolve/preconnect instead of prefetching. It is only
//  usable when LoadingPredictorPrefetch is enabled.
class PrefetchManager {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when a prefetch is initiated. |prefetch_url| is the subresource
    // being prefetched, and |url| is the main frame of the navigation.
    virtual void PrefetchInitiated(const GURL& url,
                                   const GURL& prefetch_url) = 0;

    // Called when all prefetch jobs for the |stats->url| are finished.
    // Called on the UI thread.
    virtual void PrefetchFinished(std::unique_ptr<PrefetchStats> stats) = 0;
  };

  // For testing.
  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void OnPrefetchFinished(
        const GURL& url,
        const GURL& prefetch_url,
        const network::URLLoaderCompletionStatus& status) {}
    virtual void OnAllPrefetchesFinished(const GURL& url) {}
  };

  PrefetchManager(base::WeakPtr<Delegate> delegate, Profile* profile);
  ~PrefetchManager();

  PrefetchManager(const PrefetchManager&) = delete;
  PrefetchManager& operator=(const PrefetchManager&) = delete;

  // Starts prefetch jobs keyed by |url|.
  void Start(const GURL& url, std::vector<PrefetchRequest> requests);

  // Stops further prefetch jobs keyed by |url|. Queued jobs will never start;
  // started jobs will continue to completion.
  void Stop(const GURL& url);

  base::WeakPtr<PrefetchManager> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Called by PrefetchInfo.
  void AllPrefetchJobsForUrlFinished(PrefetchInfo& info);

  void set_observer_for_testing(Observer* observer) {
    observer_for_testing_ = observer;
  }

 private:
  friend class PrefetchManagerTest;

  void PrefetchUrl(std::unique_ptr<PrefetchJob> job,
                   scoped_refptr<network::SharedURLLoaderFactory> factory);
  void OnPrefetchFinished(
      std::unique_ptr<PrefetchJob> job,
      std::unique_ptr<blink::ThrottlingURLLoader> loader,
      std::unique_ptr<network::mojom::URLLoaderClient> client,
      const network::URLLoaderCompletionStatus& status);
  void TryToLaunchPrefetchJobs();

  base::WeakPtr<Delegate> delegate_;
  const raw_ptr<Profile, DanglingUntriaged> profile_;

  // All the jobs that haven't yet started. A job is removed once it starts.
  // Inflight jobs destruct once finished.
  std::list<std::unique_ptr<PrefetchJob>> queued_jobs_;

  std::map<GURL, std::unique_ptr<PrefetchInfo>> prefetch_info_;

  // The total number of prefetches that have started and not yet finished,
  // across all main frame URLs.
  size_t inflight_jobs_count_ = 0;

  raw_ptr<Observer, DanglingUntriaged> observer_for_testing_ = nullptr;

  base::WeakPtrFactory<PrefetchManager> weak_factory_{this};
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_PREFETCH_MANAGER_H_
