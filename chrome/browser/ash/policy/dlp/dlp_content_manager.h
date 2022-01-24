// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_DLP_CONTENT_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_DLP_CONTENT_MANAGER_H_

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/capture_mode/capture_mode_delegate.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/dlp/dlp_confidential_contents.h"
#include "chrome/browser/ash/policy/dlp/dlp_warn_dialog.h"
#include "chrome/browser/ash/policy/dlp/dlp_window_observer.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_observer.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "chrome/browser/ui/ash/screenshot_area.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/media_stream_request.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

struct ScreenshotArea;

namespace aura {
class Window;
}  // namespace aura

namespace content {
class WebContents;
}  // namespace content

namespace policy {

using OnDlpRestrictionCheckedCallback =
    base::OnceCallback<void(bool should_proceed)>;

class DlpReportingManager;

// System-wide class that tracks the set of currently known confidential
// WebContents and whether any of them are currently visible.
// If any confidential WebContents is visible, the corresponding restrictions
// will be enforced according to the current enterprise policy.
class DlpContentManager : public DlpContentObserver,
                          public DlpWindowObserver::Delegate {
 public:
  DlpContentManager(const DlpContentManager&) = delete;
  DlpContentManager& operator=(const DlpContentManager&) = delete;

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
  // TODO(crbug.com/1257493): Remove when it won't be used anymore.
  virtual bool IsScreenshotRestricted(const ScreenshotArea& area);

  // Returns whether screenshots should be restricted for extensions API.
  virtual bool IsScreenshotApiRestricted(const ScreenshotArea& area);

  // Checks whether screenshots of |area| are restricted or not advised.
  // Depending on the result, calls |callback| and passes an indicator whether
  // to proceed or not.
  void CheckScreenshotRestriction(
      const ScreenshotArea& area,
      ash::OnCaptureModeDlpRestrictionChecked callback);

  // Returns whether video capture should be restricted.
  // TODO(crbug.com/1257493): Remove when it won't be used anymore.
  bool IsVideoCaptureRestricted(const ScreenshotArea& area);

  // Checks whether video capture of |area| is restricted or not advised.
  // Depending on the result, calls |callback| and passes an indicator whether
  // to proceed or not.
  void CheckVideoCaptureRestriction(
      const ScreenshotArea& area,
      ash::OnCaptureModeDlpRestrictionChecked callback);

  // Returns whether printing should be restricted.
  bool IsPrintingRestricted(content::WebContents* web_contents);

  // Returns whether the user should be warned before printing.
  bool ShouldWarnBeforePrinting(content::WebContents* web_contents);

  // Returns whether screen capture of the defined content should be restricted.
  // TODO(crbug.com/1257493): Remove when it won't be used anymore.
  virtual bool IsScreenCaptureRestricted(
      const content::DesktopMediaID& media_id);

  // Checks whether screen sharing of content from the |media_id| source with
  // the calling application |application_title| is restricted or not advised.
  // Depending on the result, calls |callback| and passes an indicator whether
  // to proceed or not.
  void CheckScreenShareRestriction(const content::DesktopMediaID& media_id,
                                   const std::u16string& application_title,
                                   OnDlpRestrictionCheckedCallback callback);

  // Called when video capturing for |area| is started.
  void OnVideoCaptureStarted(const ScreenshotArea& area);

  // Called when video capturing is stopped. Calls |callback| with an indicator
  // whether to proceed or not, based on DLP restrictions and potentially
  // confidential content captured.
  void CheckStoppedVideoCapture(
      ash::OnCaptureModeDlpRestrictionChecked callback);

  // Returns whether initiation of capture mode should be restricted because
  // any restricted content is currently visible.
  // TODO(crbug.com/1257493): Remove when it won't be used anymore.
  bool IsCaptureModeInitRestricted();

  // Checks whether initiation of capture mode is restricted or not advised
  // based on the currently visible content. Depending on the result, calls
  // |callback| and passes an indicator whether to proceed or not.
  void CheckCaptureModeInitRestriction(
      ash::OnCaptureModeDlpRestrictionChecked callback);

  // Called when screen capture is started.
  // |state_change_callback| will be called when restricted content will appear
  // or disappear in the captured area.
  void OnScreenCaptureStarted(
      const std::string& label,
      std::vector<content::DesktopMediaID> screen_capture_ids,
      const std::u16string& application_title,
      content::MediaStreamUI::StateChangeCallback state_change_callback);

  // Called when screen capture is stopped.
  void OnScreenCaptureStopped(const std::string& label,
                              const content::DesktopMediaID& media_id);

  // Called when a Capture Mode session is finished to reset the stored user's
  // choice.
  void ResetCaptureModeAllowance();

  // The caller (test) should manage |dlp_content_manager| lifetime.
  // Reset doesn't delete the object.
  // Please use ScopedDlpContentManagerForTesting instead of these methods,
  // if possible.
  static void SetDlpContentManagerForTesting(
      DlpContentManager* dlp_content_manager);
  static void ResetDlpContentManagerForTesting();

 protected:
  void SetReportingManagerForTesting(DlpReportingManager* manager);

 private:
  friend class DlpContentManagerTestHelper;
  friend class DlpContentTabHelper;
  friend class MockDlpContentManager;

  // Used to keep track of running screen shares.
  class ScreenShareInfo {
   public:
    ScreenShareInfo();
    ScreenShareInfo(
        const std::string& label,
        const content::DesktopMediaID& media_id,
        const std::u16string& application_title,
        content::MediaStreamUI::StateChangeCallback state_change_callback);
    ScreenShareInfo(const ScreenShareInfo& other);
    ScreenShareInfo& operator=(const ScreenShareInfo& other);
    ~ScreenShareInfo();

    bool operator==(const ScreenShareInfo& other) const;
    bool operator!=(const ScreenShareInfo& other) const;

    const content::DesktopMediaID& GetMediaId() const;
    const std::string& GetLabel() const;
    const std::u16string& GetApplicationTitle() const;
    bool IsRunning() const;

    // Pauses a running screen share.
    // No-op if the screen share is already paused.
    void Pause();
    // Resumes a paused screen share.
    // No-op if the screen share is already running.
    void Resume();

    // If necessary, hides or shows the paused/resumed notification for this
    // screen share. The notification should be updated after changing the state
    // from running to paused, or paused to running.
    void MaybeUpdateNotifications();

    // If shown, hides both the paused and resumed notification for this screen
    // share.
    void HideNotifications();

   private:
    enum class NotificationState {
      kNotShowingNotification,
      kShowingPausedNotification,
      kShowingResumedNotification
    };
    // Shows (if |show| is true) or hides (if |show| is false) paused
    // notification for this screen share. Does nothing if the notification is
    // already in the required state.
    void UpdatePausedNotification(bool show);
    // Shows (if |show| is true) or hides (if |show| is false) resumed
    // notification for this screen share. Does nothing if the notification is
    // already in the required state.
    void UpdateResumedNotification(bool show);

    std::string label_;
    content::DesktopMediaID media_id_;
    // TODO(crbug.com/1264793): Don't cache the application name.
    std::u16string application_title_;
    content::MediaStreamUI::StateChangeCallback state_change_callback_;
    bool is_running_ = true;
    NotificationState notification_state_ =
        NotificationState::kNotShowingNotification;
  };

  // Structure to keep track of a running video capture.
  struct VideoCaptureInfo {
    explicit VideoCaptureInfo(const ScreenshotArea& area);

    const ScreenshotArea area;
    DlpConfidentialContents confidential_contents;
  };

  // Structure to relate a list of confidential contents to the corresponding
  // restriction level.
  struct ConfidentialContentsInfo {
    RestrictionLevelAndUrl restriction_info;
    DlpConfidentialContents confidential_contents;
  };

  DlpContentManager();
  ~DlpContentManager() override;

  // Initializing to be called separately to make testing possible.
  virtual void Init();

  // DlpContentObserver overrides:
  void OnConfidentialityChanged(
      content::WebContents* web_contents,
      const DlpContentRestrictionSet& restriction_set) override;
  void OnWebContentsDestroyed(content::WebContents* web_contents) override;
  void OnVisibilityChanged(content::WebContents* web_contents) override;

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

  // Returns the information about contents that are currently visible on
  // screen, for which the highest level |restriction| is enforced.
  ConfidentialContentsInfo GetConfidentialContentsOnScreen(
      DlpContentRestriction restriction) const;

  // Returns level, url, and information about visible confidential contents of
  // |restriction| that is currently enforced for |area|.
  ConfidentialContentsInfo GetAreaConfidentialContentsInfo(
      const ScreenshotArea& area,
      DlpContentRestriction restriction) const;

  // Returns which level, url, and information about visible confidential
  // contents of screen share restriction that is currently enforced for
  // |media_id|.
  ConfidentialContentsInfo GetScreenShareConfidentialContentsInfo(
      const content::DesktopMediaID& media_id) const;

  // Checks and stops the running video capture if restricted content appeared
  // in the corresponding areas.
  void CheckRunningVideoCapture();

  // Called back from Screen Share warning dialogs that are shown during the
  // screen share. Saves the user's response, based on which either resumes or
  // fully stops the share.
  void OnScreenShareReply(DlpConfidentialContents& confidential_contents,
                          ScreenShareInfo screen_share,
                          bool should_proceed);

  // Called back from Screen Share warning dialogs before it's started. Saves
  // the user's response and passes it along to |callback| which handles
  // continuing or cancelling the action based on this response.
  void OnScreenShareInitReply(DlpConfidentialContents& confidential_contents,
                              OnDlpRestrictionCheckedCallback callback,
                              bool should_proceed);

  // Checks and stops the running screen shares if restricted content appeared
  // in the corresponding areas.
  void CheckRunningScreenShares();

  // Get the delay before switching privacy screen off.
  static base::TimeDelta GetPrivacyScreenOffDelayForTesting();

  // Returns which level and url of printing restriction is currently enforced
  // for |web_contents|.
  RestrictionLevelAndUrl GetPrintingRestrictionInfo(
      content::WebContents* web_contents) const;

  // Helper method for async check of the restriction level, based on which
  // calls |callback| with an indicator whether to proceed or not.
  void CheckScreenCaptureRestriction(
      ConfidentialContentsInfo info,
      ash::OnCaptureModeDlpRestrictionChecked callback);

  // Reports events if required by the |restriction_info| and
  // `reporting_manager` is configured.
  void MaybeReportEvent(const RestrictionLevelAndUrl& restriction_info,
                        DlpRulesManager::Restriction restriction);

  // Reports warning events if required by the |restriction_info| and
  // `reporting_manager` is configured.
  void MaybeReportWarnEvent(const RestrictionLevelAndUrl& restriction_info,
                            DlpRulesManager::Restriction restriction);

  // Called back from Screen Capture warning dialogs. Saves the user's response
  // and passes it along to |callback| which handles continuing or cancelling
  // the action based on this response.
  void OnScreenCaptureReply(ash::OnCaptureModeDlpRestrictionChecked callback,
                            bool proceed);

  // Map from currently known confidential WebContents to the restrictions.
  base::flat_map<content::WebContents*, DlpContentRestrictionSet>
      confidential_web_contents_;

  // Map of window observers for the current confidential WebContents.
  base::flat_map<content::WebContents*, std::unique_ptr<DlpWindowObserver>>
      window_observers_;

  // Set of restriction applied to the currently visible content.
  DlpContentRestrictionSet on_screen_restrictions_;

  // Information about the currently running video capture area if any.
  absl::optional<VideoCaptureInfo> running_video_capture_info_;

  // List of the currently running screen shares.
  std::vector<ScreenShareInfo> running_screen_shares_;

  // List of contents for which the user bypassed the warning during the current
  // user session.
  // TODO(crbug.com/1264803): Limit the size + delete if related WebContents are
  // destroyed
  DlpConfidentialContents user_allowed_contents_for_screen_share_;

  // Keeps track of user's selection after being shown a warning modal
  // dialog, to avoid showing the dialog multiple times during the same capture
  // mode session.
  bool user_allowed_screen_capture_ = false;

  DlpReportingManager* reporting_manager_;
};

// Helper class to call SetDlpContentManagerForTesting and
// ResetDlpContentManagerForTesting automically.
// The caller (test) should manage `test_dlp_content_manager` lifetime.
// This class does not own it.
class ScopedDlpContentManagerForTesting {
 public:
  explicit ScopedDlpContentManagerForTesting(
      DlpContentManager* test_dlp_content_manager);
  ~ScopedDlpContentManagerForTesting();
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DLP_CONTENT_MANAGER_H_
