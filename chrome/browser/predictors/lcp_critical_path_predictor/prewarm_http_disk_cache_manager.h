// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_PREWARM_HTTP_DISK_CACHE_MANAGER_H_
#define CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_PREWARM_HTTP_DISK_CACHE_MANAGER_H_

#include <queue>
#include <tuple>

#include "base/containers/lru_cache.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "net/base/isolation_info.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
class HttpResponseHeaders;
}

namespace predictors {

// PrewarmHttpDiskCacheManager prewarms the HTTP disk cache.
//
// - Only warms up the HTTP disk cache entries for the given URL.
// - Does not go to the network.
// - Processes the requested URLs sequentially to avoid heavy load.
// - Runs on UI thread in an asynchronous manner.
// - Only supports the top frame's resources.
class PrewarmHttpDiskCacheManager
    : public network::SimpleURLLoaderStreamConsumer {
 public:
  explicit PrewarmHttpDiskCacheManager(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~PrewarmHttpDiskCacheManager() override;
  PrewarmHttpDiskCacheManager(const PrewarmHttpDiskCacheManager&) = delete;
  PrewarmHttpDiskCacheManager& operator=(const PrewarmHttpDiskCacheManager&) =
      delete;

  void MaybePrewarmResources(
      const GURL& top_frame_main_resource_url,
      const std::vector<GURL>& top_frame_subresource_urls);

 private:
  friend class PrewarmHttpDiskCacheManagerTest;

  void MaybeAddPrewarmJob(const url::Origin& top_frame_origin,
                          const GURL& url,
                          net::IsolationInfo::RequestType request_type);
  void MaybeProcessNextQueuedJob();
  void PrewarmHttpDiskCache(GURL url);

  // Implements network::SimpleURLLoaderStreamConsumer
  void OnDataReceived(std::string_view string_piece,
                      base::OnceClosure resume) override;
  void OnComplete(bool success) override;
  void OnRetry(base::OnceClosure start_retry) override;

  // Called when we are discarding the body inside SimpleURLLoader;
  void OnHeadersOnly(scoped_refptr<net::HttpResponseHeaders> ignored);

  // Handles completion of a load, and possibly starts a new one.
  void DoComplete();

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::queue<std::tuple<url::Origin, GURL, net::IsolationInfo::RequestType>>
      queued_jobs_;
  // Keeps recent warm-up history to prevent excessive duplicate
  // warm-up. The maximum size of prewarm_history_ must be large enough
  // to avoid excessive duplicated warm-up requests.
  base::LRUCache<std::tuple<url::Origin, GURL, net::IsolationInfo::RequestType>,
                 base::TimeTicks>
      prewarm_history_;
  const base::TimeDelta reprewarm_period_;
  const bool use_read_and_discard_body_option_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  base::OnceCallback<void()> prewarm_finished_callback_for_testing_;
  base::WeakPtrFactory<PrewarmHttpDiskCacheManager> weak_factory_{this};
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_PREWARM_HTTP_DISK_CACHE_MANAGER_H_
