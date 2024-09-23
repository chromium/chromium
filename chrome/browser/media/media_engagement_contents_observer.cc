// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_contents_observer.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/media/media_engagement_preloaded_list.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/media/media_engagement_session.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/autoplay/autoplay.mojom.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace {

void SendEngagementLevelToFrame(const url::Origin& origin,
                                content::RenderFrameHost* render_frame_host) {
  mojo::AssociatedRemote<blink::mojom::AutoplayConfigurationClient> client;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&client);
  client->AddAutoplayFlags(origin,
                           blink::mojom::kAutoplayFlagHighMediaEngagement);
}

}  // namespace.

// This is the minimum size (in px) of each dimension that a media
// element has to be in order to be determined significant.
const gfx::Size MediaEngagementContentsObserver::kSignificantSize =
    gfx::Size(200, 140);

const base::TimeDelta MediaEngagementContentsObserver::kMaxShortPlaybackTime =
    base::Seconds(3);

const base::TimeDelta
    MediaEngagementContentsObserver::kSignificantMediaPlaybackTime =
        base::Seconds(7);

MediaEngagementContentsObserver::MediaEngagementContentsObserver(
    content::WebContents* web_contents,
    MediaEngagementService* service)
    : WebContentsObserver(web_contents),
      service_(service),
      task_runner_(nullptr) {}

MediaEngagementContentsObserver::~MediaEngagementContentsObserver() = default;

MediaEngagementContentsObserver::PlaybackTimer::PlaybackTimer(
    base::Clock* clock)
    : clock_(clock) {}

MediaEngagementContentsObserver::PlaybackTimer::~PlaybackTimer() = default;

void MediaEngagementContentsObserver::PlaybackTimer::Start() {
  start_time_ = clock_->Now();
}

void MediaEngagementContentsObserver::PlaybackTimer::Stop() {
  recorded_time_ = Elapsed();
  start_time_.reset();
}

bool MediaEngagementContentsObserver::PlaybackTimer::IsRunning() const {
  return start_time_.has_value();
}

base::TimeDelta MediaEngagementContentsObserver::PlaybackTimer::Elapsed()
    const {
  base::Time now = clock_->Now();
  base::TimeDelta duration = now - start_time_.value_or(now);
  return recorded_time_ + duration;
}

void MediaEngagementContentsObserver::PlaybackTimer::Reset() {
  recorded_time_ = base::TimeDelta();
  start_time_.reset();
}

void MediaEngagementContentsObserver::WebContentsDestroyed() {
  RegisterAudiblePlayersWithSession();
  session_ = nullptr;

  ClearPlayerStates();
  service_->contents_observers_.erase(web_contents());
  delete this;
}

void MediaEngagementContentsObserver::ClearPlayerStates() {
  playback_timer_.Stop();
  player_states_.clear();
  significant_players_.clear();
  audio_context_players_.clear();
  audio_context_timer_.Stop();
}

void MediaEngagementContentsObserver::RegisterAudiblePlayersWithSession() {
  if (!session_)
    return;

  int32_t significant_players = 0;
  int32_t audible_players = 0;

  for (const auto& row : audible_players_) {
    const PlayerState& player_state = GetPlayerState(row.first);
    const base::TimeDelta elapsed = player_state.playback_timer->Elapsed();

    if (elapsed < kMaxShortPlaybackTime && player_state.reached_end_of_stream) {
      session_->RecordShortPlaybackIgnored(elapsed.InMilliseconds());
      continue;
    }

    significant_players += row.second.first;
    ++audible_players;
  }

  session_->RegisterAudiblePlayers(audible_players, significant_players);
  audible_players_.clear();
}

void MediaEngagementContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument() || navigation_handle->IsErrorPage()) {
    return;
  }

  RegisterAudiblePlayersWithSession();
  ClearPlayerStates();

  if (session_ && session_->IsSameOriginWith(navigation_handle->GetURL()))
    return;

  // Only get the opener if the navigation originated from a link.
  // This is done outside of `GetOrCreateSession()` to simplify unit testing.
  content::WebContents* opener = nullptr;
  if (ui::PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                                   ui::PAGE_TRANSITION_LINK) ||
      ui::PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                                   ui::PAGE_TRANSITION_RELOAD)) {
    opener = GetOpener();
  }

  session_ = GetOrCreateSession(navigation_handle, opener);
}

MediaEngagementContentsObserver::PlayerState::PlayerState(base::Clock* clock)
    : playback_timer(new PlaybackTimer(clock)) {}

MediaEngagementContentsObserver::PlayerState::~PlayerState() = default;

MediaEngagementContentsObserver::PlayerState::PlayerState(PlayerState&&) =
    default;

MediaEngagementContentsObserver::PlayerState&
MediaEngagementContentsObserver::GetPlayerState(
    const content::MediaPlayerId& id) {
  auto state = player_states_.find(id);
  if (state != player_states_.end())
    return state->second;

  auto iter =
      player_states_.insert(std::make_pair(id, PlayerState(service_->clock_)));
  return iter.first->second;
}

void MediaEngagementContentsObserver::MediaStartedPlaying(
    const MediaPlayerInfo& media_player_info,
    const content::MediaPlayerId& media_player_id) {
  PlayerState& state = GetPlayerState(media_player_id);
  state.playing = true;
  state.has_audio = media_player_info.has_audio;
  state.has_video = media_player_info.has_video;

  // Reset the playback timer if we previously reached the end of the stream.
  if (state.reached_end_of_stream) {
    state.playback_timer->Reset();
    state.reached_end_of_stream = false;
  }
  state.playback_timer->Start();

  MaybeInsertRemoveSignificantPlayer(media_player_id);
  UpdatePlayerTimer(media_player_id);
}

void MediaEngagementContentsObserver::MediaMutedStatusChanged(
    const content::MediaPlayerId& id,
    bool muted) {
  GetPlayerState(id).muted = muted;
  MaybeInsertRemoveSignificantPlayer(id);
  UpdatePlayerTimer(id);
}

void MediaEngagementContentsObserver::MediaResized(
    const gfx::Size& size,
    const content::MediaPlayerId& id) {
  GetPlayerState(id).significant_size =
      (size.width() >= kSignificantSize.width() &&
       size.height() >= kSignificantSize.height());
  MaybeInsertRemoveSignificantPlayer(id);
  UpdatePlayerTimer(id);
}

void MediaEngagementContentsObserver::MediaDestroyed(
    const content::MediaPlayerId& id) {
  player_states_.erase(id);
  audible_players_.erase(id);
  significant_players_.erase(id);
}

void MediaEngagementContentsObserver::MediaStoppedPlaying(
    const MediaPlayerInfo& media_player_info,
    const content::MediaPlayerId& media_player_id,
    WebContentsObserver::MediaStoppedReason reason) {
  PlayerState& state = GetPlayerState(media_player_id);
  state.playing = false;
  state.reached_end_of_stream =
      reason == WebContentsObserver::MediaStoppedReason::kReachedEndOfStream;

  // Reset the playback timer if we finished playing.
  state.playback_timer->Stop();

  MaybeInsertRemoveSignificantPlayer(media_player_id);
  UpdatePlayerTimer(media_player_id);
}

void MediaEngagementContentsObserver::AudioContextPlaybackStarted(
    const AudioContextId& audio_context_id) {
  audio_context_players_.insert(audio_context_id);
  UpdateAudioContextTimer();
}

void MediaEngagementContentsObserver::AudioContextPlaybackStopped(
    const AudioContextId& audio_context_id) {
  audio_context_players_.erase(audio_context_id);
  UpdateAudioContextTimer();
}

void MediaEngagementContentsObserver::DidUpdateAudioMutingState(bool muted) {
  UpdatePageTimer();
  UpdateAudioContextTimer();
}

bool MediaEngagementContentsObserver::IsPlayerStateComplete(
    const PlayerState& state) {
  return state.muted.has_value() && state.playing.has_value() &&
         state.has_audio.has_value() && state.has_video.has_value() &&
         (!state.has_video.value_or(false) ||
          state.significant_size.has_value());
}

void MediaEngagementContentsObserver::OnSignificantMediaPlaybackTimeForPlayer(
    const content::MediaPlayerId& id) {
  // Clear the timer.
  auto audible_row = audible_players_.find(id);
  CHECK(audible_row != audible_players_.end());

  audible_row->second.second = nullptr;

  // Check that the tab is not muted.
  auto* audible_helper = RecentlyAudibleHelper::FromWebContents(web_contents());
  if (web_contents()->IsAudioMuted() || !audible_helper->WasRecentlyAudible())
    return;

  // Record significant audible playback.
  audible_row->second.first = true;
}

void MediaEngagementContentsObserver::OnSignificantMediaPlaybackTimeForPage() {
  DCHECK(session_);

  if (session_->significant_media_element_playback_recorded())
    return;

  // Do not record significant playback if the tab did not make
  // a sound recently.
  auto* audible_helper = RecentlyAudibleHelper::FromWebContents(web_contents());
  if (!audible_helper->WasRecentlyAudible())
    return;

  session_->RecordSignificantMediaElementPlayback();
}

void MediaEngagementContentsObserver::
    OnSignificantAudioContextPlaybackTimeForPage() {
  DCHECK(session_);

  if (session_->significant_audio_context_playback_recorded())
    return;

  // Do not record significant playback if the tab did not make
  // a sound recently.
  auto* audible_helper = RecentlyAudibleHelper::FromWebContents(web_contents());
  if (!audible_helper->WasRecentlyAudible())
    return;

  session_->RecordSignificantAudioContextPlayback();
}

void MediaEngagementContentsObserver::MaybeInsertRemoveSignificantPlayer(
    const content::MediaPlayerId& id) {
  // If we have not received the whole player state yet then we can't be
  // significant and therefore we don't want to make a decision yet.
  PlayerState& state = GetPlayerState(id);
  if (!IsPlayerStateComplete(state))
    return;

  // If the player has an audio track, is un-muted and is playing then we should
  // add it to the audible players map.
  if (state.muted == false && state.playing == true &&
      state.has_audio == true &&
      audible_players_.find(id) == audible_players_.end()) {
    audible_players_.emplace(id, std::make_pair(false, nullptr));
  }

  const bool is_currently_listed_significant =
      significant_players_.find(id) != significant_players_.end();

  if (is_currently_listed_significant) {
    if (!IsSignificantPlayer(id)) {
      significant_players_.erase(id);
    }
  } else {
    if (IsSignificantPlayer(id)) {
      significant_players_.insert(id);
    }
  }
}

bool MediaEngagementContentsObserver::IsSignificantPlayer(
    const content::MediaPlayerId& id) {
  const PlayerState& state = GetPlayerState(id);

  if (state.muted.value_or(true)) {
    return false;
  }

  if (!state.playing.value_or(false)) {
    return false;
  }

  if (!state.significant_size.value_or(false) &&
      state.has_video.value_or(false)) {
    return false;
  }

  if (!state.has_audio.value_or(false)) {
    return false;
  }

  return true;
}

void MediaEngagementContentsObserver::UpdatePlayerTimer(
    const content::MediaPlayerId& id) {
  UpdatePageTimer();

  // The player should be considered audible.
  auto audible_row = audible_players_.find(id);
  if (audible_row == audible_players_.end())
    return;

  // If we meet all the reqirements for being significant then start a timer.
  if (significant_players_.find(id) != significant_players_.end()) {
    if (audible_row->second.second)
      return;

    auto new_timer = std::make_unique<base::OneShotTimer>();
    if (task_runner_)
      new_timer->SetTaskRunner(task_runner_);

    new_timer->Start(
        FROM_HERE,
        MediaEngagementContentsObserver::kSignificantMediaPlaybackTime,
        base::BindOnce(&MediaEngagementContentsObserver::
                           OnSignificantMediaPlaybackTimeForPlayer,
                       base::Unretained(this), id));

    audible_row->second.second = std::move(new_timer);
  } else if (audible_row->second.second) {
    // We no longer meet the requirements so we should get rid of the timer.
    audible_row->second.second = nullptr;
  }
}

bool MediaEngagementContentsObserver::AreConditionsMet() const {
  if (significant_players_.empty())
    return false;

  return !web_contents()->IsAudioMuted();
}

void MediaEngagementContentsObserver::UpdatePageTimer() {
  if (!session_ || session_->significant_media_element_playback_recorded())
    return;

  if (AreConditionsMet()) {
    if (playback_timer_.IsRunning())
      return;

    if (task_runner_)
      playback_timer_.SetTaskRunner(task_runner_);

    playback_timer_.Start(
        FROM_HERE,
        MediaEngagementContentsObserver::kSignificantMediaPlaybackTime,
        base::BindOnce(&MediaEngagementContentsObserver::
                           OnSignificantMediaPlaybackTimeForPage,
                       base::Unretained(this)));
  } else {
    if (!playback_timer_.IsRunning())
      return;
    playback_timer_.Stop();
  }
}

bool MediaEngagementContentsObserver::AreAudioContextConditionsMet() const {
  if (!base::FeatureList::IsEnabled(media::kRecordWebAudioEngagement))
    return false;

  if (audio_context_players_.empty())
    return false;

  return !web_contents()->IsAudioMuted();
}

void MediaEngagementContentsObserver::UpdateAudioContextTimer() {
  if (!session_ || session_->significant_audio_context_playback_recorded())
    return;

  if (AreAudioContextConditionsMet()) {
    if (audio_context_timer_.IsRunning())
      return;

    if (task_runner_)
      audio_context_timer_.SetTaskRunner(task_runner_);

    audio_context_timer_.Start(
        FROM_HERE,
        MediaEngagementContentsObserver::kSignificantMediaPlaybackTime,
        base::BindOnce(&MediaEngagementContentsObserver::
                           OnSignificantAudioContextPlaybackTimeForPage,
                       base::Unretained(this)));
  } else if (audio_context_timer_.IsRunning()) {
    audio_context_timer_.Stop();
  }
}

void MediaEngagementContentsObserver::SetTaskRunnerForTest(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  task_runner_ = std::move(task_runner);
}

void MediaEngagementContentsObserver::ReadyToCommitNavigation(
    content::NavigationHandle* handle) {
  // Do nothing if prerendering.
  if (handle->GetRenderFrameHost()->GetLifecycleState() ==
          content::RenderFrameHost::LifecycleState::kPrerendering &&
      !handle->IsPrerenderedPageActivation()) {
    return;
  }

  // If the navigation is occurring in the main frame we should use the URL
  // provided by |handle| as the navigation has not committed yet. If the
  // navigation is in a subframe or fenced frame, use the URL from the outermost
  // main frame.
  url::Origin origin = url::Origin::Create(handle->IsInPrimaryMainFrame()
                                               ? handle->GetURL()
                                               : handle->GetRenderFrameHost()
                                                     ->GetOutermostMainFrame()
                                                     ->GetLastCommittedURL());
  MediaEngagementScore score = service_->CreateEngagementScore(origin);
  bool has_high_engagement = score.high_score();

  if (base::FeatureList::IsEnabled(media::kMediaEngagementHTTPSOnly))
    DCHECK(!has_high_engagement || (origin.scheme() == url::kHttpsScheme));

  // If the preloaded feature flag is enabled and the number of visits is less
  // than the number of visits required to have an MEI score we should check the
  // global data.
  if (!has_high_engagement &&
      score.visits() < MediaEngagementScore::GetScoreMinVisits() &&
      base::FeatureList::IsEnabled(media::kPreloadMediaEngagementData)) {
    has_high_engagement =
        MediaEngagementPreloadedList::GetInstance()->CheckOriginIsPresent(
            origin);
  }

  // If we have high media engagement then we should send that to Blink.
  if (has_high_engagement) {
    SendEngagementLevelToFrame(url::Origin::Create(handle->GetURL()),
                               handle->GetRenderFrameHost());
  }
}

content::WebContents* MediaEngagementContentsObserver::GetOpener() const {
#if !BUILDFLAG(IS_ANDROID)
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile() != service_->profile())
      continue;

    int index =
        browser->tab_strip_model()->GetIndexOfWebContents(web_contents());
    if (index == TabStripModel::kNoTab)
      continue;

    // Whether or not the |opener| is null, this is the right tab strip.
    const tabs::TabModel* tab =
        browser->tab_strip_model()->GetOpenerOfTabAt(index);
    return tab ? tab->contents() : nullptr;
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  return nullptr;
}

scoped_refptr<MediaEngagementSession>
MediaEngagementContentsObserver::GetOrCreateSession(
    content::NavigationHandle* navigation_handle,
    content::WebContents* opener) const {
  url::Origin origin = url::Origin::Create(navigation_handle->GetURL());

  if (origin.opaque())
    return nullptr;

  if (!service_->ShouldRecordEngagement(origin))
    return nullptr;

  MediaEngagementContentsObserver* opener_observer =
      service_->GetContentsObserverFor(opener);

  if (opener_observer && opener_observer->session_ &&
      opener_observer->session_->IsSameOriginWith(
          navigation_handle->GetURL())) {
    return opener_observer->session_;
  }

  MediaEngagementSession::RestoreType restore_type =
      navigation_handle->GetRestoreType() == content::RestoreType::kNotRestored
          ? MediaEngagementSession::RestoreType::kNotRestored
          : MediaEngagementSession::RestoreType::kRestored;

  return new MediaEngagementSession(
      service_, origin, restore_type,
      ukm::ConvertToSourceId(navigation_handle->GetNavigationId(),
                             ukm::SourceIdType::NAVIGATION_ID));
}
