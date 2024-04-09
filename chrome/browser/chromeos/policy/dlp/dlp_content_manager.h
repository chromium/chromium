// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_H_

#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/dlp_warn_dialog.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_contents.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager_observer.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_observer.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_tab_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/media_stream_request.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace content {
struct DesktopMediaID;
class WebContents;
}  // namespace content

namespace data_controls {
class DlpReportingManager;
}  // namespace data_controls

namespace policy {

class DlpWarnNotifier;

// System-wide class that tracks the set of currently known confidential
// WebContents and whether any of them are currently visible.
// If any confidential WebContents is visible, the corresponding restrictions
// will be enforced according to the current enterprise policy.
class DlpContentManager : public DlpContentObserver,
                          public BrowserListObserver,
                          public TabStripModelObserver {
 public:
  // Holds DLP restrictions information for `web_contents` object.
  struct WebContentsInfo {
    WebContentsInfo();
    WebContentsInfo(content::WebContents* web_contents,
                    DlpContentRestrictionSet restriction_set,
                    std::vector<DlpContentTabHelper::RfhInfo> rfh_info_vector);
    WebContentsInfo(const WebContentsInfo&);
    WebContentsInfo& operator=(const WebContentsInfo&);
    ~WebContentsInfo();

    raw_ptr<content::WebContents> web_contents = nullptr;
    // Restrictions set for `web_contents`.
    DlpContentRestrictionSet restriction_set;
    // DLP restrictions info for RenderFrameHosts in `web_contents`.
    std::vector<DlpContentTabHelper::RfhInfo> rfh_info_vector;
  };

  DlpContentManager(const DlpContentManager&) = delete;
  DlpContentManager& operator=(const DlpContentManager&) = delete;

  // Returns platform-specific implementation of the class. Never returns
  // nullptr.
  static DlpContentManager* Get();

  // Returns which restrictions are applied to the |web_contents| according to
  // the policy.
  DlpContentRestrictionSet GetConfidentialRestrictions(
      content::WebContents* web_contents) const;

  // Returns whether screenshare should be blocked for the specified
  // WebContents.
  bool IsScreenShareBlocked(content::WebContents* web_contents) const;

  // Checks whether printing of |web_contents| is restricted or not advised.
  // Depending on the result, calls |callback| and passes an indicator whether
  // to proceed or not.
  void CheckPrintingRestriction(content::WebContents* web_contents,
                                content::GlobalRenderFrameHostId rfh_id,
                                WarningCallback callback);

  // Returns whether screenshots should be restricted for extensions API.
  virtual bool IsScreenshotApiRestricted(content::WebContents* web_contents);

  // Checks whether screen sharing of content from the |media_id| source with
  // application |application_title| is restricted or not advised. Depending on
  // the result, calls |callback| and passes an indicator whether to proceed or
  // not.
  virtual void CheckScreenShareRestriction(
      const content::DesktopMediaID& media_id,
      const std::u16string& application_title,
      WarningCallback callback) = 0;

  // Called when screen share is started.
  // |state_change_callback| will be called when restricted content will appear
  // or disappear in the captured area to pause/resume the share.
  // |source_callback| will be called only to update the source for a tab share
  // before resuming the capture.
  // |stop_callback| will be called after a user dismisses a warning.
  virtual void OnScreenShareStarted(
      const std::string& label,
      std::vector<content::DesktopMediaID> screen_share_ids,
      const std::u16string& application_title,
      base::RepeatingClosure stop_callback,
      content::MediaStreamUI::StateChangeCallback state_change_callback,
      content::MediaStreamUI::SourceCallback source_callback) = 0;

  // Called when screen share is stopped.
  virtual void OnScreenShareStopped(
      const std::string& label,
      const content::DesktopMediaID& media_id) = 0;

  // Called when a screen share is being stopped before processing a source
  // change from |old_media_id| to |new_media_id|. The share might have been
  // paused by DLP due to restricted content, so should be resumed before the
  // change source request proceeds.
  virtual void OnScreenShareSourceChanging(
      const std::string& label,
      const content::DesktopMediaID& old_media_id,
      const content::DesktopMediaID& new_media_id,
      bool captured_surface_control_active);

  void AddObserver(DlpContentManagerObserver* observer,
                   DlpContentRestriction restriction);

  void RemoveObserver(const DlpContentManagerObserver* observer,
                      DlpContentRestriction restriction);

  // Returns an array of DLP restrictions info to all the tracked WebContents.
  std::vector<WebContentsInfo> GetWebContentsInfo() const;

 protected:
  friend class DlpContentManagerTestHelper;

  void SetReportingManagerForTesting(
      data_controls::DlpReportingManager* manager);

  void SetWarnNotifierForTesting(
      std::unique_ptr<DlpWarnNotifier> warn_notifier);
  void ResetWarnNotifierForTesting();

  // Sets the delay before resuming a screen share.
  static void SetScreenShareResumeDelayForTesting(base::TimeDelta delay);

  // Structure that relates a list of confidential contents to the
  // corresponding restriction level.
  struct ConfidentialContentsInfo {
    RestrictionLevelAndUrl restriction_info;
    DlpConfidentialContents confidential_contents;
  };

  // Used to keep track of running screen shares.
  class ScreenShareInfo {
   public:
    enum class State {
      kRunning,
      kPaused,
      kStopped,
      kRunningBeforeSourceChange,
      kPausedBeforeSourceChange
    };

    ScreenShareInfo(
        const std::string& label,
        const content::DesktopMediaID& media_id,
        const std::u16string& application_title,
        base::OnceClosure stop_callback,
        content::MediaStreamUI::StateChangeCallback state_change_callback,
        content::MediaStreamUI::SourceCallback source_callback);
    ~ScreenShareInfo();

    // Updates an existing ScreenShareInfo instance. Should only be called for
    // tab shares and after a source change has occurred. In addition to the
    // passed parameters, restores the state to a correct value and updates
    // notifications if necessary.
    void UpdateAfterSourceChange(
        const content::DesktopMediaID& media_id,
        const std::u16string& application_title,
        base::OnceClosure stop_callback,
        content::MediaStreamUI::StateChangeCallback state_change_callback,
        content::MediaStreamUI::SourceCallback source_callback);

    bool operator==(const ScreenShareInfo& other) const;
    bool operator!=(const ScreenShareInfo& other) const;

    const content::DesktopMediaID& media_id() const;
    const content::DesktopMediaID& new_media_id() const;
    void set_new_media_id(const content::DesktopMediaID& media_id);
    const std::string& label() const;
    const std::u16string& application_title() const;
    State state() const;
    base::WeakPtr<content::WebContents> web_contents() const;
    // Saves the |dialog_widget| as the current dialog handle.
    // Assumes that the previous widget is closed or not set. It's
    // responsibility of the called to ensure that the restriction level is
    // WARN.
    void set_dialog_widget(base::WeakPtr<views::Widget> dialog_widget);
    void set_latest_confidential_contents_info(
        ConfidentialContentsInfo confidential_contents_info);
    // Returns the restriction information that was the last enforced on this
    // screen share.
    const RestrictionLevelAndUrl& GetLatestRestriction() const;
    // Returns the confidential contents that caused the latest restriction.
    const DlpConfidentialContents& GetConfidentialContents() const;

    // Pauses a running screen share.
    // No-op if the screen share is already paused.
    void Pause();
    // Resumes a paused screen share.
    // For tab shares, resuming can be a consequence of navigating within a page
    // so calls the |source_callback_| to ensure that the |media_id| is updated.
    // No-op if the screen share is already running.
    void Resume();
    // Changes the state to indicate that the source is being changed.
    // Every source change stops the share and starts a new one, so this is
    // needed to store all the required information in the meantime and update
    // it if the source change is successful.
    void ChangeStateBeforeSourceChange();
    // Stops the screen share. Can only be called once.
    void Stop();
    // Start the screen share after source change if pending.
    void StartIfPending();

    // If necessary, hides or shows the paused/resumed notification for this
    // screen share. The notification should be updated after changing the state
    // from running to paused, or paused to running.
    void MaybeUpdateNotifications();

    // If shown, hides both the paused and resumed notification for this screen
    // share.
    void HideNotifications();

    // If currently opened, closes the associated DlpWarnDialog widget.
    void MaybeCloseDialogWidget();
    // Returns true if there is an associated DlpWarnDialog object, false
    // otherwise.
    bool HasOpenDialogWidget();

    // Records that Captured Surface Control was active at some point during
    // the capture.
    void SetCapturedSurfaceControlActive();

    base::WeakPtr<ScreenShareInfo> GetWeakPtr();

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
    content::DesktopMediaID new_media_id_;
    std::u16string application_title_;
    base::OnceClosure stop_callback_;
    content::MediaStreamUI::StateChangeCallback state_change_callback_;
    content::MediaStreamUI::SourceCallback source_callback_;
    State state_ = State::kRunning;
    NotificationState notification_state_ =
        NotificationState::kNotShowingNotification;
    // Information on the latest restriction enforced.
    ConfidentialContentsInfo latest_confidential_contents_info_;
    // Pointer to the associated DlpWarnDialog widget.
    // Not null only while the dialog is opened.
    base::WeakPtr<views::Widget> dialog_widget_ = nullptr;
    // Remembers that it should be restarted after source update.
    bool pending_start_on_source_change_ = false;

    // Set only for tab shares.
    base::WeakPtr<content::WebContents> web_contents_;

    // Only meaningful for tab-shares.
    bool captured_surface_control_active_ = false;

    base::WeakPtrFactory<ScreenShareInfo> weak_factory_{this};
  };

  DlpContentManager();
  ~DlpContentManager() override;

  // Reports events of type DlpPolicyEvent::Mode::WARN_PROCEED to
  // `reporting_manager`.
  static void ReportWarningProceededEvent(
      const GURL& url,
      DlpRulesManager::Restriction restriction,
      data_controls::DlpReportingManager* reporting_manager);

  // Helper method to create a callback with ReportWarningProceededEvent
  // function.
  static bool MaybeReportWarningProceededEvent(
      GURL url,
      DlpRulesManager::Restriction restriction,
      data_controls::DlpReportingManager* reporting_manager,
      bool should_proceed);

  // Retrieves WebContents from |media_id| for tab shares. Otherwise returns
  // nullptr.
  static content::WebContents* GetWebContentsFromMediaId(
      const content::DesktopMediaID& media_id);

  // Initializing to be called separately to make testing possible.
  virtual void Init();

  // DlpContentObserver overrides:
  void OnConfidentialityChanged(
      content::WebContents* web_contents,
      const DlpContentRestrictionSet& restriction_set) override;
  void OnWebContentsDestroyed(content::WebContents* web_contents) override;

  // BrowserListObserver overrides:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // TabStripModelObserver overrides:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // Called when tab was probably moved, but without change of the visibility.
  virtual void TabLocationMaybeChanged(content::WebContents* web_contents) = 0;

  // Helper to remove |web_contents| from the confidential set.
  virtual void RemoveFromConfidential(content::WebContents* web_contents);

  // Returns which level and url of printing restriction is currently enforced
  // for |web_contents|.
  RestrictionLevelAndUrl GetPrintingRestrictionInfo(
      content::WebContents* web_contents,
      content::GlobalRenderFrameHostId rfh_id) const;

  // Returns confidential info for screen share of a single |web_contents|.
  ConfidentialContentsInfo GetScreenShareConfidentialContentsInfoForWebContents(
      content::WebContents* web_contents) const;

  // Applies retrieved restrictions in |info| to screens share attempt from
  // app |application_title| and calls the |callback| with a result.
  void ProcessScreenShareRestriction(const std::u16string& application_title,
                                     ConfidentialContentsInfo info,
                                     WarningCallback callback);

  // Returns which level, url, and information about visible confidential
  // contents of screen share restriction that is currently enforced for
  // |media_id|. |web_contents| is not null for tab shares.
  virtual ConfidentialContentsInfo GetScreenShareConfidentialContentsInfo(
      const content::DesktopMediaID& media_id,
      content::WebContents* web_contents) const = 0;

  // If a screen share with the same |label| already exists in
  // |running_screen_shares_|, updates the existing object. Otherwise adds a new
  // screen share to be tracked in |running_screen_shares_|. Callbacks are used
  // to control the screen share state in case it should be paused, resumed or
  // completely stopped by DLP.
  void AddOrUpdateScreenShare(
      const std::string& label,
      const content::DesktopMediaID& media_id,
      const std::u16string& application_title,
      base::RepeatingClosure stop_callback,
      content::MediaStreamUI::StateChangeCallback state_change_callback,
      content::MediaStreamUI::SourceCallback source_callback);

  // Removes screen share from |running_screen_shares_|.
  void RemoveScreenShare(const std::string& label,
                         const content::DesktopMediaID& media_id);

  // Checks and stops the running screen shares if restricted content appeared
  // in the corresponding areas.
  void CheckRunningScreenShares();

  // Resumes the |screen_share| after a delay if it's still necessary.
  void MaybeResumeScreenShare(base::WeakPtr<ScreenShareInfo> screen_share);

  // Called back from Screen Share warning dialogs that are shown during the
  // screen share. Passes along the user's response, reflected in the value of
  // |should_proceed| along to |callback| which handles continuing or cancelling
  // the action based on this response. In case that |should_proceed| is true,
  // also saves the |confidential_contents| that were allowed to be shared by
  // the user to avoid future warnings.
  void OnDlpScreenShareWarnDialogReply(
      const ConfidentialContentsInfo& info,
      base::WeakPtr<ScreenShareInfo> screen_share,
      bool should_proceed);

  // Called back from warning dialogs. Passes along the user's response,
  // reflected in the value of |should_proceed| along to |callback| which
  // handles continuing or cancelling the action based on this response. In case
  // that |should_proceed| is true, also saves the |confidential_contents| that
  // were allowed by the user for |restriction| to avoid future warnings.
  void OnDlpWarnDialogReply(
      const DlpConfidentialContents& confidential_contents,
      DlpRulesManager::Restriction restriction,
      WarningCallback callback,
      bool should_proceed);

  // Reports events if required by the |restriction_info| and
  // `reporting_manager` is configured.
  void MaybeReportEvent(const RestrictionLevelAndUrl& restriction_info,
                        DlpRulesManager::Restriction restriction);

  // Reports warning events if `reporting_manager` is configured.
  void ReportWarningEvent(const GURL& url,
                          DlpRulesManager::Restriction restriction);

  // Removes all elemxents of |contents| that the user has recently already
  // acknowledged the warning for.
  void RemoveAllowedContents(DlpConfidentialContents& contents,
                             DlpRulesManager::Restriction restriction);

  // Updates confidentiality for |web_contents| with the |restriction_set|.
  void UpdateConfidentiality(content::WebContents* web_contents,
                             const DlpContentRestrictionSet& restriction_set);

  // Notifies observers if the restrictions they are listening to changed.
  void NotifyOnConfidentialityChanged(
      const DlpContentRestrictionSet& old_restriction_set,
      const DlpContentRestrictionSet& new_restriction_set,
      content::WebContents* web_contents);

  // Map from currently known confidential WebContents to the restrictions.
  base::flat_map<content::WebContents*, DlpContentRestrictionSet>
      confidential_web_contents_;

  // Keeps track of the contents for which the user allowed the action after
  // being shown a warning for each type of restriction.
  // TODO(crbug.com/1264803): Change to DlpConfidentialContentsCache
  DlpConfidentialContentsCache user_allowed_contents_cache_;

  // List of the currently running screen shares.
  std::vector<std::unique_ptr<ScreenShareInfo>> running_screen_shares_;

  raw_ptr<data_controls::DlpReportingManager, DanglingUntriaged>
      reporting_manager_{nullptr};

  std::unique_ptr<DlpWarnNotifier> warn_notifier_;

  // One ObserverList per restriction.
  std::array<base::ObserverList<DlpContentManagerObserver>,
             static_cast<int>(DlpContentRestriction::kMaxValue) + 1>
      observer_lists_;

  // A helper structure that contains web contents which were reported during
  // the current screen share.
  // Navigating a tab or switching a tab with share-this-tab-instead does not
  // invalidate this contents.
  struct LastReportedScreenShare {
    // Checks if DLP should report for |label| and |confidential_contents|. If
    // yes, then updates internal structures. Does not emit any reporting event.
    bool ShouldReportAndUpdate(
        const std::string& label,
        const DlpConfidentialContents& confidential_contents);

   private:
    std::string label_;
    DlpConfidentialContents confidential_contents_;
  } last_reported_screen_share_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_H_
