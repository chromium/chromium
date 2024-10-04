// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_helper.h"

#include "base/feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_strip_observer_helper.h"
#include "chrome/browser/picture_in_picture/auto_pip_setting_helper.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/media_session_service.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/frame/user_activation_state.h"

AutoPictureInPictureTabHelper::AutoPictureInPictureTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<AutoPictureInPictureTabHelper>(
          *web_contents),
      host_content_settings_map_(HostContentSettingsMapFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))),
      auto_blocker_(PermissionDecisionAutoBlockerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))) {
  // `base::Unretained` is safe here since we own `tab_strip_observer_helper_`.
  tab_strip_observer_helper_ =
      std::make_unique<AutoPictureInPictureTabStripObserverHelper>(
          web_contents,
          base::BindRepeating(
              &AutoPictureInPictureTabHelper::OnTabActivatedChanged,
              base::Unretained(this)));

  // Connect to receive audio focus events.
  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_remote;
  content::GetMediaSessionService().BindAudioFocusManager(
      audio_focus_remote.BindNewPipeAndPassReceiver());
  audio_focus_remote->AddObserver(
      audio_focus_observer_receiver_.BindNewPipeAndPassRemote());

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

AutoPictureInPictureTabHelper::~AutoPictureInPictureTabHelper() = default;

bool AutoPictureInPictureTabHelper::HasAutoPictureInPictureBeenRegistered()
    const {
  return has_ever_registered_for_auto_picture_in_picture_;
}

void AutoPictureInPictureTabHelper::PrimaryPageChanged(content::Page& page) {
  has_ever_registered_for_auto_picture_in_picture_ = false;
  // On navigation, forget any 'allow once' state.
  auto_pip_setting_helper_.reset();
}

void AutoPictureInPictureTabHelper::MediaPictureInPictureChanged(
    bool is_in_picture_in_picture) {
  if (is_in_picture_in_picture_ == is_in_picture_in_picture) {
    return;
  }
  is_in_picture_in_picture_ = is_in_picture_in_picture;

  if (!is_in_picture_in_picture_) {
    is_in_auto_picture_in_picture_ = false;
    MaybeStartOrStopObservingTabStrip();
    return;
  }

  if (AreAutoPictureInPicturePreconditionsMet()) {
    is_in_auto_picture_in_picture_ = true;
    auto_picture_in_picture_activation_time_ = base::TimeTicks();

    // If the tab is activated by the time auto picture-in-picture fires, we
    // should immediately close the auto picture-in-picture.
    if (is_tab_activated_) {
      MaybeExitAutoPictureInPicture();
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
    auto* active_contents = tab_strip_observer_helper_->GetActiveWebContents();
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
  }
}

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

void AutoPictureInPictureTabHelper::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  is_playing_ =
      session_info && session_info->playback_state ==
                          media_session::mojom::MediaPlaybackState::kPlaying;
}

void AutoPictureInPictureTabHelper::MediaSessionActionsChanged(
    const std::vector<media_session::mojom::MediaSessionAction>& actions) {
  is_enter_auto_picture_in_picture_available_ =
      base::ranges::find(actions,
                         media_session::mojom::MediaSessionAction::
                             kEnterAutoPictureInPicture) != actions.end();

  if (is_enter_auto_picture_in_picture_available_) {
    has_ever_registered_for_auto_picture_in_picture_ = true;
  }
  MaybeStartOrStopObservingTabStrip();
}

void AutoPictureInPictureTabHelper::MaybeEnterAutoPictureInPicture() {
  if (!IsEligibleForAutoPictureInPicture()) {
    MaybeGetVisibility();
    return;
  }

  EnterAutoPictureInPicture();
}

void AutoPictureInPictureTabHelper::EnterAutoPictureInPicture() {
  auto_picture_in_picture_activation_time_ =
      base::TimeTicks::Now() + blink::kActivationLifespan;
  content::MediaSession::Get(web_contents())->EnterAutoPictureInPicture();
}

void AutoPictureInPictureTabHelper::MaybeExitAutoPictureInPicture() {
  get_visibility_weak_factory_.InvalidateWeakPtrs();

  if (!is_in_auto_picture_in_picture_) {
    return;
  }
  is_in_auto_picture_in_picture_ = false;

  PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture();
}

void AutoPictureInPictureTabHelper::MaybeStartOrStopObservingTabStrip() {
  if (is_enter_auto_picture_in_picture_available_ ||
      is_in_auto_picture_in_picture_) {
    tab_strip_observer_helper_->StartObserving();
  } else {
    tab_strip_observer_helper_->StopObserving();
  }
}

bool AutoPictureInPictureTabHelper::IsEligibleForAutoPictureInPicture(
    HasSufficientlyVisibleVideo has_sufficiently_visible_video) {
  // Don't try to autopip if picture-in-picture is currently disabled.
  if (PictureInPictureWindowManager::GetInstance()
          ->IsPictureInPictureDisabled()) {
    return false;
  }

  // The tab must either have playback or be using camera/microphone to autopip.
  if (!MeetsVideoPlaybackConditions(has_sufficiently_visible_video) &&
      !IsUsingCameraOrMicrophone()) {
    return false;
  }

  // Only https:// or file:// may autopip.
  const GURL url = web_contents()->GetLastCommittedURL();
  if (!url.SchemeIs(url::kHttpsScheme) && !url.SchemeIsFile()) {
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
    EnsureAutoPipSettingHelper();
    auto_pip_setting_helper_->OnAutoPipBlockedByPermission();
    return false;
  } else if (setting == CONTENT_SETTING_ASK &&
             Profile::FromBrowserContext(web_contents()->GetBrowserContext())
                 ->IsIncognitoProfile()) {
    EnsureAutoPipSettingHelper();
    auto_pip_setting_helper_->OnAutoPipBlockedByIncognito();
    return false;
  }

  return true;
}

bool AutoPictureInPictureTabHelper::MeetsVideoPlaybackConditions(
    HasSufficientlyVisibleVideo has_sufficiently_visible_video) const {
  if (!base::FeatureList::IsEnabled(
          media::kAutoPictureInPictureForVideoPlayback)) {
    return false;
  }

  return has_audio_focus_ && is_playing_ && WasRecentlyAudible() &&
         (has_sufficiently_visible_video == HasSufficientlyVisibleVideo::kYes);
}

bool AutoPictureInPictureTabHelper::IsUsingCameraOrMicrophone() const {
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

void AutoPictureInPictureTabHelper::MaybeGetVisibility() {
  get_visibility_weak_factory_.InvalidateWeakPtrs();
  content::MediaSession* media_session =
      content::MediaSession::GetIfExists(web_contents());
  if (!media_session || is_in_picture_in_picture_) {
    return;
  }

  media_session->GetVisibility(
      base::BindOnce(&AutoPictureInPictureTabHelper::GetVideoVisibility,
                     get_visibility_weak_factory_.GetWeakPtr()));
}

void AutoPictureInPictureTabHelper::GetVideoVisibility(
    bool has_sufficiently_visible_video) {
  if (!has_sufficiently_visible_video || is_in_picture_in_picture_) {
    return;
  }

  if (!IsEligibleForAutoPictureInPicture(HasSufficientlyVisibleVideo::kYes)) {
    return;
  }

  EnterAutoPictureInPicture();
}

void AutoPictureInPictureTabHelper::EnsureAutoPipSettingHelper() {
  if (!auto_pip_setting_helper_) {
    auto_pip_setting_helper_ = AutoPipSettingHelper::CreateForWebContents(
        web_contents(), host_content_settings_map_, auto_blocker_);
  }
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
      std::move(close_pip_cb), anchor_view, arrow);
}

void AutoPictureInPictureTabHelper::OnUserClosedWindow() {
  if (!auto_pip_setting_helper_) {
    // There is definitely no auto-pip UI showing, so ignore this.  Either this
    // isn't auto-pip, or we didn't need to ask the user about it.
    return;
  }

  // There might be the auto-pip setting UI shown, so forward this.
  auto_pip_setting_helper_->OnUserClosedWindow();
}

void AutoPictureInPictureTabHelper::OnTabBecameActive() {
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
