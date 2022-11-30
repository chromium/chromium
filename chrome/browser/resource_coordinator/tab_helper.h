// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_HELPER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_HELPER_H_

#include "base/process/kill.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace resource_coordinator {

class ResourceCoordinatorTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ResourceCoordinatorTabHelper> {
 public:
  ResourceCoordinatorTabHelper(const ResourceCoordinatorTabHelper&) = delete;
  ResourceCoordinatorTabHelper& operator=(const ResourceCoordinatorTabHelper&) =
      delete;

  ~ResourceCoordinatorTabHelper() override;

  // Helper function to check if a given WebContents is loaded. Returns true by
  // default if there's no TabHelper for this content.
  static bool IsLoaded(content::WebContents* contents);

  // WebContentsObserver overrides.
  void DidStopLoading() override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  void WebContentsDestroyed() override;
  void PrimaryPageChanged(content::Page& page) override;

  void UpdateUkmRecorder(int64_t navigation_id);
  ukm::SourceId ukm_source_id() const { return ukm_source_id_; }
  void SetUkmSourceIdForTest(ukm::SourceId id) { ukm_source_id_ = id; }

 private:
  explicit ResourceCoordinatorTabHelper(content::WebContents* web_contents);

  // TODO(siggi): This is used by the TabLifecycleUnit, remove this with it.
  ukm::SourceId ukm_source_id_ = ukm::kInvalidSourceId;

  friend class content::WebContentsUserData<ResourceCoordinatorTabHelper>;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_HELPER_H_
