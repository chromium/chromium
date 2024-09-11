// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_DLP_CONTENT_MANAGER_ASH_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_DLP_CONTENT_MANAGER_ASH_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/public/cpp/capture_mode/capture_mode_delegate.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/dlp/dlp_window_observer.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_contents.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chromeos/ash/experiences/screenshot_area/screenshot_area.h"
#include "components/exo/window_properties.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/media_stream_request.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "ui/aura/window_observer.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"
#include "url/gurl.h"

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
class DlpContentManagerAsh : public DlpContentManager,
                             public DlpWindowObserver::Delegate,
                             public wm::ActivationChangeObserver {
 public:
  DlpContentManagerAsh(const DlpContentManagerAsh&) = delete;
  DlpContentManagerAsh& operator=(const DlpContentManagerAsh&) = delete;

  // Creates the instance if not yet created.
  // There will always be a single instance created on the first access.
  static DlpContentManagerAsh* Get();

  // DlpWindowObserver::Delegate overrides:
  void OnWindowOcclusionChanged(aura::Window* window) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowTitleChanged(aura::Window* window) override;

  // Returns which restrictions are applied to the WebContents which are
  // currently visible.
  DlpContentRestrictionSet GetOnScreenPresentRestrictions() const;

  // Checks whether screenshots of |area| are restricted or not advised.
  // Depending on the result, calls |callback| and passes an indicator whether
  // to proceed or not.
  void CheckScreenshotRestriction(
      const ScreenshotArea& area,
      ash::OnCaptureModeDlpRestrictionChecked callback);

  // Called when video capturing for |area| is started.
  void OnVideoCaptureStarted(const ScreenshotArea& area);

  // Called when video capturing is stopped. Calls |callback| with an indicator
  // whether to proceed or not, based on DLP restrictions and potentially
  // confidential content captured.
  void CheckStoppedVideoCapture(
      ash::OnCaptureModeDlpRestrictionChecked callback);

  // Called when screenshot is taken for |area|.
  void OnImageCapture(const ScreenshotArea& area);

  // Checks whether initiation of capture mode is restricted or not advised
  // based on the currently visible content. Depending on the result, calls
  // |callback| and passes an indicator whether to proceed or not.
  void CheckCaptureModeInitRestriction(
      ash::OnCaptureModeDlpRestrictionChecked callback);

  // DlpContentManager overrides:
  void CheckScreenShareRestriction(const content::DesktopMediaID& media_id,
                                   const std::u16string& application_title,
                                   WarningCallback callback) override;
  void OnScreenShareStarted(
      const std::string& label,
      std::vector<content::DesktopMediaID> screen_share_ids,
      const std::u16string& application_title,
      base::RepeatingClosure stop_callback,
      content::MediaStreamUI::StateChangeCallback state_change_callback,
      content::MediaStreamUI::SourceCallback source_callback) override;
  void OnScreenShareStopped(const std::string& label,
                            const content::DesktopMediaID& media_id) override;

  // Called when an updated restrictions are received for Lacros window.
  void OnWindowRestrictionChanged(mojo::ReceiverId receiver_id,
                                  const std::string& window_id,
                                  const DlpContentRestrictionSet& restrictions);

  // ActivationChangeObserver:
  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // Clean pending restrictions for a receiver.
  void CleanPendingRestrictions(mojo::ReceiverId receiver_id);

 private:
  friend class DlpContentManagerTestHelper;
  friend class DlpContentObserver;
  friend class DlpContentTabHelper;

  // Structure to keep track of a running video capture.
  struct VideoCaptureInfo {
    explicit VideoCaptureInfo(const ScreenshotArea& area);

    const ScreenshotArea area;
    DlpConfidentialContents confidential_contents;
    // Contents reported during a video capture, after the start of a capture.
    DlpConfidentialContents reported_confidential_contents;
    // Flag that indicates that there was some content with warn level
    // restriction captured. Used to indicate that the warn UMA should be
    // logged, even if no warning is shown.
    bool had_warning_restriction = false;
  };

  DlpContentManagerAsh();
  ~DlpContentManagerAsh() override;

  // DlpContentManager overrides:
  void OnConfidentialityChanged(
      content::WebContents* web_contents,
      const DlpContentRestrictionSet& restriction_set) override;
  void OnVisibilityChanged(content::WebContents* web_contents) override;
  void RemoveFromConfidential(content::WebContents* web_contents) override;
  ConfidentialContentsInfo GetScreenShareConfidentialContentsInfo(
      const content::DesktopMediaID& media_id,
      content::WebContents* web_contents) const override;
  void TabLocationMaybeChanged(content::WebContents* web_contents) override;

  // Updates |on_screen_restrictions_| and calls
  // OnScreenRestrictionsChanged() if needed.
  void MaybeChangeOnScreenRestrictions();

  // Called when the restrictions for currently visible content changes.
  void OnScreenRestrictionsChanged(
      const DlpContentRestrictionSet& added_restrictions,
      const DlpContentRestrictionSet& removed_restrictions);

  // Removes PrivacyScreen enforcement after delay if it's still not enforced.
  void MaybeRemovePrivacyScreenEnforcement() const;

  // Returns the information about contents that are currently visible on
  // screen, for which the highest level |restriction| is enforced.
  ConfidentialContentsInfo GetConfidentialContentsOnScreen(
      DlpContentRestriction restriction) const;

  // Returns level, url, and information about visible confidential contents of
  // |restriction| that is currently enforced for |area|.
  ConfidentialContentsInfo GetAreaConfidentialContentsInfo(
      const ScreenshotArea& area,
      DlpContentRestriction restriction) const;

  // Checks and stops the running video capture if restricted content appeared
  // in the corresponding areas.
  void CheckRunningVideoCapture();

  // Get the delay before switching privacy screen off.
  static base::TimeDelta GetPrivacyScreenOffDelayForTesting();

  // Helper method for async check of the restriction level, based on which
  // calls |callback| with an indicator whether to proceed or not.
  void CheckScreenCaptureRestriction(
      ConfidentialContentsInfo info,
      ash::OnCaptureModeDlpRestrictionChecked callback);

  // Map of window observers for the current confidential WebContents.
  base::flat_map<content::WebContents*, std::unique_ptr<DlpWindowObserver>>
      web_contents_window_observers_;

  // Map from currently known Lacros Windows to their restrictions.
  base::flat_map<aura::Window*, DlpContentRestrictionSet> confidential_windows_;

  // Map of observers for currently known Lacros Windows.
  base::flat_map<aura::Window*, std::unique_ptr<DlpWindowObserver>>
      window_observers_;
  // Map of observers for Lacros surfaces that are being notified for visibility
  // changes.
  base::flat_map<aura::Window*, std::unique_ptr<DlpWindowObserver>>
      surface_observers_;

  // Set of restriction applied to the currently visible content.
  DlpContentRestrictionSet on_screen_restrictions_;

  // Information about the currently running video capture area if any.
  std::optional<VideoCaptureInfo> running_video_capture_info_;

  // Cache for restrictions, which are sent to ash with a window id before the
  // id is known to ash. The window is (considered to be) invisible, so the
  // restrictions are not applied as long as they are in the cache. The id of
  // the receiver is also saved.

  // Restrictions might be sent from lacros to ash before the window is known to
  // ash. The pending restrictions are saved here until the window gets active
  // in wayland / ash.
  std::map<std::string, std::pair<mojo::ReceiverId, DlpContentRestrictionSet>>
      pending_restrictions_;

  // Map to save all windows of a receiver with pending restrictions.
  std::map<mojo::ReceiverId, std::set<std::string>> pending_restrictions_owner_;

  base::ScopedObservation<::wm::ActivationClient, wm::ActivationChangeObserver>
      window_activation_observation_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DLP_CONTENT_MANAGER_ASH_H_
