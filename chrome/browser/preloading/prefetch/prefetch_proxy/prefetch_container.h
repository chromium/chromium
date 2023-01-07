// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_CONTAINER_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_CONTAINER_H_

#include <memory>

#include "base/time/time.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/chrome_speculation_host_delegate.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_cookie_listener.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_network_context.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_prefetch_status.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_type.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetched_mainframe_response_container.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

class Profile;

// This class contains the state for a request to prefetch a page. This
// encompasses the prefetch for the page itself as well as the prefetches for
// any subresources for the page.
class PrefetchContainer {
 public:
  PrefetchContainer(const GURL& url,
                    const PrefetchType& prefetch_type,
                    size_t original_prediction_index);
  ~PrefetchContainer();

  PrefetchContainer(const PrefetchContainer&) = delete;
  PrefetchContainer& operator=(const PrefetchContainer&) = delete;

  // The URL that will potentially be prefetched.
  GURL GetUrl() const { return url_; }

  // The type of this Prefetch. Used to control how the prefetch is handled.
  const PrefetchType& GetPrefetchType() const { return prefetch_type_; }

  // Changes the type of this prefetch.
  void ChangePrefetchType(const PrefetchType& new_prefetch_type);

  // The ordering of this prefetch in the context of other prefetches from the
  // same main frame.
  size_t GetOriginalPredictionIndex() const {
    return original_prediction_index_;
  }

  // The status of the current prefetch. Note that |HasPrefetchStatus| will be
  // initially false until |SetPrefetchStatus| is called.
  void SetPrefetchStatus(PrefetchProxyPrefetchStatus prefetch_status) {
    prefetch_status_ = prefetch_status;
  }
  bool HasPrefetchStatus() const { return prefetch_status_.has_value(); }
  PrefetchProxyPrefetchStatus GetPrefetchStatus() const;

  // The possible statuses of NoStatePrefetch for a URL. All prefetches start
  // with a status of kNotStarted. Then the only valid transitions are
  // kNotStarted to kInProgress, kInProgress to kSucceeded, and kInProgress to
  // kFailed. All other possible transitions are invalid.
  enum class NoStatePrefetchStatus {
    kNotStarted,
    kInProgress,
    kSucceeded,
    kFailed,
  };

  // Tracks the state of NSP for this |url_|. Note that NSP can only be run if
  // |allowed_to_prefetch_subresources_| is true.
  void SetNoStatePrefetchStatus(NoStatePrefetchStatus no_state_prefetch_status);
  NoStatePrefetchStatus GetNoStatePrefetchStatus() const {
    return no_state_prefetch_status_;
  }

  // Whether this prefetch is a decoy. Decoy prefetches will not store the
  // response, and not serve any prefetched resources.
  void SetIsDecoy(bool is_decoy) { is_decoy_ = is_decoy; }
  bool IsDecoy() const { return is_decoy_; }

  // After the initial eligiblity check for |url_|, a
  // |PrefetchProxyCookieListener| listens for any changes to the cookies
  // associated with |url_|. If these cookies change, then no prefetched
  // resources will be served.
  void RegisterCookieListener(
      base::OnceCallback<void(const GURL&)> on_cookie_changed_callback,
      network::mojom::CookieManager* cookie_manager);
  void StopCookieListener();
  bool HaveCookiesChanged() const;

  // Whether there is a non-null value for |prefetched_response_|.
  bool HasPrefetchedResponse() const { return prefetched_response_ != nullptr; }

  // Whether the response is still valid and not stale. For this to be true,
  // then |prefetched_response| must be less than |cacheable_duration| old.
  bool IsPrefetchedResponseValid(base::TimeDelta cacheable_duration) const;

  // Gives ownership of |prefetched_response| to this instance. Additionally
  // sets the value of |prefetch_received_time_| to the current time, which is
  // used later on to determine if the response is stale or valid.
  void SetPrefetchedResponse(
      std::unique_ptr<PrefetchedMainframeResponseContainer>
          prefetched_response);

  // Releases ownership of |prefetched_response_| from this instance and gives
  // it to the caller.
  std::unique_ptr<PrefetchedMainframeResponseContainer>
  ReleasePrefetchedResponse();

  // Returns a copy of |prefetched_response_|, and this instance retains
  // ownership of the original.
  std::unique_ptr<PrefetchedMainframeResponseContainer>
  ClonePrefetchedResponse() const;

  // The network context used for just this prefetch.
  void CreateNetworkContextForPrefetch(Profile* profile);
  PrefetchProxyNetworkContext* GetNetworkContext() const {
    return network_context_.get();
  }
  std::unique_ptr<PrefetchProxyNetworkContext> ReleaseNetworkContext();

  // Returns request id to be used by DevTools
  const std::string& RequestId() const { return request_id_; }

  void SetDevToolsObserver(
      base::WeakPtr<content::SpeculationHostDevToolsObserver>
          devtools_observer) {
    devtools_observer_ = std::move(devtools_observer);
  }

  const base::WeakPtr<content::SpeculationHostDevToolsObserver>&
  GetDevToolsObserver() const {
    return devtools_observer_;
  }

 private:
  // The URL that will potentially be prefetched.
  // TODO(crbug.com/1266876): The container needs to track the entire redirect
  // chain.
  const GURL url_;

  // The type of this prefetch. This controls some specific details about how
  // the prefetch is handled, including whether an isolated network context or
  // the default network context is used to perform the prefetch, whether or
  // not the preftch proxy is used, and whether or not subresources are
  // prefetched.
  PrefetchType prefetch_type_;

  // The ordering of this prefetch in the context of other prefetches from the
  // same main frame.
  const size_t original_prediction_index_;

  // The current status, if any, of the prefetch.
  absl::optional<PrefetchProxyPrefetchStatus> prefetch_status_;

  // The status of the NoStatePrefetch for this prefetch. Note that NSP can only
  // be run if |allowed_to_prefetch_subresources_| is true.
  NoStatePrefetchStatus no_state_prefetch_status_ =
      NoStatePrefetchStatus::kNotStarted;

  // Whether this prefetch is a decoy or not. If the prefetch is a decoy then
  // any prefetched resources will not be served and will not be served.
  bool is_decoy_ = false;

  // This tracks whether the cookies associated with |url_| have changed at some
  // point after the initial eligibility check.
  std::unique_ptr<PrefetchProxyCookieListener> cookie_listener_;

  // The prefetched response for |url_|.
  std::unique_ptr<PrefetchedMainframeResponseContainer> prefetched_response_;

  // The time at which |prefetched_response_| was received. This is used to
  // determine if |prefetched_response_| is stale.
  absl::optional<base::TimeTicks> prefetch_received_time_;

  // The network context used to prefetch |url_|.
  std::unique_ptr<PrefetchProxyNetworkContext> network_context_;

  // Request identifier used by DevTools
  std::string request_id_;

  // Weak pointer to DevTools observer
  base::WeakPtr<content::SpeculationHostDevToolsObserver> devtools_observer_;
};

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_CONTAINER_H_
