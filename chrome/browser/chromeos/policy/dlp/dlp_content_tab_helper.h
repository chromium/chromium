// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_TAB_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_TAB_HELPER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/auto_reset.h"
#include "base/containers/flat_map.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace policy {

// DlpContentTabHelper attaches to relevant WebContents that are covered by
// DLP (Data Leak Prevention) feature and observes navigation in all sub-frames
// as well as visibility of the WebContents and reports it to system-wide
// DlpContentManager.
// WebContents is considered as confidential if either the main frame or any
// of sub-frames are confidential according to the current policy.
// In this case the applied restrictions for the WebContents will be the
// superset of all restrictions for sub-frames.
class DlpContentTabHelper : public content::WebContentsObserver {
 public:
  DECLARE_USER_DATA(DlpContentTabHelper);

  // Creates DlpContentTabHelper for |web_contents| if the user is managed and
  // it's not an incognito profile. The TabInterface overload registers the
  // helper on |tab|'s UnownedUserDataHost; the WebContents-only overload is
  // used for non-tab WebContents such as Chrome App windows.
  [[nodiscard]] static std::unique_ptr<DlpContentTabHelper> MaybeCreate(
      tabs::TabInterface& tab,
      content::WebContents* web_contents);
  [[nodiscard]] static std::unique_ptr<DlpContentTabHelper> MaybeCreate(
      content::WebContents* web_contents);

  static DlpContentTabHelper* FromTab(tabs::TabInterface* tab);
  static DlpContentTabHelper* FromWebContents(
      content::WebContents* web_contents);

  // Allows to create DlpContentTabHelper even if the user is not managed and
  // without the need to initialize DlpRulesManager in tests.
  using ScopedIgnoreDlpRulesManager = base::AutoReset<bool>;
  static ScopedIgnoreDlpRulesManager IgnoreDlpRulesManagerForTesting();

  ~DlpContentTabHelper() override;

  // content::WebContentsObserver:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameHostStateChanged(
      content::RenderFrameHost* render_frame_host,
      content::RenderFrameHost::LifecycleState old_state,
      content::RenderFrameHost::LifecycleState new_state) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  using RfhInfo =
      std::pair<content::RenderFrameHost*, DlpContentRestrictionSet>;
  // Returns a vector of DLP restrictions info for all the tracked frames.
  std::vector<RfhInfo> GetFramesInfo() const;

 private:
  DlpContentTabHelper(tabs::TabInterface* tab,
                      content::WebContents* web_contents);
  DlpContentTabHelper(const DlpContentTabHelper&) = delete;
  DlpContentTabHelper& operator=(const DlpContentTabHelper&) = delete;

  // Returns the superset of all restrictions for sub-frames.
  DlpContentRestrictionSet GetRestrictionSet() const;

  // Adds or updates the |render_frame_host| and |restrictions| to/in the map
  // and notifies DlpContentManager if needed.
  void AddFrame(content::RenderFrameHost* render_frame_host,
                DlpContentRestrictionSet restrictions);

  // Removes |render_frame_host| from the map and notifies DlpContentManager if
  // needed.
  void RemoveFrame(content::RenderFrameHost* render_frame_host);

  // Map from the currently known confidential frames to the corresponding
  // restriction set.
  base::flat_map<content::RenderFrameHost*, DlpContentRestrictionSet>
      confidential_frames_;

  std::optional<ui::ScopedUnownedUserData<DlpContentTabHelper>>
      scoped_unowned_user_data_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_TAB_HELPER_H_
