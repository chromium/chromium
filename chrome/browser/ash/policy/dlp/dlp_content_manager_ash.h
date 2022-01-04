// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_DLP_CONTENT_MANAGER_ASH_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_DLP_CONTENT_MANAGER_ASH_H_

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/capture_mode/capture_mode_delegate.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/dlp/dlp_window_observer.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_contents.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
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

class DlpReportingManager;

class DlpWarnNotifier;

// System-wide class that tracks the set of currently known confidential
// WebContents and whether any of them are currently visible.
// If any confidential WebContents is visible, the corresponding restrictions
// will be enforced according to the current enterprise policy.
class DlpContentManagerAsh : public DlpContentManager,
                             public DlpWindowObserver::Delegate {
 public:
  DlpContentManagerAsh(const DlpContentManagerAsh&) = delete;
  DlpContentManagerAsh& operator=(const DlpContentManagerAsh&) = delete;

  // Creates the instance if not yet created.
  // There will always be a single instance created on the first access.
  static DlpContentManagerAsh* Get();

  // DlpWindowObserver::Delegate overrides:
  void OnWindowOcclusionChanged(aura::Window* window) override;
  void OnWindowDestroying(aura::Window* window) override;

  // Returns which restrictions are applied to the |web_contents| according to
  // the policy.
  DlpContentRestrictionSet GetConfidentialRestrictions(
      content::WebContents* web_contents) const;

  // Returns which restrictions are applied to the WebContents which are
  // currently visible.
  DlpContentRestrictionSet GetOnScreenPresentRestrictions() const;

  // Returns whether screenshots should be restricted for extensions API.
  virtual bool IsScreenshotApiRestricted(const ScreenshotArea& area);

  // Checks whether screenshots of |area| are restricted or not advised.
  // Depending on the result, calls |callback| and passes an indicator whether
  // to proceed or not.
  void CheckScreenshotRestriction(
      const ScreenshotArea& area,
      ash::OnCaptureModeDlpRestrictionChecked callback);

  // Returns whether screen capture of the defined content should be restricted.
  // TODO(crbug.com/1257493): Remove when it won't be used anymore.
  virtual bool IsScreenCaptureRestricted(
      const content::DesktopMediaID& media_id);

  // Checks whether screen sharing of content from the |media_id| source with
  // application |application_name| is restricted or not advised. Depending on
  // the result, calls |callback| and passes an indicator whether to proceed or
  // not.
  void CheckScreenShareRestriction(
      const content::DesktopMediaID& media_id,
      const std::u16string& application_title,
      OnDlpRestrictionCheckedCallback callback) override;

  // Called when video capturing for |area| is started.
  void OnVideoCaptureStarted(const ScreenshotArea& area);

  // Called when video capturing is stopped. Calls |callback| with an indicator
  // whether to proceed or not, based on DLP restrictions and potentially
  // confidential content captured.
  void CheckStoppedVideoCapture(
      ash::OnCaptureModeDlpRestrictionChecked callback);

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
      base::OnceClosure stop_callback,
      content::MediaStreamUI::StateChangeCallback state_change_callback);

  // Called when screen capture is stopped.
  void OnScreenCaptureStopped(const std::string& label,
                              const content::DesktopMediaID& media_id);

  // Called when an updated restrictions are received for Lacros window.
  void OnWindowRestrictionChanged(aura::Window* window,
                                  const DlpContentRestrictionSet& restrictions);

  // The caller (test) should manage |dlp_content_manager| lifetime.
  // Reset doesn't delete the object.
  // Please use ScopedDlpContentManagerAshForTesting instead of these methods,
  // if possible.
  static void SetDlpContentManagerAshForTesting(
      DlpContentManagerAsh* dlp_content_manager);
  static void ResetDlpContentManagerAshForTesting();

 protected:
  void SetReportingManagerForTesting(DlpReportingManager* manager);

  void SetWarnNotifierForTesting(
      std::unique_ptr<DlpWarnNotifier> warn_notifier);
  void ResetWarnNotifierForTesting();

 private:
  friend class DlpContentManagerAshTestHelper;
  friend class DlpContentTabHelper;
  friend class MockDlpContentManagerAsh;

  // Used to keep track of running screen shares.
  class ScreenShareInfo {
   public:
    ScreenShareInfo(
        const std::string& label,
        const content::DesktopMediaID& media_id,
        const std::u16string& application_title,
        base::OnceClosure stop_callback,
        content::MediaStreamUI::StateChangeCallback state_change_callback);
    ScreenShareInfo(const ScreenShareInfo& other) = delete;
    ScreenShareInfo& operator=(const ScreenShareInfo& other) = delete;
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
    // Stops the screen share. Can only be called once.
    void Stop();

    // If necessary, hides or shows the paused/resumed notification for this
    // screen share. The notification should be updated after changing the state
    // from running to paused, or paused to running.
    void MaybeUpdateNotifications();

    // If shown, hides both the paused and resumed notification for this screen
    // share.
    void HideNotifications();

    base::WeakPtr<ScreenShareInfo> GetWeakPtr();

   private:
    enum class NotificationState {
      kNotShowingNotification,
      kShowingPausedNotification,
      kShowingResumedNotification
    };
    enum class State { kRunning, kPaused, kStopped };
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
    base::OnceClosure stop_callback_;
    content::MediaStreamUI::StateChangeCallback state_change_callback_;
    State state_ = State::kRunning;
    NotificationState notification_state_ =
        NotificationState::kNotShowingNotification;

    base::WeakPtrFactory<ScreenShareInfo> weak_factory_{this};
  };

  // Structure to keep track of a running video capture.
  struct VideoCaptureInfo {
    explicit VideoCaptureInfo(const ScreenshotArea& area);

    const ScreenshotArea area;
    DlpConfidentialContents confidential_contents;
  };

  DlpContentManagerAsh();
  ~DlpContentManagerAsh() override;

  // DlpContentManager overrides:
  void OnConfidentialityChanged(
      content::WebContents* web_contents,
      const DlpContentRestrictionSet& restriction_set) override;
  void OnVisibilityChanged(content::WebContents* web_contents) override;
  void RemoveFromConfidential(content::WebContents* web_contents) override;

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

  // Returns which level, url, and information about visible confidential
  // contents of screen share restriction that is currently enforced for
  // |media_id|.
  ConfidentialContentsInfo GetScreenShareConfidentialContentsInfo(
      const content::DesktopMediaID& media_id) const;

  // Checks and stops the running video capture if restricted content appeared
  // in the corresponding areas.
  void CheckRunningVideoCapture();

  // Removes screen share from |running_screen_shares_|.
  void RemoveScreenShare(const std::string& label,
                         const content::DesktopMediaID& media_id);

  // Checks and stops the running screen shares if restricted content appeared
  // in the corresponding areas.
  void CheckRunningScreenShares();

  // Get the delay before switching privacy screen off.
  static base::TimeDelta GetPrivacyScreenOffDelayForTesting();

  // Helper method for async check of the restriction level, based on which
  // calls |callback| with an indicator whether to proceed or not.
  void CheckScreenCaptureRestriction(
      ConfidentialContentsInfo info,
      ash::OnCaptureModeDlpRestrictionChecked callback);

  // Called back from Screen Share warning dialogs that are shown during the
  // screen share. Passes along the user's response, reflected in the value of
  // |should_proceed| along to |callback| which handles continuing or cancelling
  // the action based on this response. In case that |should_proceed| is true,
  // also saves the |confidential_contents| that were allowed to be shared by
  // the user to avoid future warnings.
  void OnDlpScreenShareWarnDialogReply(
      const DlpConfidentialContents& confidential_contents,
      base::WeakPtr<ScreenShareInfo> screen_share,
      bool should_proceed);

  // Map of window observers for the current confidential WebContents.
  base::flat_map<content::WebContents*, std::unique_ptr<DlpWindowObserver>>
      web_contents_window_observers_;

  // Map from currently known Lacros Windows to their restrictions.
  base::flat_map<aura::Window*, DlpContentRestrictionSet> confidential_windows_;

  // Map of observers for currently known Lacros Windows.
  base::flat_map<aura::Window*, std::unique_ptr<DlpWindowObserver>>
      window_observers_;

  // Set of restriction applied to the currently visible content.
  DlpContentRestrictionSet on_screen_restrictions_;

  // Information about the currently running video capture area if any.
  absl::optional<VideoCaptureInfo> running_video_capture_info_;

  // List of the currently running screen shares.
  std::vector<base::WeakPtr<ScreenShareInfo>> running_screen_shares_;
};

// Helper class to call SetDlpContentManagerAshForTesting and
// ResetDlpContentManagerAshForTesting automically.
// The caller (test) should manage `test_dlp_content_manager` lifetime.
// This class does not own it.
class ScopedDlpContentManagerAshForTesting {
 public:
  explicit ScopedDlpContentManagerAshForTesting(
      DlpContentManagerAsh* test_dlp_content_manager);
  ~ScopedDlpContentManagerAshForTesting();
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DLP_CONTENT_MANAGER_ASH_H_
