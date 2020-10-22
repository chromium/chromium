// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_H_

#include <memory>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_window_observer.h"
#include "chrome/browser/ui/ash/screenshot_area.h"

class GURL;
struct ScreenshotArea;

namespace aura {
class Window;
}  // namespace aura

namespace content {
class WebContents;
}  // namespace content

namespace policy {

// System-wide class that tracks the set of currently known confidential
// WebContents and whether any of them are currently visible.
// If any confidential WebContents is visible, the corresponding restrictions
// will be enforced according to the current enterprise policy.
class DlpContentManager : public DlpWindowObserver::Delegate {
 public:
  // Creates the instance if not yet created.
  // There will always be a single instance created on the first access.
  static DlpContentManager* Get();

  // DlpWindowObserver::Delegate overrides:
  void OnWindowOcclusionChanged(aura::Window* window) override;

  // Returns which restrictions are applied to the |web_contents| according to
  // the policy.
  DlpContentRestrictionSet GetConfidentialRestrictions(
      content::WebContents* web_contents) const;

  // Returns which restrictions are applied to the WebContents which are
  // currently visible.
  DlpContentRestrictionSet GetOnScreenPresentRestrictions() const;

  // Returns whether screenshots should be restricted.
  virtual bool IsScreenshotRestricted(const ScreenshotArea& area) const;

  // Returns whether video capture should be restricted.
  bool IsVideoCaptureRestricted(const ScreenshotArea& area) const;

  // Returns whether printing should be restricted.
  bool IsPrintingRestricted(content::WebContents* web_contents) const;

  // Called when video capturing for |area| is started.
  // |stop_callback| will be called when restricted content will appear there.
  void OnVideoCaptureStarted(const ScreenshotArea& area,
                             base::OnceClosure stop_callback);

  // Called when video capturing is stopped.
  void OnVideoCaptureStopped();

  // The caller (test) should manage |dlp_content_manager| lifetime.
  // Reset doesn't delete the object.
  static void SetDlpContentManagerForTesting(
      DlpContentManager* dlp_content_manager);
  static void ResetDlpContentManagerForTesting();

 private:
  FRIEND_TEST_ALL_PREFIXES(DlpContentManagerBrowserTest, ScreenshotsRestricted);
  FRIEND_TEST_ALL_PREFIXES(DlpContentManagerBrowserTest,
                           VideoCaptureStoppedWhenConfidentialWindowResized);
  FRIEND_TEST_ALL_PREFIXES(DlpContentManagerBrowserTest,
                           VideoCaptureStoppedWhenNonConfidentialWindowResized);
  FRIEND_TEST_ALL_PREFIXES(DlpContentManagerBrowserTest,
                           VideoCaptureNotStoppedWhenConfidentialWindowHidden);
  friend class DlpContentManagerTest;
  friend class DlpContentTabHelper;
  friend class MockDlpContentManager;

  DlpContentManager();
  ~DlpContentManager() override;
  DlpContentManager(const DlpContentManager&) = delete;
  DlpContentManager& operator=(const DlpContentManager&) = delete;

  // Called from DlpContentTabHelper:
  // Being called when confidentiality state changes for |web_contents|, e.g.
  // because of navigation.
  virtual void OnConfidentialityChanged(
      content::WebContents* web_contents,
      const DlpContentRestrictionSet& restriction_set);
  // Called when |web_contents| is about to be destroyed.
  virtual void OnWebContentsDestroyed(content::WebContents* web_contents);
  // Should return which restrictions are being applied to the |url| according
  // to the policies.
  virtual DlpContentRestrictionSet GetRestrictionSetForURL(
      const GURL& url) const;
  // Called when |web_contents| becomes visible or not.
  virtual void OnVisibilityChanged(content::WebContents* web_contents);

  // Helper to remove |web_contents| from the confidential set.
  void RemoveFromConfidential(content::WebContents* web_contents);

  // Updates |on_screen_restrictions_| and calls
  // OnScreenRestrictionsChanged() if needed.
  void MaybeChangeOnScreenRestrictions();

  // Called when the restrictions for currently visible content changes.
  void OnScreenRestrictionsChanged(
      const DlpContentRestrictionSet& added_restrictions,
      const DlpContentRestrictionSet& removed_restrictions) const;

  // Removes PrivacyScreen enforcement after delay if it's still not enforced.
  void MaybeRemovePrivacyScreenEnforcement() const;

  // Returns whether |restriction| is currently enforced for |area|.
  bool IsAreaRestricted(const ScreenshotArea& area,
                        DlpContentRestriction restriction) const;

  // Checks and stops the running video capture if restricted content appeared
  // in the corresponding areas.
  void CheckRunningVideoCapture();

  // Get the delay before switching privacy screen off.
  static base::TimeDelta GetPrivacyScreenOffDelayForTesting();

  // Map from currently known confidential WebContents to the restrictions.
  base::flat_map<content::WebContents*, DlpContentRestrictionSet>
      confidential_web_contents_;

  // Map of window observers for the current confidential WebContents.
  base::flat_map<content::WebContents*, std::unique_ptr<DlpWindowObserver>>
      window_observers_;

  // Set of restriction applied to the currently visible content.
  DlpContentRestrictionSet on_screen_restrictions_;

  // The currently running video capture are and callback to stop, if any.
  base::Optional<std::pair<ScreenshotArea, base::OnceClosure>>
      running_video_capture_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_H_
