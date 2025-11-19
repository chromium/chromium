// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_helper.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/media_engagement_service.h"  // nogncheck crbug.com/422038808
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_observer_helper_base.h"
#include "chrome/browser/picture_in_picture/auto_pip_setting_helper.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/media_session_service.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/frame/user_activation_state.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/picture_in_picture/auto_pip_setting_overlay_view.h"
#endif

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#endif

AutoPictureInPictureTabHelper::AutoPictureInPictureTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<AutoPictureInPictureTabHelper>(
          *web_contents),
      host_content_settings_map_(HostContentSettingsMapFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))),
      auto_blocker_(PermissionDecisionAutoBlockerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))),
      media_engagement_service_(MediaEngagementService::Get(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))),
      clock_(base::DefaultTickClock::GetInstance()) {
  // `base::Unretained` is safe here since we own `tab_observer_helper_`.
  tab_observer_helper_ = AutoPictureInPictureTabObserverHelperBase::Create(
      web_contents,
      base::BindRepeating(&AutoPictureInPictureTabHelper::OnTabActivatedChanged,
                          base::Unretained(this)));

  // On non-Android platforms, we observe the internal AudioFocusManager to
  // track audio focus state. Android has a native system-wide AudioManager,
  // so this observer is never notified and is not used.
#if !BUILDFLAG(IS_ANDROID)
  // Connect to receive audio focus events.
  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_remote;
  content::GetMediaSessionService().BindAudioFocusManager(
      audio_focus_remote.BindNewPipeAndPassReceiver());
  audio_focus_remote->AddObserver(
      audio_focus_observer_receiver_.BindNewPipeAndPassRemote());
#endif  // !BUILDFLAG(IS_ANDROID)

  // Connect to receive media session updates if the media session already
  // exists. If it does not, then we'll become an observer in
  // `MediaSessionCreated()`.
  content::MediaSession* media_session =
      content::MediaSession::GetIfExists(web_contents);
  if (media_session) {
    media_session->AddObserver(
        media_session_observer_receiver_.BindNewPipeAndPassRemote());
  }
}

AutoPictureInPictureTabHelper::~AutoPictureInPictureTabHelper() {
  MaybeRecordTotalPipTimeForSession();
  StopAndResetAsyncTasks();
}

bool AutoPictureInPictureTabHelper::HasAutoPictureInPictureBeenRegistered()
    const {
  return has_ever_registered_for_auto_picture_in_picture_;
}

void AutoPictureInPictureTabHelper::PrimaryPageChanged(content::Page& page) {
  has_ever_registered_for_auto_picture_in_picture_ = false;
  // On navigation, forget any 'allow once' state.
#if !BUILDFLAG(IS_ANDROID)
  auto_pip_setting_helper_.reset();
#endif  // !BUILDFLAG(IS_ANDROID)

  StopAndResetAsyncTasks();

#if BUILDFLAG(IS_ANDROID)
  hide_button_clicked_time_ = std::nullopt;
#endif  // BUILDFLAG(IS_ANDROID)
}

void AutoPictureInPictureTabHelper::AccumulateTotalPipTimeForSession(
    const base::TimeDelta total_pip_time,
    media::PictureInPictureEventsInfo::AutoPipReason reason) {
  switch (reason) {
    case media::PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing:
      if (!total_video_conferencing_pip_time_for_session_) {
        total_video_conferencing_pip_time_for_session_ = total_pip_time;
      } else {
        total_video_conferencing_pip_time_for_session_.value() +=
            total_pip_time;
      }
      break;
    case media::PictureInPictureEventsInfo::AutoPipReason::kMediaPlayback:
      if (!total_media_playback_pip_time_for_session_) {
        total_media_playback_pip_time_for_session_ = total_pip_time;
      } else {
        total_media_playback_pip_time_for_session_.value() += total_pip_time;
      }
      break;
    case media::PictureInPictureEventsInfo::AutoPipReason::kBrowserInitiated:
      if (!total_browser_initiated_pip_time_for_session_) {
        total_browser_initiated_pip_time_for_session_ = total_pip_time;
      } else {
        total_browser_initiated_pip_time_for_session_.value() += total_pip_time;
      }
      break;
    case media::PictureInPictureEventsInfo::AutoPipReason::kUnknown:
      break;
  }
}

void AutoPictureInPictureTabHelper::MaybeRecordPictureInPictureChanged(
    bool is_picture_in_picture) {
  if (is_picture_in_picture) {
    current_enter_pip_time_ = clock_->NowTicks();
    return;
  }

  if (!current_enter_pip_time_) {
    return;
  }

  base::TimeDelta total_pip_time =
      clock_->NowTicks() - current_enter_pip_time_.value();
  current_enter_pip_time_ = std::nullopt;

  // Calculate total playback time for the duration of the pip window.
  std::optional<base::TimeDelta> total_playback_time = std::nullopt;
  if (current_pip_playback_time_) {
    // Start with the existing recorded PiP playback time.
    total_playback_time = current_pip_playback_time_.value();

    if (playing_start_time_) {
      // Add additional time elapsed since the video started playing in PiP.
      total_playback_time.value() +=
          clock_->NowTicks() - playing_start_time_.value();
    }
  }
  // Reset playback time related fields.
  playing_start_time_ = std::nullopt;
  current_pip_playback_time_ = std::nullopt;

  if (auto_pip_trigger_reason_ ==
      media::PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing) {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReason."
        "VideoConferencing.TotalTime",
        total_pip_time, base::Milliseconds(1), base::Minutes(2), 50);
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReason."
        "VideoConferencing.TotalTimeV2",
        total_pip_time, base::Milliseconds(1), base::Hours(10), 100);
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReasonV2."
        "VideoConferencing.TotalTime",
        total_pip_time, base::Milliseconds(1), base::Minutes(2), 50);
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReasonV2."
        "VideoConferencing.TotalTimeV2",
        total_pip_time, base::Milliseconds(1), base::Hours(10), 100);
  } else if (auto_pip_trigger_reason_ ==
             media::PictureInPictureEventsInfo::AutoPipReason::kMediaPlayback) {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReason."
        "MediaPlayback.TotalTime",
        total_pip_time, base::Milliseconds(1), base::Minutes(2), 50);
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReason."
        "MediaPlayback.TotalTimeV2",
        total_pip_time, base::Milliseconds(1), base::Hours(10), 100);
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReasonV2."
        "MediaPlayback.TotalTime",
        total_pip_time, base::Milliseconds(1), base::Minutes(2), 50);
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReasonV2."
        "MediaPlayback.TotalTimeV2",
        total_pip_time, base::Milliseconds(1), base::Hours(10), 100);
    if (total_playback_time) {
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReasonV2."
          "MediaPlayback.TotalPlaybackTime",
          total_playback_time.value(), base::Milliseconds(1), base::Hours(10),
          100);

      if (total_pip_time.is_positive()) {
        base::UmaHistogramPercentage(
            "Media.AutoPictureInPicture.EnterPictureInPicture."
            "AutomaticReasonV2."
            "MediaPlayback.PlaybackToTotalTimeRatio",
            100 * (total_playback_time.value() / total_pip_time));
      }
    }
  } else if (auto_pip_trigger_reason_ == media::PictureInPictureEventsInfo::
                                             AutoPipReason::kBrowserInitiated) {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReasonV2."
        "BrowserInitiated.TotalTime",
        total_pip_time, base::Milliseconds(1), base::Minutes(2), 50);
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReasonV2."
        "BrowserInitiated.TotalTimeV2",
        total_pip_time, base::Milliseconds(1), base::Hours(10), 100);
  }

  AccumulateTotalPipTimeForSession(total_pip_time, auto_pip_trigger_reason_);
}

void AutoPictureInPictureTabHelper::MaybeRecordTotalPipTimeForSession() {
  if (!total_video_conferencing_pip_time_for_session_ &&
      !total_media_playback_pip_time_for_session_ &&
      !total_browser_initiated_pip_time_for_session_) {
    return;
  }

  if (total_video_conferencing_pip_time_for_session_) {
    base::UmaHistogramCustomTimes(
        "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReason."
        "VideoConferencing.TotalTimeForSession",
        total_video_conferencing_pip_time_for_session_.value(),
        base::Milliseconds(1), base::Minutes(2), 50);
    base::UmaHistogramCustomTimes(
        "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReason."
        "VideoConferencing.TotalTimeForSessionV2",
        total_video_conferencing_pip_time_for_session_.value(),
        base::Milliseconds(1), base::Hours(10), 100);
    base::UmaHistogramCustomTimes(
        "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReasonV2."
        "VideoConferencing.TotalTimeForSession",
        total_video_conferencing_pip_time_for_session_.value(),
        base::Milliseconds(1), base::Minutes(2), 50);
    base::UmaHistogramCustomTimes(
        "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReasonV2."
        "VideoConferencing.TotalTimeForSessionV2",
        total_video_conferencing_pip_time_for_session_.value(),
        base::Milliseconds(1), base::Hours(10), 100);
  }

  if (total_media_playback_pip_time_for_session_) {
    base::UmaHistogramCustomTimes(
        "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReason."
        "MediaPlayback.TotalTimeForSession",
        total_media_playback_pip_time_for_session_.value(),
        base::Milliseconds(1), base::Minutes(2), 50);
    base::UmaHistogramCustomTimes(
        "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReason."
        "MediaPlayback.TotalTimeForSessionV2",
        total_media_playback_pip_time_for_session_.value(),
        base::Milliseconds(1), base::Hours(10), 100);
    base::UmaHistogramCustomTimes(
        "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReasonV2."
        "MediaPlayback.TotalTimeForSession",
        total_media_playback_pip_time_for_session_.value(),
        base::Milliseconds(1), base::Minutes(2), 50);
    base::UmaHistogramCustomTimes(
        "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReasonV2."
        "MediaPlayback.TotalTimeForSessionV2",
        total_media_playback_pip_time_for_session_.value(),
        base::Milliseconds(1), base::Hours(10), 100);
  }

  if (total_browser_initiated_pip_time_for_session_) {
    base::UmaHistogramCustomTimes(
        "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReasonV2."
        "BrowserInitiated.TotalTimeForSession",
        total_browser_initiated_pip_time_for_session_.value(),
        base::Milliseconds(1), base::Minutes(2), 50);
    base::UmaHistogramCustomTimes(
        "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReasonV2."
        "BrowserInitiated.TotalTimeForSessionV2",
        total_browser_initiated_pip_time_for_session_.value(),
        base::Milliseconds(1), base::Hours(10), 100);
  }

  total_video_conferencing_pip_time_for_session_ = std::nullopt;
  total_media_playback_pip_time_for_session_ = std::nullopt;
  total_browser_initiated_pip_time_for_session_ = std::nullopt;
}

void AutoPictureInPictureTabHelper::MediaPictureInPictureChanged(
    bool is_in_picture_in_picture) {
  if (is_in_picture_in_picture_ == is_in_picture_in_picture) {
    return;
  }
  is_in_picture_in_picture_ = is_in_picture_in_picture;
  blocked_due_to_content_setting_ = false;

  if (!is_in_picture_in_picture_) {
    is_in_auto_picture_in_picture_ = false;
    MaybeRecordPictureInPictureChanged(false);
    MaybeStartOrStopObservingTabStrip();
    auto_pip_trigger_reason_ =
        media::PictureInPictureEventsInfo::AutoPipReason::kUnknown;
    return;
  }

  if (AreAutoPictureInPicturePreconditionsMet()) {
    current_pip_playback_time_ = base::TimeDelta();
    is_in_auto_picture_in_picture_ = true;
    auto_picture_in_picture_activation_time_ = base::TimeTicks();
    MaybeRecordPictureInPictureChanged(true);

    // If the tab is activated by the time auto picture-in-picture fires, we
    // should immediately close the auto picture-in-picture.
    if (is_tab_activated_) {
      MaybeExitAutoPictureInPicture();
    } else if (is_playing_) {
      // Media is playing, start the watch time timer.
      playing_start_time_ = clock_->NowTicks();
    }
  }
}

void AutoPictureInPictureTabHelper::MediaSessionCreated(
    content::MediaSession* media_session) {
  // Connect to receive media session updates.
  media_session->AddObserver(
      media_session_observer_receiver_.BindNewPipeAndPassRemote());
}

void AutoPictureInPictureTabHelper::OnTabActivatedChanged(
    bool is_tab_activated) {
  is_tab_activated_ = is_tab_activated;
  if (is_tab_activated_) {
    OnTabBecameActive();
  } else {
    auto* active_contents = tab_observer_helper_->GetActiveWebContents();
    if (auto* active_tab_helper =
            active_contents ? FromWebContents(active_contents) : nullptr) {
      // There is a tab helper that's associated with the newly active contents.
      // Since it's unclear whether we find out about the activation change
      // before it does, notify it now.  This gives it the opportunity to close
      // any auto-pip window it has before we try to autopip and find that
      // there's a pip window already.  It's also possible that there is no pip
      // window, or that it's not associated with the active tab, which is also
      // fine.  Whatever the pip state is after this, we'll just believe it.
      active_tab_helper->OnTabBecameActive();
    }

    MaybeEnterAutoPictureInPicture();
    MaybeScheduleAsyncTasks();
  }
}

#if !BUILDFLAG(IS_ANDROID)
void AutoPictureInPictureTabHelper::OnFocusGained(
    media_session::mojom::AudioFocusRequestStatePtr session) {
  if (has_audio_focus_) {
    return;
  }
  auto request_id =
      content::MediaSession::GetRequestIdFromWebContents(web_contents());
  if (request_id.is_empty()) {
    return;
  }
  has_audio_focus_ = (request_id == session->request_id);
}

void AutoPictureInPictureTabHelper::OnFocusLost(
    media_session::mojom::AudioFocusRequestStatePtr session) {
  if (!has_audio_focus_) {
    return;
  }
  auto request_id =
      content::MediaSession::GetRequestIdFromWebContents(web_contents());
  if (request_id.is_empty()) {
    // I don't think this can happen, but if we reach here without a request ID,
    // we can safely assume we no longer have focus.
    has_audio_focus_ = false;
    return;
  }
  has_audio_focus_ = (request_id != session->request_id);
}
#endif  // !BUILDFLAG(IS_ANDROID)

void AutoPictureInPictureTabHelper::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  // On Android, audio focus is managed by the operating system. The
  // MediaSession state is the source of truth as it reflects focus changes from
  // the Android system's AudioManager.
#if BUILDFLAG(IS_ANDROID)
  has_audio_focus_ =
      session_info &&
      session_info->state ==
          media_session::mojom::MediaSessionInfo::SessionState::kActive;
#endif  // BUILDFLAG(IS_ANDROID)
  const bool is_playing =
      session_info && session_info->playback_state ==
                          media_session::mojom::MediaPlaybackState::kPlaying;

  // If the playing state hasn't changed, nothing more to do.
  if (is_playing_ == is_playing) {
    return;
  }
  is_playing_ = is_playing;

  // Watch time tracking is only relevant if we are currently in Auto PiP.
  if (!is_in_auto_picture_in_picture_) {
    return;
  }

  if (is_playing_) {
    // Media started playing: record the start time.
    playing_start_time_ = clock_->NowTicks();
  } else if (playing_start_time_) {
    // Media paused/stopped: calculate and accumulate watch time.
    current_pip_playback_time_.value() +=
        clock_->NowTicks() - playing_start_time_.value();
    playing_start_time_ = std::nullopt;
  }
}

void AutoPictureInPictureTabHelper::MediaSessionActionsChanged(
    const std::vector<media_session::mojom::MediaSessionAction>& actions) {
  bool was_available = is_enter_auto_picture_in_picture_available_;
  is_enter_auto_picture_in_picture_available_ =
      std::ranges::find(actions,
                        media_session::mojom::MediaSessionAction::
                            kEnterAutoPictureInPicture) != actions.end();

  if (is_enter_auto_picture_in_picture_available_ && !was_available) {
    has_ever_registered_for_auto_picture_in_picture_ = true;
    // Inform PageSpecificContentSettings that this page now has auto
    // picture-in-picture support. This will cause the UI to update. This is
    // done so that the UI is updated if the page info menu is already open when
    // the status changes.
    auto* pscs = content_settings::PageSpecificContentSettings::GetForFrame(
        web_contents()->GetPrimaryMainFrame());
    if (pscs) {
      pscs->OnRegisteredForAutoPictureInPictureChanged();
    }
  }
  MaybeStartOrStopObservingTabStrip();
}

void AutoPictureInPictureTabHelper::MaybeEnterAutoPictureInPicture() {
  if (!IsEligibleForAutoPictureInPicture(
          /*should_record_blocking_metrics=*/true)) {
    if (base::FeatureList::IsEnabled(
            media::kAutoPictureInPictureForVideoPlayback) &&
        !IsUsingCameraOrMicrophone() && !has_safe_url_) {
      // This is a media playback case, but we have not checked for URL safety
      // yet. Do not report info changed, as an async check will be triggered
      // which will call this function again.
      return;
    }
    MaybeReportAutoPictureInPictureInfoChanged();
    return;
  }
  auto_picture_in_picture_activation_time_ =
      base::TimeTicks::Now() + blink::kActivationLifespan;
  auto_pip_trigger_reason_ = GetAutoPipReason();
  content::MediaSession::Get(web_contents())->EnterAutoPictureInPicture();
}

void AutoPictureInPictureTabHelper::MaybeScheduleAsyncTasks() {
  if (!base::FeatureList::IsEnabled(
          media::kAutoPictureInPictureForVideoPlayback)) {
    return;
  }

  StopAndResetAsyncTasks();

  // Prevent scheduling asynchronous checks if we are already in picture in
  // picture, picture in picture was blocked due to content setting/incognito,
  // we are using camera or microphone, or a media session does not exist. Also
  // prevent these checks if we are already eligible for auto picture in
  // picture, since auto picture in picture requests will succeed anyways.
  //
  // The `blocked_due_to_content_setting_` check is performed to prevent
  // recording duplicate entries for blocking metrics.
  if (is_in_picture_in_picture_ ||
      !(content::MediaSession::GetIfExists(web_contents())) ||
      blocked_due_to_content_setting_ || IsUsingCameraOrMicrophone() ||
      IsEligibleForAutoPictureInPicture(
          /*should_record_blocking_metrics=*/false)) {
    return;
  }

  ScheduleUrlSafetyCheck();
}

void AutoPictureInPictureTabHelper::MaybeReportAutoPictureInPictureInfoChanged()
    const {
  content::MediaSession* media_session =
      content::MediaSession::GetIfExists(web_contents());
  if (!media_session) {
    return;
  }

  media_session->ReportAutoPictureInPictureInfoChanged();
}

void AutoPictureInPictureTabHelper::StopAndResetAsyncTasks() {
  if (!base::FeatureList::IsEnabled(
          media::kAutoPictureInPictureForVideoPlayback)) {
    return;
  }

  async_tasks_weak_factory_.InvalidateWeakPtrs();
  safe_browsing_checker_client_.reset();

  has_safe_url_ = false;
}

void AutoPictureInPictureTabHelper::MaybeExitAutoPictureInPicture() {
  blocked_due_to_content_setting_ = false;
  MaybeRecordPictureInPictureChanged(false);
  StopAndResetAsyncTasks();
  auto_pip_trigger_reason_ =
      media::PictureInPictureEventsInfo::AutoPipReason::kUnknown;

  if (!is_in_auto_picture_in_picture_) {
    return;
  }
  is_in_auto_picture_in_picture_ = false;

  PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture();
}

void AutoPictureInPictureTabHelper::MaybeStartOrStopObservingTabStrip() {
  if (is_enter_auto_picture_in_picture_available_ ||
      is_in_auto_picture_in_picture_) {
    tab_observer_helper_->StartObserving();
  } else {
    tab_observer_helper_->StopObserving();
  }
}

bool AutoPictureInPictureTabHelper::IsEligibleForAutoPictureInPicture(
    bool should_record_blocking_metrics) {
  // Don't try to autopip if picture-in-picture is currently disabled.
  if (PictureInPictureWindowManager::GetInstance()
          ->IsPictureInPictureDisabled()) {
    return false;
  }

  // Only https:// or file:// may autopip.
  const GURL url = web_contents()->GetLastCommittedURL();
  if (!url.SchemeIs(url::kHttpsScheme) && !url.SchemeIsFile()) {
    return false;
  }

  // The tab must either have playback or be using camera/microphone to autopip.
  if (!MeetsVideoPlaybackConditions() && !IsUsingCameraOrMicrophone()) {
    return false;
  }

  // The website must have registered for autopip.
  if (!is_enter_auto_picture_in_picture_available_) {
    return false;
  }

  // Do not replace any PiP with autopip.  In the special case where the
  // incoming active tab owns a pip window that will close as a result of the
  // tab switch, it should have closed already by now.  Either it received a
  // notification from its tab strip helper, or we notified it, depending on
  // which one of us was notified by our respective tab strip helper.
  if (PictureInPictureWindowManager::GetInstance()->GetWebContents() !=
      nullptr) {
    return false;
  }

  // Since nobody has a pip window, we shouldn't think we do.
  CHECK(!is_in_picture_in_picture_);

  // The user may block autopip via a content setting. Also, if we're in an
  // incognito window, then we should treat "ask" as "block". This should be the
  // final check before triggering autopip since it will record metrics about
  // why autopip has been blocked.
  ContentSetting setting = GetCurrentContentSetting();
  if (setting == CONTENT_SETTING_BLOCK) {
    blocked_due_to_content_setting_ = true;

    if (should_record_blocking_metrics) {
      EnsureAutoPipSettingHelper();
      auto_pip_setting_helper_->OnAutoPipBlockedByPermission(GetAutoPipReason(),
                                                             GetUkmSourceId());
    }
    return false;
  } else if (setting == CONTENT_SETTING_ASK &&
             Profile::FromBrowserContext(web_contents()->GetBrowserContext())
                 ->IsIncognitoProfile()) {
    blocked_due_to_content_setting_ = true;

    if (should_record_blocking_metrics) {
      EnsureAutoPipSettingHelper();
      auto_pip_setting_helper_->OnAutoPipBlockedByIncognito(GetAutoPipReason());
    }
    return false;
  }

  return true;
}

bool AutoPictureInPictureTabHelper::MeetsVideoPlaybackConditions() const {
  if (!base::FeatureList::IsEnabled(
          media::kAutoPictureInPictureForVideoPlayback)) {
    return false;
  }

  return has_audio_focus_ && is_playing_ && WasRecentlyAudible() &&
         has_safe_url_ && MeetsMediaEngagementConditions();
}

bool AutoPictureInPictureTabHelper::IsUsingCameraOrMicrophone() const {
#if BUILDFLAG(IS_ANDROID)
  // For Android JNI tests, return the testing override value if it's available,
  // completely bypassing the IsCapturingUserMedia check.
  if (is_using_camera_or_microphone_for_testing_.has_value()) {
    return is_using_camera_or_microphone_for_testing_.value();
  }
#endif
  return MediaCaptureDevicesDispatcher::GetInstance()
      ->GetMediaStreamCaptureIndicator()
      ->IsCapturingUserMedia(web_contents());
}

bool AutoPictureInPictureTabHelper::WasRecentlyAudible() const {
  auto* audible_helper = RecentlyAudibleHelper::FromWebContents(web_contents());
  if (!audible_helper) {
    return false;
  }

  return audible_helper->WasRecentlyAudible();
}

bool AutoPictureInPictureTabHelper::MeetsMediaEngagementConditions() const {
  // Skip checking media engagement when content setting is set to allow.
  if (GetCurrentContentSetting() == CONTENT_SETTING_ALLOW) {
    return true;
  }

  std::optional<content::RenderFrameHost*> rfh = GetPrimaryMainRoutedFrame();
  if (!rfh) {
    return false;
  }

  const url::Origin origin = rfh.value()->GetLastCommittedOrigin();
  if (origin.GetURL().SchemeIsFile()) {
    return true;
  }

  if (!media_engagement_service_) {
    return false;
  }

#if BUILDFLAG(IS_ANDROID)
  // For Android JNI tests, return the testing override value if it's available,
  // completely bypassing the MediaEngagementService check.
  if (has_high_engagement_for_testing_.has_value()) {
    return has_high_engagement_for_testing_.value();
  }
#endif

  return media_engagement_service_->HasHighEngagement(origin);
}

ContentSetting AutoPictureInPictureTabHelper::GetCurrentContentSetting() const {
  GURL url = web_contents()->GetLastCommittedURL();
  auto setting = host_content_settings_map_->GetContentSetting(
      url, url, ContentSettingsType::AUTO_PICTURE_IN_PICTURE);
  if (setting == CONTENT_SETTING_ASK && auto_blocker_ &&
      auto_blocker_->IsEmbargoed(
          url, ContentSettingsType::AUTO_PICTURE_IN_PICTURE)) {
    return CONTENT_SETTING_BLOCK;
  }
  return setting;
}

void AutoPictureInPictureTabHelper::OnUrlSafetyResult(bool has_safe_url) {
  has_safe_url_ = has_safe_url;

  if (!has_safe_url_) {
    // If URL is not safe, we are not eligible. Report the auto
    // picture-in-picture information change.
    MaybeReportAutoPictureInPictureInfoChanged();
    return;
  }

  MaybeEnterAutoPictureInPicture();
}

void AutoPictureInPictureTabHelper::ScheduleUrlSafetyCheck() {
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  CHECK(!is_in_picture_in_picture_);
  CHECK(g_browser_process);
  CHECK(g_browser_process->safe_browsing_service());

  std::optional<content::RenderFrameHost*> rfh = GetPrimaryMainRoutedFrame();
  if (!rfh) {
    return;
  }

  if (!safe_browsing_checker_client_) {
    // Create the AutoPiP safe browsing checker client, which will be used for
    // determining URL safety.
    safe_browsing_checker_client_ = std::make_unique<
        AutoPictureInPictureSafeBrowsingCheckerClient>(
        g_browser_process->safe_browsing_service()->database_manager().get(),
        kSafeBrowsingCheckDelay,
        base::BindRepeating(&AutoPictureInPictureTabHelper::OnUrlSafetyResult,
                            async_tasks_weak_factory_.GetWeakPtr()));
  }

  safe_browsing_checker_client_->CheckUrlSafety(
      rfh.value()->GetLastCommittedURL());
#else
  OnUrlSafetyResult(/*has_safe_url=*/true);
#endif
}

void AutoPictureInPictureTabHelper::EnsureAutoPipSettingHelper() {
  if (!auto_pip_setting_helper_) {
    auto_pip_setting_helper_ = AutoPipSettingHelper::CreateForWebContents(
        web_contents(), host_content_settings_map_, auto_blocker_);
  }
}

bool AutoPictureInPictureTabHelper::
    CanEnterBrowserInitiatedAutoPictureInPicture() const {
  content::MediaSession* media_session =
      content::MediaSession::GetIfExists(web_contents());
  if (!media_session) {
    return false;
  }

  media_session::mojom::MediaSessionInfoPtr info =
      media_session->GetMediaSessionInfoSync();
  return info && info->can_enter_browser_initiated_autopip;
}

std::optional<content::RenderFrameHost*>
AutoPictureInPictureTabHelper::GetPrimaryMainRoutedFrame() const {
  content::MediaSession* media_session =
      content::MediaSession::GetIfExists(web_contents());
  if (!media_session) {
    return std::nullopt;
  }

  auto* rfh = media_session->GetRoutedFrame();

  // Default to using the WebContents primary main frame for browser initiated
  // auto picture in picture, where the MediaSession routed frame may not exist
  // (a MediaSession routed frame is guaranteed to exist if the user manually
  // registered a MediaSession `enterpictureinpicture` action handler). This is
  // in line with the current requirement of only allowing auto picture in
  // picture from the top frame.
  if (base::FeatureList::IsEnabled(
          blink::features::kBrowserInitiatedAutomaticPictureInPicture) &&
      rfh == nullptr) {
    rfh = web_contents()->GetPrimaryMainFrame();
  }

  if (!rfh || !rfh->IsInPrimaryMainFrame()) {
    return std::nullopt;
  }

  return {rfh};
}

std::optional<ukm::SourceId> AutoPictureInPictureTabHelper::GetUkmSourceId()
    const {
  const std::optional<content::RenderFrameHost*> rfh =
      GetPrimaryMainRoutedFrame();

  if (!rfh) {
    return std::nullopt;
  }

  return {rfh.value()->GetPageUkmSourceId()};
}

media::PictureInPictureEventsInfo::AutoPipReason
AutoPictureInPictureTabHelper::GetAutoPipReason() const {
  if (IsUsingCameraOrMicrophone()) {
    return media::PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing;
  }

  // Note that order matters here since
  // `CanEnterBrowserInitiatedAutoPictureInPicture` and
  // `MeetsVideoPlaybackConditions` can both be true at the same time. The
  // disambiguating condition is that browser initiated
  // automatic-picture-in-picture can only be true when there is no action
  // handler registered for the media session enterpictureinpicture action.
  if (CanEnterBrowserInitiatedAutoPictureInPicture()) {
    return media::PictureInPictureEventsInfo::AutoPipReason::kBrowserInitiated;
  }

  if (MeetsVideoPlaybackConditions()) {
    return media::PictureInPictureEventsInfo::AutoPipReason::kMediaPlayback;
  }

  return media::PictureInPictureEventsInfo::AutoPipReason::kUnknown;
}

media::PictureInPictureEventsInfo::AutoPipInfo
AutoPictureInPictureTabHelper::GetAutoPipInfo() const {
  return media::PictureInPictureEventsInfo::AutoPipInfo{
      .auto_pip_reason = GetAutoPipTriggerReason(),
      .has_audio_focus = has_audio_focus_,
      .is_playing = is_playing_,
      .was_recently_audible = WasRecentlyAudible(),
      .has_safe_url = has_safe_url_,
      .meets_media_engagement_conditions = MeetsMediaEngagementConditions(),
      .blocked_due_to_content_setting = blocked_due_to_content_setting_,
  };
}

media::PictureInPictureEventsInfo::AutoPipReason
AutoPictureInPictureTabHelper::GetAutoPipTriggerReason() const {
  return auto_pip_trigger_reason_;
}

bool AutoPictureInPictureTabHelper::IsInAutoPictureInPicture() const {
  return is_in_auto_picture_in_picture_;
}

bool AutoPictureInPictureTabHelper::AreAutoPictureInPicturePreconditionsMet()
    const {
  // Note that `auto_picture_in_picture_activation_time_` is not set if all of
  // the other preconditions are not set.
  return base::TimeTicks::Now() < auto_picture_in_picture_activation_time_;
}

#if !BUILDFLAG(IS_ANDROID)
std::unique_ptr<AutoPipSettingOverlayView>
AutoPictureInPictureTabHelper::CreateOverlayPermissionViewIfNeeded(
    base::OnceClosure close_pip_cb,
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow) {
  // Check both preconditions and "in pip", since we don't know if pip is
  // officially ready yet or not.  This might be during the opening of the pip
  // window, so we might not know about it yet.
  if (!AreAutoPictureInPicturePreconditionsMet() &&
      !IsInAutoPictureInPicture()) {
    // This isn't auto-pip, so the content setting doesn't matter.
    return nullptr;
  }

  // If we don't have a setting helper associated with this session (site) yet,
  // then create one.
  EnsureAutoPipSettingHelper();

  return auto_pip_setting_helper_->CreateOverlayViewIfNeeded(
      std::move(close_pip_cb), auto_pip_trigger_reason_, GetUkmSourceId(),
      anchor_view, arrow);
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
void AutoPictureInPictureTabHelper::OnPictureInPictureDismissed() {
  // An auto-PiP window is considered "dismissed" by the user if it's closed
  // shortly after appearing ("quick dismissal") or if the "hide" button is
  // clicked. Both actions signal that the user may not want auto-PiP for this
  // site, so we increment the dismissal count to potentially embargo the
  // feature.
  //
  // We only count dismissals if the tab is not active, to avoid counting cases
  // where the PiP window is automatically closed when switching back to the
  // tab.
  if (!is_tab_activated_ && auto_blocker_) {
    // Set `dismissed_prompt_was_quiet` to false for now as the dismissal count
    // threshold is only 1(vs 3) for quiet UI permission prompts, which might be
    // too stringent for auto-pip.
    // TODO(crbug.com/421606013): confirm the dismissal count threshold with
    // privacy reviewer.
    auto_blocker_->RecordDismissAndEmbargo(
        web_contents()->GetLastCommittedURL(),
        ContentSettingsType::AUTO_PICTURE_IN_PICTURE,
        /*dismissed_prompt_was_quiet=*/false);
  }
}

void AutoPictureInPictureTabHelper::OnPictureInPictureWindowWillHide() {
  hide_button_clicked_time_ = clock_->NowTicks();
}

int AutoPictureInPictureTabHelper::GetDismissCountForTesting(const GURL& url) {
  if (!auto_blocker_) {
    return 0;
  }
  return static_cast<permissions::PermissionDecisionAutoBlocker*>(
             auto_blocker_.get())
      ->GetDismissCount(url, ContentSettingsType::AUTO_PICTURE_IN_PICTURE);
}
#endif  // BUILDFLAG(IS_ANDROID)

void AutoPictureInPictureTabHelper::OnUserClosedWindow() {
#if !BUILDFLAG(IS_ANDROID)
  if (!auto_pip_setting_helper_) {
    // There is definitely no auto-pip UI showing, so ignore this.  Either this
    // isn't auto-pip, or we didn't need to ask the user about it.
    return;
  }

  // There might be the auto-pip setting UI shown, so forward this.
  auto_pip_setting_helper_->OnUserClosedWindow(GetAutoPipReason(),
                                               GetUkmSourceId());
#endif  // !BUILDFLAG(IS_ANDROID)
}

void AutoPictureInPictureTabHelper::OnTabBecameActive() {
#if BUILDFLAG(IS_ANDROID)
  if (hide_button_clicked_time_) {
    base::TimeDelta back_to_tab_post_hide_time =
        clock_->NowTicks() - hide_button_clicked_time_.value();
    hide_button_clicked_time_ = std::nullopt;

    base::UmaHistogramCustomTimes(
        "Media.AutoPictureInPicture.BackToTabPostHideTime",
        back_to_tab_post_hide_time, base::Milliseconds(1), base::Hours(10),
        100);
  }
#endif  // BUILDFLAG(IS_ANDROID)

  // We're the newly active tab, possibly before we've been notified by the tab
  // strip helper.  See if there's an autopip instance to close, and close it.
  // We may be called more than once for the same tab switch operation, once
  // from our tab strip observer and once from an incoming tab's tab helper.
  // This is because the order of the observers matters on the tab strip helper;
  // we don't know whether the outgoing or incoming tab will be notified first.
  // As a result, the outgoing tab notifies the incoming tab unconditionally, so
  // that the incoming tab has the opportunity to close pip.
  MaybeExitAutoPictureInPicture();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AutoPictureInPictureTabHelper);
