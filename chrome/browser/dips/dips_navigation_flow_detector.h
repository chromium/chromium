// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_NAVIGATION_FLOW_DETECTOR_H_
#define CHROME_BROWSER_DIPS_DIPS_NAVIGATION_FLOW_DETECTOR_H_

#include <string>

#include "base/threading/sequence_bound.h"
#include "chrome/browser/dips/dips_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace {
struct PageVisitInfo {
  PageVisitInfo();
  PageVisitInfo(PageVisitInfo&& other);

  std::string site;
  ukm::SourceId source_id;
  bool did_page_access_cookies;
  bool did_page_access_storage;
  bool did_page_receive_user_activation;
  bool did_page_have_successful_waa;
  std::optional<bool> was_navigation_to_page_renderer_initiated;
  std::optional<bool> was_navigation_to_page_user_initiated;
  std::optional<bool> did_site_have_prior_activation_record;
};
}  // namespace

// Detects possible navigation flows with the aim of discovering how to
// distinguish user-interest navigation flows from navigational tracking.
// Currently only reports UKM to inform how we might identify possible
// navigational tracking by sites that also perform user-interest activity.
class DipsNavigationFlowDetector
    : public content::WebContentsObserver,
      public content::WebContentsUserData<DipsNavigationFlowDetector> {
 public:
  ~DipsNavigationFlowDetector() override;

  static void MaybeCreateForWebContents(content::WebContents* web_contents);

  void SetClockForTesting(base::Clock* clock) {
    CHECK(dips_service_);
    CHECK(clock);
    dips_service_->storage()
        ->AsyncCall(&DIPSStorage::SetClockForTesting)
        .WithArgs(clock);
    clock_ = *clock;
  }

 protected:
  explicit DipsNavigationFlowDetector(content::WebContents* web_contents,
                                      DIPSServiceImpl* dips_service);

  void MaybeEmitUkmForPreviousPage();
  bool CanEmitUkmForPreviousPage() {
    bool page_is_in_series_of_three = two_pages_ago_visit_info_.has_value() &&
                                      previous_page_visit_info_.has_value() &&
                                      current_page_visit_info_.has_value();
    if (!page_is_in_series_of_three) {
      return false;
    }

    bool page_has_valid_source_id =
        previous_page_visit_info_->source_id != ukm::kInvalidSourceId;
    bool site_had_triggering_storage_access =
        previous_page_visit_info_->did_page_access_cookies ||
        previous_page_visit_info_->did_page_access_storage;
    bool is_site_different_from_prior_page =
        previous_page_visit_info_->site != two_pages_ago_visit_info_->site;
    bool is_site_different_from_next_page =
        previous_page_visit_info_->site != current_page_visit_info_->site;

    return page_has_valid_source_id && site_had_triggering_storage_access &&
           is_site_different_from_prior_page &&
           is_site_different_from_next_page;
  }

 private:
  // So the controller can call the constructor.
  friend class DipsNavigationFlowController;
  // So WebContentsUserData::CreateForWebContents can call the constructor.
  friend class content::WebContentsUserData<DipsNavigationFlowDetector>;

  // start WebContentsObserver overrides
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  // For client-initiated cookie accesses, and late-reported cookie accesses in
  // navigations.
  void OnCookiesAccessed(content::RenderFrameHost* render_frame_host,
                         const content::CookieAccessDetails& details) override;
  // For cookie accesses in navigations.
  void OnCookiesAccessed(content::NavigationHandle* navigation_handle,
                         const content::CookieAccessDetails& details) override;
  void NotifyStorageAccessed(content::RenderFrameHost* render_frame_host,
                             blink::mojom::StorageTypeAccessed storage_type,
                             bool blocked) override;
  void FrameReceivedUserActivation(
      content::RenderFrameHost* render_frame_host) override;
  void WebAuthnAssertionRequestSucceeded(
      content::RenderFrameHost* render_frame_host) override;
  // end WebContentsObserver overrides

  void CheckIfSiteHadPriorActivation(GURL url);
  void GotDipsState(std::string site_read_state_for,
                    const DIPSState dips_state);

  std::optional<PageVisitInfo> two_pages_ago_visit_info_;
  std::optional<PageVisitInfo> previous_page_visit_info_;
  std::optional<PageVisitInfo> current_page_visit_info_;

  base::Time last_page_change_time_;
  long bucketized_previous_page_visit_duration_;

  // raw_ptr<> is safe here because DIPSService is a KeyedService, associated
  // with the BrowserContext/Profile, which will outlive the WebContents that
  // DipsNavigationFlowDetector is observing.
  raw_ptr<DIPSServiceImpl> dips_service_;

  raw_ref<base::Clock> clock_{*base::DefaultClock::GetInstance()};

  base::WeakPtrFactory<DipsNavigationFlowDetector> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_DIPS_DIPS_NAVIGATION_FLOW_DETECTOR_H_
