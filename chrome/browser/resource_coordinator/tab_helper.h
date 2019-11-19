// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_HELPER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_HELPER_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/process/kill.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace resource_coordinator {

class LocalSiteCharacteristicsWebContentsObserver;

class ResourceCoordinatorTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ResourceCoordinatorTabHelper> {
 public:
  ~ResourceCoordinatorTabHelper() override;

  // Helper function to check if a given WebContents is loaded. Returns true by
  // default if there's no TabHelper for this content.
  static bool IsLoaded(content::WebContents* contents);

  // Helper function to check if a given WebContents is frozen. Returns false by
  // default if there's no TabHelper for this content.
  static bool IsFrozen(content::WebContents* contents);

  // WebContentsObserver overrides.
  void DidStartLoading() override;
  void DidReceiveResponse() override;
  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code,
                   const base::string16& error_description) override;
  void RenderProcessGone(base::TerminationStatus status) override;
  void WebContentsDestroyed() override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  void UpdateUkmRecorder(int64_t navigation_id);
  ukm::SourceId ukm_source_id() const { return ukm_source_id_; }
  void SetUkmSourceIdForTest(ukm::SourceId id) { ukm_source_id_ = id; }

#if !defined(OS_ANDROID)
  LocalSiteCharacteristicsWebContentsObserver*
  local_site_characteristics_wc_observer() {
    return local_site_characteristics_wc_observer_.get();
  }
#endif

 private:
  explicit ResourceCoordinatorTabHelper(content::WebContents* web_contents);

  // TODO(siggi): This is used by the TabLifecycleUnit, remove this with it.
  ukm::SourceId ukm_source_id_ = ukm::kInvalidSourceId;

  friend class content::WebContentsUserData<ResourceCoordinatorTabHelper>;

#if !defined(OS_ANDROID)
  std::unique_ptr<LocalSiteCharacteristicsWebContentsObserver>
      local_site_characteristics_wc_observer_;
#endif


  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(ResourceCoordinatorTabHelper);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_HELPER_H_
