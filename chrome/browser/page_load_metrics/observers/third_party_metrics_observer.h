// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_THIRD_PARTY_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_THIRD_PARTY_METRICS_OBSERVER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "net/cookies/canonical_cookie.h"
#include "url/gurl.h"
#include "url/origin.h"

// Records metrics about third-party storage accesses to a page.
class ThirdPartyMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  ThirdPartyMetricsObserver();
  ~ThirdPartyMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnCookiesRead(const GURL& url,
                     const GURL& first_party_url,
                     const net::CookieList& cookie_list,
                     bool blocked_by_policy) override;
  void OnCookieChange(const GURL& url,
                      const GURL& first_party_url,
                      const net::CanonicalCookie& cookie,
                      bool blocked_by_policy) override;
  void OnDomStorageAccessed(const GURL& url,
                            const GURL& first_party_url,
                            bool local,
                            bool blocked_by_policy) override;
  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void OnTimingUpdate(
      content::RenderFrameHost* subframe_rfh,
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 private:
  enum class AccessType { kRead, kWrite };

  struct CookieAccessTypes {
    explicit CookieAccessTypes(AccessType access_type);
    bool read = false;
    bool write = false;
  };

  enum class StorageType { kLocalStorage, kSessionStorage };

  struct StorageAccessTypes {
    explicit StorageAccessTypes(StorageType storage_type);
    bool local_storage = false;
    bool session_storage = false;
  };

  void OnCookieAccess(const GURL& url,
                      const GURL& first_party_url,
                      bool blocked_by_policy,
                      AccessType access_type);
  void RecordMetrics();

  // A map of third parties that have read or written cookies on this page. A
  // third party document.cookie access happens when the context's registrable
  // domain differs from the main frame's. A third party resource request
  // happens when the URL request's registrable domain differs from the main
  // frame's. For URLs which have no registrable domain, the hostname is used
  // instead.
  std::map<std::string, CookieAccessTypes> third_party_cookie_access_types_;

  // A map of third parties that have accessed storage other than cookies. A
  // third party access happens when the context's origin differs from the main
  // frame's.
  std::map<url::Origin, StorageAccessTypes> third_party_storage_access_types_;

  // A set of RenderFrameHosts that we've recorded timing data for. The
  // RenderFrameHosts are later removed when they navigate again or are deleted.
  std::set<content::RenderFrameHost*> recorded_frames_;

  // If the page has any blocked_by_policy cookie or DOM storage access (e.g.,
  // block third-party cookies is enabled) then we don't want to record any
  // metrics for the page.
  bool should_record_metrics_ = true;

  DISALLOW_COPY_AND_ASSIGN(ThirdPartyMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_THIRD_PARTY_METRICS_OBSERVER_H_
