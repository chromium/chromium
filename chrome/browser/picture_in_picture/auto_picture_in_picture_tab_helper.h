// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_TAB_HELPER_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_TAB_HELPER_H_

#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/picture_in_picture/auto_picture_in_picture_safe_browsing_checker_client.h"
#include "chrome/browser/picture_in_picture/auto_pip_setting_helper.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "media/base/picture_in_picture_events_info.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/views/bubble/bubble_border.h"

namespace permissions {
class PermissionDecisionAutoBlockerBase;
}  // namespace permissions

class AutoPictureInPictureTabStripObserverHelper;
class AutoPipSettingOverlayView;
class HostContentSettingsMap;
class MediaEngagementService;

// The AutoPictureInPictureTabHelper is a TabHelper attached to each WebContents
// that facilitates automatically opening and closing picture-in-picture windows
// as the given WebContents becomes hidden or visible. WebContents are only
// eligible for auto picture-in-picture if ALL of the following are true:
//   - The website has registered a MediaSession action handler for the
//     'enterpictureinpicture' action.
//   - The 'Auto Picture-in-Picture' content setting is allowed for the website.
//   - The website is capturing camera or microphone.
class AutoPictureInPictureTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<AutoPictureInPictureTabHelper>,
      public media_session::mojom::AudioFocusObserver,
      public media_session::mojom::MediaSessionObserver {
 public:
  // Delay used by `AutoPictureInPictureSafeBrowsingCheckerClient` to check
  // URL safety. If a check takes longer than `kSafeBrowsingCheckDelay`, the URL
  // will be considered not safe and enter AutoPiP requests will be denied.
  static constexpr base::TimeDelta kSafeBrowsingCheckDelay =
      base::Milliseconds(500);

  ~AutoPictureInPictureTabHelper() override;
  AutoPictureInPictureTabHelper(const AutoPictureInPictureTabHelper&) = delete;
  AutoPictureInPictureTabHelper& operator=(
      const AutoPictureInPictureTabHelper&) = delete;

  // True if the current page has registered for auto picture-in-picture since
  // last navigation. Remains true even if the page unregisters for auto
  // picture-in-picture. It only resets on navigation.
  bool HasAutoPictureInPictureBeenRegistered() const;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void MediaPictureInPictureChanged(bool is_in_picture_in_picture) override;
  void MediaSessionCreated(content::MediaSession* media_session) override;

  // Called by `tab_strip_observer_helper_` when the tab changes between
  // activated and unactivated.
  void OnTabActivatedChanged(bool is_tab_activated);

  // media_session::mojom::AudioFocusObserver:
  void OnFocusGained(
      media_session::mojom::AudioFocusRequestStatePtr session) override;
  void OnFocusLost(
      media_session::mojom::AudioFocusRequestStatePtr session) override;
  void OnRequestIdReleased(const base::UnguessableToken& request_id) override {}

  // media_session::mojom::MediaSessionObserver:
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr session_info) override;
  void MediaSessionMetadataChanged(
      const std::optional<media_session::MediaMetadata>& metadata) override {}
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& action)
      override;
  void MediaSessionImagesChanged(
      const base::flat_map<media_session::mojom::MediaSessionImageType,
                           std::vector<media_session::MediaImage>>& images)
      override {}
  void MediaSessionPositionChanged(
      const std::optional<media_session::MediaPosition>& position) override {}

  // Returns true if the tab is in PiP mode, and PiP was started by auto-pip.
  bool IsInAutoPictureInPicture() const;

  void set_is_in_auto_picture_in_picture_for_testing(bool auto_pip) {
    is_in_auto_picture_in_picture_ = auto_pip;
  }

  // Returns true if a PiP window would be considered auto-pip if it opened.
  // This is useful during PiP window startup, when we might not be technically
  // in PiP yet.  `IsInAutoPictureInPicture()` requires that we're in PiP mode
  // to return true.
  //
  // This differs from `IsEligibleForAutoPictureInPicture()` in that this
  // measures if a pip window would qualify for pip, which requires that we've
  // actually detected that the site `IsEligible`, the page has been obscured,
  // and we've sent a pip media session action.  In contrast, `IsEligible` only
  // makes sure that, if the tab were to be obscured, we would send the pip
  // action at that point.
  //
  // Note that this will be false once auto-pip opens.  Opening the window
  // clears the preconditions until the next time they're met.
  bool AreAutoPictureInPicturePreconditionsMet() const;

  void set_auto_blocker_for_testing(
      permissions::PermissionDecisionAutoBlockerBase* auto_blocker) {
    auto_blocker_ = auto_blocker;
    // If we're clearing the auto blocker, then also drop any setting helper we
    // have, since it might also know about it.  This is intended during test
    // cleanup to prevent dangling raw ptrs.
    if (auto_pip_setting_helper_ && !auto_blocker) {
      auto_pip_setting_helper_.reset();
    }
  }

  // Create and return the allow/block overlay view if we should show it for
  // this pip window.  May be called multiple times per pip window instance,
  // since the pip window can be destroyed and recreated by theme changes and
  // other things.  Will return null if there's no need to show the setting UI.
  //
  // `close_pip_cb` should be a callback to close the pip window, in case it
  // should be blocked.  This may be called before this returned, or later, or
  // never.  The other parameters are described in AutoPipSettingHelper.
  std::unique_ptr<AutoPipSettingOverlayView>
  CreateOverlayPermissionViewIfNeeded(
      base::OnceClosure close_pip_cb,
      views::View* anchor_view,
      views::BubbleBorder::Arrow arrow);

  // Should be called when the user closes the pip window manually, so that we
  // can keep the auto-pip setting embargo up to date.
  void OnUserClosedWindow();

  // Notification that our tab became active.  This is our signal to close up
  // any auto-pip window we have open, though there might also not be one.
  void OnTabBecameActive();

  void set_clock_for_testing(const base::TickClock* testing_clock) {
    clock_ = testing_clock;
  }

  void set_auto_pip_trigger_reason_for_testing(
      media::PictureInPictureEventsInfo::AutoPipReason
          auto_pip_trigger_reason) {
    auto_pip_trigger_reason_ = auto_pip_trigger_reason;
  }

  media::PictureInPictureEventsInfo::AutoPipReason GetAutoPipTriggerReason()
      const;

  // Returns information related to auto picture in picture. This information
  // includes the reason for entering picture in picture automatically, if
  // known, and various conditions that are used to allow/deny autopip requests.
  media::PictureInPictureEventsInfo::AutoPipInfo GetAutoPipInfo() const;

 private:
  explicit AutoPictureInPictureTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<AutoPictureInPictureTabHelper>;
  FRIEND_TEST_ALL_PREFIXES(AutoPictureInPictureTabHelperBrowserTest,
                           CannotAutopipViaHttp);
  FRIEND_TEST_ALL_PREFIXES(AutoPictureInPictureTabHelperBrowserTest,
                           PromptResultNotRecorded);
  FRIEND_TEST_ALL_PREFIXES(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                           DoesNotDocumentAutopip_VideoInRemoteIFrame);

  void MaybeEnterAutoPictureInPicture();

  // If needed, schedules the asynchronous task to get the URL safety. When
  // async tasks complete there may be a call to
  // `MaybeEnterAutoPictureInPicture`. This method can safely be called multiple
  // times.
  void MaybeScheduleAsyncTasks();

  // Stops any pending URL safety task. Also reset relevant member variables:
  //   * Sets `has_safe_url_` to false.
  //   * Resets `safe_browsing_checker_client_`.
  //   * Invalidates async tasks weak ptr factory.
  void StopAndResetAsyncTasks();

  void MaybeExitAutoPictureInPicture();

  void MaybeStartOrStopObservingTabStrip();

  bool IsEligibleForAutoPictureInPicture(bool should_record_blocking_metrics);

  // Returns true if the tab:
  //   * Has audio focus
  //   * Is playing unmuted playback
  //   * Has a safe URL as reported by
  //   `AutoPictureInPictureSafeBrowsingCheckerClient`
  bool MeetsVideoPlaybackConditions() const;

  // Returns true if the tab is currently using the camera or microphone.
  bool IsUsingCameraOrMicrophone() const;

  // Returns true if the tab is currently audible, or was audible
  // recently.
  bool WasRecentlyAudible() const;

  // Returns true if the tab has high media engagement or content setting is set
  // to `CONTENT_SETTING_ALLOW`, false otherwise.
  //
  // Among other cases, this method will also return false if the media session
  // routed frame either does not exist or is not in the primary main frame.
  bool MeetsMediaEngagementConditions() const;

  // Returns the current state of the 'Auto Picture-in-Picture' content
  // setting for the current website of the observed WebContents.
  ContentSetting GetCurrentContentSetting() const;

  // Called when the result of checking URL safety is known.
  // `MaybeEnterAutoPictureInPicture` will be called if the URL is safe.
  void OnUrlSafetyResult(bool has_safe_url);

  // Schedules a URL safety check. Before scheduling a URL safety check,
  // initializes the `safe_browsing_checker_client_` if needed.
  void ScheduleUrlSafetyCheck();

  // Creates the `auto_pip_setting_helper_` if it does not already exist.
  void EnsureAutoPipSettingHelper();

  // Returns the primary main routed frame for the MediaSession, if it exists.
  // Otherwise, the primary main frame for the WebContent. If both do not exist,
  // an empty optional is returned.
  //
  // This method retrieves the routed frame associated with the WebContents's
  // MediaSession. If a routed frame is found and it resides within the primary
  // main frame, an optional containing a pointer to the RenderFrameHost is
  // returned.
  //
  // If there is no MediaSession routed frame, an optional containing a pointer
  // to the WebContent primary main frame is returned. For cases where both, the
  // MediaSession routed frame and the WebContent, primary main frames do not
  // exist an empty optional is returned.
  std::optional<content::RenderFrameHost*> GetPrimaryMainRoutedFrame() const;

  // Returns the page UKM SourceId associated with the primary main routed frame
  // for the MediaSession, if it exists.
  std::optional<ukm::SourceId> GetUkmSourceId() const;

  // Returns the reason for entering auto picture in picture.
  //
  // Note that a media element can meet both, the "video conferencing" and
  // "media playback" conditions. If both conditions are met, this method will
  // return
  // "media::PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing". If
  // no conditions are met,
  // `Autmedia::PictureInPictureEventsInfo::AutoPipReasonPipReason::kUnknown`
  // will be returned.
  media::PictureInPictureEventsInfo::AutoPipReason GetAutoPipReason() const;

  // Accumulates the total time spent in picture in picture during the lifetime
  // of `this`, separated by the reason for entering auto picture in picture:
  // video conferencing or media playback.
  //
  // If `is_video_conferencing` is true, `total_pip_time` will be accumulated
  // for video conferencing, otherwise the time will be accumulated for media
  // playback.
  void AccumulateTotalPipTimeForSession(const base::TimeDelta total_pip_time,
                                        bool is_video_conferencing);

  // Records the total time spent on a picture in picture window, regardless of
  // the Picture-in-Picture window type (document vs video) and the reason for
  // closing the window (UI interaction, returning back to opener tab, etc.).
  //
  // The resulting histogram is configured to allow analyzing closures that take
  // place within a short period of time, to account for user reaction time
  // (~273 ms).
  void MaybeRecordPictureInPictureChanged(bool is_picture_in_picture);

  // Records, if needed, the total accumulated picture in picture time,
  // separated by the reason for entering auto picture in picture: video
  // conferencing or media playback. This metric is recorded during the tab
  // helper destruction.
  void MaybeRecordTotalPipTimeForSession();

  // HostContentSettingsMap is tied to the Profile which outlives the
  // WebContents (which we're tied to), so this is safe.
  const raw_ptr<HostContentSettingsMap> host_content_settings_map_;

  // Embargo checker, if enabled.  May be null.
  raw_ptr<permissions::PermissionDecisionAutoBlockerBase> auto_blocker_ =
      nullptr;

  // Notifies us when our tab either becomes the active tab on its tabstrip or
  // becomes an inactive tab on its tabstrip.
  std::unique_ptr<AutoPictureInPictureTabStripObserverHelper>
      tab_strip_observer_helper_;

  // True if the tab is the activated tab on its tab strip.
  bool is_tab_activated_ = false;

  // True if the media session associated with the observed WebContents has
  // gained audio focus.
  bool has_audio_focus_ = false;

  // True if the media session associated with the observed WebContents is
  // currently playing.
  bool is_playing_ = false;

  // True if the observed WebContents is currently in picture-in-picture.
  bool is_in_picture_in_picture_ = false;

  // True if the observed WebContents is currently in picture-in-picture due
  // to autopip.
  bool is_in_auto_picture_in_picture_ = false;

  // This is used to determine whether the website has used an
  // EnterAutoPictureInPicture action handler to open a picture-in-picture
  // window. When we send the message, we set this time to be the length of a
  // user activation, and if the WebContents enters picture-in-picture before
  // that time, then we will assume we have entered auto-picture-in-picture
  // (and are therefore eligible to exit auto-picture-in-picture when the tab
  // becomes visible again).
  base::TimeTicks auto_picture_in_picture_activation_time_;

  // True if the 'EnterAutoPictureInPicture' action is available on the media
  // session.
  bool is_enter_auto_picture_in_picture_available_ = false;

  // True if the current page has registered for auto picture-in-picture since
  // last navigation. Remains true even if the page unregisters for auto
  // picture-in-picture. It only resets on navigation.
  bool has_ever_registered_for_auto_picture_in_picture_ = false;

  // TODO(crbug.com/40250017): Reword to reference the "MediaSession routed
  // frame last committed URL".
  //
  // True if the observed WebContents last committed URL is safe, as reported by
  // `AutoPictureInPictureSafeBrowsingCheckerClient`.
  bool has_safe_url_ = false;

  // Connections with the media session service to listen for audio focus
  // updates and control media sessions.
  mojo::Receiver<media_session::mojom::AudioFocusObserver>
      audio_focus_observer_receiver_{this};
  mojo::Receiver<media_session::mojom::MediaSessionObserver>
      media_session_observer_receiver_{this};

  // If non-null, this is the setting helper for the permission setting UI.
  std::unique_ptr<AutoPipSettingHelper> auto_pip_setting_helper_;

  // Implementation of the Safe Browsing client, used to check and report URL
  // safety.
  std::unique_ptr<AutoPictureInPictureSafeBrowsingCheckerClient>
      safe_browsing_checker_client_;

  // The `MediaEngagementService` is used by `this` to determine whether or not
  // the web contents origin has high media engagement.
  //
  // This is safe since the `MediaEngagementService` is tied to the Profile
  // which outlives the WebContents (which `this` is tied to).
  raw_ptr<MediaEngagementService> media_engagement_service_ = nullptr;

  // Set to the current time when `this` calls the MediaSession
  // `EnterAutoPictureInPicture` method.
  std::optional<base::TimeTicks> current_enter_pip_time_ = std::nullopt;

  // The total accumulated time spent in picture in picture due to video
  // conferencing. The accumulated time does not differentiate between the
  // different types of picture in picture windows (document vs video).
  // Accumulated time is recorded during the destruction of `this`.
  std::optional<base::TimeDelta>
      total_video_conferencing_pip_time_for_session_ = std::nullopt;

  // The total accumulated time spent in picture in picture due to media
  // playback. The accumulated time does not differentiate between the different
  // types of picture in picture windows (document vs video). Accumulated time
  // is recorded during the destruction of `this`.
  std::optional<base::TimeDelta> total_media_playback_pip_time_for_session_ =
      std::nullopt;

  // Clock used for metric related to the total time spent with a
  // picture-in-picture window open.
  raw_ptr<const base::TickClock> clock_;

  // Stores the reason that triggered auto picture in picture. The value is
  // updated as needed when entering/exiting picture in picture.
  media::PictureInPictureEventsInfo::AutoPipReason auto_pip_trigger_reason_ =
      media::PictureInPictureEventsInfo::AutoPipReason::kUnknown;

  // Set to true if auto picture in picture was blocked due to content setting
  // or incognito, false otherwise. The value is used to prevent recording
  // duplicate entries for blocking metrics.
  bool blocked_due_to_content_setting_ = false;

  // WeakPtrFactory used only for requesting URL safety. This weak ptr factory
  // is invalidated during calls to `StopAndResetAsyncTasks`.
  base::WeakPtrFactory<AutoPictureInPictureTabHelper> async_tasks_weak_factory_{
      this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_TAB_HELPER_H_
