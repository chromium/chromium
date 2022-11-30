// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_URL_LOADER_INTERCEPTOR_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_URL_LOADER_INTERCEPTOR_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_prefetch_status.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_probe_result.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

class PrefetchedMainframeResponseContainer;

// Intercepts prefetch proxy navigations that are eligible to be isolated.
class PrefetchProxyURLLoaderInterceptor
    : public content::URLLoaderRequestInterceptor {
 public:
  explicit PrefetchProxyURLLoaderInterceptor(int frame_tree_node_id);

  PrefetchProxyURLLoaderInterceptor(const PrefetchProxyURLLoaderInterceptor&) =
      delete;
  PrefetchProxyURLLoaderInterceptor& operator=(
      const PrefetchProxyURLLoaderInterceptor&) = delete;

  ~PrefetchProxyURLLoaderInterceptor() override;

  // content::URLLoaderRequestInterceptor:
  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      content::BrowserContext* browser_context,
      content::URLLoaderRequestInterceptor::LoaderCallback callback) override;

 protected:
  // Virtual for testing
  virtual std::unique_ptr<PrefetchedMainframeResponseContainer>
  GetPrefetchedResponse(const GURL& url);

 private:
  // Ensures the cookies from the mainframe have been copied to the normal
  // profile before calling |InterceptPrefetchedNavigation|.
  void EnsureCookiesCopiedAndInterceptPrefetchedNavigation(
      const network::ResourceRequest& tentative_resource_request,
      std::unique_ptr<PrefetchedMainframeResponseContainer> prefetch);

  void InterceptPrefetchedNavigation(
      const network::ResourceRequest& tentative_resource_request,
      std::unique_ptr<PrefetchedMainframeResponseContainer>);
  void DoNotInterceptNavigation();

  // Check if this navigation is a NoStatePrefetch and should try to use a
  // prefetched response. Returns true if the navigation is intercepted.
  bool MaybeInterceptNoStatePrefetchNavigation(
      const network::ResourceRequest& tentative_resource_request);

  // Called when the probe finishes with |result|.
  void OnProbeComplete(base::OnceClosure on_success_callback,
                       PrefetchProxyProbeResult result);

  // Notifies the Tab Helper about the usage of a prefetched resource.
  void NotifyPrefetchStatusUpdate(PrefetchProxyPrefetchStatus usage) const;

  // Used to get the current WebContents.
  const int frame_tree_node_id_;

  // The url that |MaybeCreateLoader| is called with.
  GURL url_;

  // The time when probing was started. Used to calculate probe latency which is
  // reported to the tab helper.
  absl::optional<base::TimeTicks> probe_start_time_;

  // The time when we started waiting for cookies to be copied, delaying the
  // navigation. Used to calculate total cookie wait time.
  absl::optional<base::TimeTicks> cookie_copy_start_time_;

  // Set in |MaybeCreateLoader| and used in |On[DoNot]InterceptRequest|.
  content::URLLoaderRequestInterceptor::LoaderCallback loader_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PrefetchProxyURLLoaderInterceptor> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_URL_LOADER_INTERCEPTOR_H_
