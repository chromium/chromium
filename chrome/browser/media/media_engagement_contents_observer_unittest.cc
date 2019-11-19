// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_contents_observer.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/timer/mock_timer.h"
#include "build/build_config.h"
#include "chrome/browser/media/media_engagement_score.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/media/media_engagement_service_factory.h"
#include "chrome/browser/media/media_engagement_session.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_service_manager_context.h"
#include "content/public/test/web_contents_tester.h"
#include "media/base/media_switches.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(crbug/1004580) All these tests crash on Android
#if !defined(OS_ANDROID)
class MediaEngagementContentsObserverTest
    : public ChromeRenderViewHostTestHarness {
 public:
  MediaEngagementContentsObserverTest()
      : task_runner_(new base::TestMockTimeTaskRunner()),
        test_service_manager_context_(
            std::make_unique<content::TestServiceManagerContext>()) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    SetContents(content::WebContentsTester::CreateTestWebContents(
        browser_context(), nullptr));
    RecentlyAudibleHelper::CreateForWebContents(web_contents());

    service_ =
        base::WrapUnique(new MediaEngagementService(profile(), &test_clock_));
    contents_observer_ = CreateContentsObserverFor(web_contents());

    // Navigate to an initial URL to setup the |session|.
    content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(GURL("https://first.example.com"));

    contents_observer_->SetTaskRunnerForTest(task_runner_);
    SimulateInaudible();

    // Advance the test clock to a non-null value.
    Advance15Minutes();
  }

  void TearDown() override {
    // Must be reset before browser thread teardown.
    test_service_manager_context_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  MediaEngagementContentsObserver* CreateContentsObserverFor(
      content::WebContents* web_contents) {
    MediaEngagementContentsObserver* contents_observer =
        new MediaEngagementContentsObserver(web_contents, service_.get());
    service_->contents_observers_.insert({web_contents, contents_observer});
    return contents_observer;
  }

  bool IsTimerRunning() const {
    return contents_observer_->playback_timer_.IsRunning();
  }

  bool IsTimerRunningForPlayer(int id) const {
    content::MediaPlayerId player_id(nullptr /* RenderFrameHost */, id);
    auto audible_row = contents_observer_->audible_players_.find(player_id);
    return audible_row != contents_observer_->audible_players_.end() &&
           audible_row->second.second;
  }

  bool IsAudioContextTimerRunning() const {
    return contents_observer_->audio_context_timer_.IsRunning();
  }

  bool HasSession() const { return !!contents_observer_->session_; }

  bool WasSignificantPlaybackRecorded() const {
    return contents_observer_->session_->WasSignificantPlaybackRecorded();
  }

  bool WasSignificantAudioContextPlaybackRecorded() const {
    return contents_observer_->session_
        ->significant_audio_context_playback_recorded();
  }

  size_t GetSignificantActivePlayersCount() const {
    return contents_observer_->significant_players_.size();
  }

  size_t GetStoredPlayerStatesCount() const {
    return contents_observer_->player_states_.size();
  }

  size_t GetAudioContextPlayersCount() const {
    return contents_observer_->audio_context_players_.size();
  }

  void SimulatePlaybackStarted(int id, bool has_audio, bool has_video) {
    content::WebContentsObserver::MediaPlayerInfo player_info(has_video,
                                                              has_audio);
    SimulatePlaybackStarted(player_info, id, false);
  }

  void SimulateResizeEvent(int id, gfx::Size size) {
    content::MediaPlayerId player_id(nullptr /* RenderFrameHost */, id);
    contents_observer_->MediaResized(size, player_id);
  }

  void SimulateAudioVideoPlaybackStarted(int id) {
    SimulatePlaybackStarted(id, true, true);
  }

  void SimulateResizeEventSignificantSize(int id) {
    SimulateResizeEvent(id, MediaEngagementContentsObserver::kSignificantSize);
  }

  void SimulatePlaybackStarted(
      content::WebContentsObserver::MediaPlayerInfo player_info,
      int id,
      bool muted_state) {
    content::MediaPlayerId player_id(nullptr /* RenderFrameHost */, id);
    contents_observer_->MediaStartedPlaying(player_info, player_id);
    SimulateMutedStateChange(id, muted_state);
  }

  void SimulatePlaybackStoppedWithTime(int id,
                                       bool finished,
                                       base::TimeDelta elapsed) {
    test_clock_.Advance(elapsed);

    content::WebContentsObserver::MediaPlayerInfo player_info(true, true);
    content::MediaPlayerId player_id(nullptr /* RenderFrameHost */, id);
    contents_observer_->MediaStoppedPlaying(
        player_info, player_id,
        finished
            ? content::WebContentsObserver::MediaStoppedReason::
                  kReachedEndOfStream
            : content::WebContentsObserver::MediaStoppedReason::kUnspecified);
  }

  void SimulatePlaybackStopped(int id) {
    SimulatePlaybackStoppedWithTime(id, true, base::TimeDelta::FromSeconds(0));
  }

  void SimulateMutedStateChange(int id, bool muted) {
    content::MediaPlayerId player_id(nullptr /* RenderFrameHost */, id);
    contents_observer_->MediaMutedStatusChanged(player_id, muted);
  }

  void SimulateIsVisible() {
    contents_observer_->OnVisibilityChanged(content::Visibility::VISIBLE);
  }

  void SimulateIsHidden() {
    contents_observer_->OnVisibilityChanged(content::Visibility::HIDDEN);
  }

  void SimulateAudioContextStarted(int id) {
    content::WebContentsObserver::AudioContextId player_id(
        nullptr /* RenderFrameHost */, id);
    contents_observer_->AudioContextPlaybackStarted(player_id);
  }

  void SimulateAudioContextStopped(int id) {
    content::WebContentsObserver::AudioContextId player_id(
        nullptr /* RenderFrameHost */, id);
    contents_observer_->AudioContextPlaybackStopped(player_id);
  }

  bool AreConditionsMet() const {
    return contents_observer_->AreConditionsMet();
  }

  bool AreAudioContextConditionsMet() const {
    return contents_observer_->AreAudioContextConditionsMet();
  }

  void SimulateSignificantMediaElementPlaybackRecorded() {
    contents_observer_->session_->RecordSignificantMediaElementPlayback();
  }

  void SimulateSignificantMediaElementPlaybackTimeForPage() {
    contents_observer_->OnSignificantMediaPlaybackTimeForPage();
  }

  void SimulateSignificantAudioContextPlaybackRecorded() {
    contents_observer_->OnSignificantAudioContextPlaybackTimeForPage();
  }

  void SimulateSignificantPlaybackTimeForPlayer(int id) {
    SimulateLongMediaPlayback(id);
    content::MediaPlayerId player_id(nullptr /* RenderFrameHost */, id);
    contents_observer_->OnSignificantMediaPlaybackTimeForPlayer(player_id);
  }

  void SimulatePlaybackTimerFired() {
    task_runner_->FastForwardBy(kMaxWaitingTime);
  }

  void SimulateAudioContextPlaybackTimerFired() {
    task_runner_->FastForwardBy(kMaxWaitingTime);
  }

  void ExpectScores(const url::Origin origin,
                    double expected_score,
                    int expected_visits,
                    int expected_media_playbacks,
                    int expected_audible_playbacks,
                    int expected_significant_playbacks,
                    int expected_media_element_playbacks,
                    int expected_audio_context_playbacks) {
    EXPECT_EQ(service_->GetEngagementScore(origin), expected_score);
    EXPECT_EQ(service_->GetScoreMapForTesting()[origin], expected_score);

    MediaEngagementScore score = service_->CreateEngagementScore(origin);
    EXPECT_EQ(expected_visits, score.visits());
    EXPECT_EQ(expected_media_playbacks, score.media_playbacks());
    EXPECT_EQ(expected_audible_playbacks, score.audible_playbacks());
    EXPECT_EQ(expected_significant_playbacks, score.significant_playbacks());
    EXPECT_EQ(expected_media_element_playbacks,
              score.media_element_playbacks());
    EXPECT_EQ(expected_audio_context_playbacks,
              score.audio_context_playbacks());
  }

  void SetScores(const url::Origin& origin,
                 int visits,
                 int media_playbacks,
                 int audible_playbacks,
                 int significant_playbacks) {
    MediaEngagementScore score =
        contents_observer_->service_->CreateEngagementScore(origin);
    score.SetVisits(visits);
    score.SetMediaPlaybacks(media_playbacks);
    score.set_audible_playbacks(audible_playbacks);
    score.set_significant_playbacks(significant_playbacks);
    score.Commit();
  }

  void SetScores(const url::Origin& origin, int visits, int media_playbacks) {
    SetScores(origin, visits, media_playbacks, 0, 0);
  }

  void Navigate(const GURL& url) {
    content::MockNavigationHandle test_handle(GURL(url), main_rfh());
    test_ukm_recorder_.UpdateSourceURL(
        ukm::ConvertToSourceId(test_handle.GetNavigationId(),
                               ukm::SourceIdType::NAVIGATION_ID),
        url);

    contents_observer_->ReadyToCommitNavigation(&test_handle);

    test_handle.set_has_committed(true);
    contents_observer_->DidFinishNavigation(&test_handle);
  }

  scoped_refptr<MediaEngagementSession> GetOrCreateSession(
      const url::Origin& origin,
      content::WebContents* opener) {
    content::MockNavigationHandle navigation_handle(origin.GetURL(), nullptr);
    return contents_observer_->GetOrCreateSession(&navigation_handle, opener);
  }

  scoped_refptr<MediaEngagementSession> GetSessionFor(
      MediaEngagementContentsObserver* contents_observer) {
    return contents_observer->session_;
  }

  void SimulateAudible() {
    content::WebContentsTester::For(web_contents())
        ->SetIsCurrentlyAudible(true);
  }

  void SimulateInaudible() {
    content::WebContentsTester::For(web_contents())
        ->SetIsCurrentlyAudible(false);
  }

  void ExpectUkmEntry(const url::Origin& origin,
                      int playbacks_total,
                      int visits_total,
                      int score,
                      int audible_players_delta,
                      int significant_players_delta) {
    using Entry = ukm::builders::Media_Engagement_SessionFinished;

    auto ukm_entries = test_ukm_recorder_.GetEntriesByName(Entry::kEntryName);
    ASSERT_NE(0u, ukm_entries.size());

    auto* ukm_entry = ukm_entries.back();
    test_ukm_recorder_.ExpectEntrySourceHasUrl(ukm_entry, origin.GetURL());
    EXPECT_EQ(playbacks_total, *test_ukm_recorder_.GetEntryMetric(
                                   ukm_entry, Entry::kPlaybacks_TotalName));
    EXPECT_EQ(visits_total, *test_ukm_recorder_.GetEntryMetric(
                                ukm_entry, Entry::kVisits_TotalName));
    EXPECT_EQ(score, *test_ukm_recorder_.GetEntryMetric(
                         ukm_entry, Entry::kEngagement_ScoreName));
    EXPECT_EQ(audible_players_delta,
              *test_ukm_recorder_.GetEntryMetric(
                  ukm_entry, Entry::kPlayer_Audible_DeltaName));
    EXPECT_EQ(significant_players_delta,
              *test_ukm_recorder_.GetEntryMetric(
                  ukm_entry, Entry::kPlayer_Significant_DeltaName));
  }

  void ExpectUkmIgnoredEntries(const url::Origin& origin,
                               std::vector<int64_t> entries) {
    using Entry = ukm::builders::Media_Engagement_ShortPlaybackIgnored;
    auto ukm_entries = test_ukm_recorder_.GetEntriesByName(Entry::kEntryName);

    EXPECT_EQ(entries.size(), ukm_entries.size());
    for (std::vector<int>::size_type i = 0; i < entries.size(); i++) {
      test_ukm_recorder_.ExpectEntrySourceHasUrl(ukm_entries[i],
                                                 origin.GetURL());
      EXPECT_EQ(entries[i], *test_ukm_recorder_.GetEntryMetric(
                                ukm_entries[i], Entry::kLengthName));
    }
  }

  void ExpectNoUkmIgnoreEntry() {
    using Entry = ukm::builders::Media_Engagement_ShortPlaybackIgnored;
    auto ukm_entries = test_ukm_recorder_.GetEntriesByName(Entry::kEntryName);
    EXPECT_EQ(0U, ukm_entries.size());
  }

  void ExpectNoUkmEntry() { EXPECT_FALSE(test_ukm_recorder_.sources_count()); }

  void SimulateDestroy() { contents_observer_->WebContentsDestroyed(); }

  void SimulateSignificantAudioPlayer(int id) {
    SimulatePlaybackStarted(id, true, false);
    SimulateAudible();
    web_contents()->SetAudioMuted(false);
  }

  void SimulateSignificantVideoPlayer(int id) {
    SimulateAudioVideoPlaybackStarted(id);
    SimulateAudible();
    web_contents()->SetAudioMuted(false);
    SimulateResizeEventSignificantSize(id);
  }

  void ForceUpdateTimer(int id) {
    content::MediaPlayerId player_id(nullptr /* RenderFrameHost */, id);
    contents_observer_->UpdatePlayerTimer(player_id);
  }

  void ExpectNotAddedFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason reason,
      int count) {
    histogram_tester_.ExpectBucketCount(
        MediaEngagementContentsObserver::
            kHistogramSignificantNotAddedFirstTimeName,
        static_cast<int>(reason), count);
  }

  void ExpectNotAddedAfterFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason reason,
      int count) {
    histogram_tester_.ExpectBucketCount(
        MediaEngagementContentsObserver::
            kHistogramSignificantNotAddedAfterFirstTimeName,
        static_cast<int>(reason), count);
  }

  void ExpectRemovedBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason reason,
      int count) {
    histogram_tester_.ExpectBucketCount(
        MediaEngagementContentsObserver::kHistogramSignificantRemovedName,
        static_cast<int>(reason), count);
  }

  void ExpectPlaybackTime(int id, base::TimeDelta expected_time) {
    content::MediaPlayerId player_id(nullptr /* RenderFrameHost */, id);
    EXPECT_EQ(expected_time, contents_observer_->GetPlayerState(player_id)
                                 .playback_timer->Elapsed());
  }

  void SimulateLongMediaPlayback(int id) {
    SimulatePlaybackStoppedWithTime(
        id, false, MediaEngagementContentsObserver::kMaxShortPlaybackTime);
  }

  void SetLastPlaybackTime(const url::Origin& origin, base::Time new_time) {
    MediaEngagementScore score = service_->CreateEngagementScore(origin);
    score.set_last_media_playback_time(new_time);
    score.Commit();
  }

  void ExpectLastPlaybackTime(const url::Origin& origin,
                              const base::Time expected_time) {
    MediaEngagementScore score = service_->CreateEngagementScore(origin);
    EXPECT_EQ(expected_time, score.last_media_playback_time());
  }

  base::Time Now() { return test_clock_.Now(); }

  void Advance15Minutes() {
    test_clock_.Advance(base::TimeDelta::FromMinutes(15));
  }

  ukm::TestAutoSetUkmRecorder& test_ukm_recorder() {
    return test_ukm_recorder_;
  }

 private:
  // contents_observer_ auto-destroys when WebContents is destroyed.
  MediaEngagementContentsObserver* contents_observer_;

  std::unique_ptr<MediaEngagementService> service_;

  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;

  base::HistogramTester histogram_tester_;

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

  base::SimpleTestClock test_clock_;

  const base::TimeDelta kMaxWaitingTime =
      MediaEngagementContentsObserver::kSignificantMediaPlaybackTime +
      base::TimeDelta::FromSeconds(2);

  // WebContentsImpl accesses the system Connector so the Service Manager must
  // be initialized.
  std::unique_ptr<content::TestServiceManagerContext>
      test_service_manager_context_;
};

// TODO(mlamouri): test that visits are not recorded multiple times when a
// same-origin navigation happens.

TEST_F(MediaEngagementContentsObserverTest, SignificantActivePlayerCount) {
  EXPECT_EQ(0u, GetSignificantActivePlayersCount());
  EXPECT_EQ(0u, GetAudioContextPlayersCount());

  SimulateAudioVideoPlaybackStarted(0);
  SimulateResizeEventSignificantSize(0);
  EXPECT_EQ(1u, GetSignificantActivePlayersCount());
  EXPECT_EQ(0u, GetAudioContextPlayersCount());

  SimulateAudioVideoPlaybackStarted(1);
  SimulateResizeEventSignificantSize(1);
  EXPECT_EQ(2u, GetSignificantActivePlayersCount());
  EXPECT_EQ(0u, GetAudioContextPlayersCount());

  SimulateAudioVideoPlaybackStarted(2);
  SimulateResizeEventSignificantSize(2);
  EXPECT_EQ(3u, GetSignificantActivePlayersCount());
  EXPECT_EQ(0u, GetAudioContextPlayersCount());

  SimulatePlaybackStopped(1);
  EXPECT_EQ(2u, GetSignificantActivePlayersCount());
  EXPECT_EQ(0u, GetAudioContextPlayersCount());

  SimulatePlaybackStopped(0);
  EXPECT_EQ(1u, GetSignificantActivePlayersCount());
  EXPECT_EQ(0u, GetAudioContextPlayersCount());

  SimulateResizeEvent(2, gfx::Size(1, 1));
  EXPECT_EQ(0u, GetSignificantActivePlayersCount());
  EXPECT_EQ(0u, GetAudioContextPlayersCount());

  SimulateSignificantAudioPlayer(3);
  EXPECT_EQ(1u, GetSignificantActivePlayersCount());
  EXPECT_EQ(0u, GetAudioContextPlayersCount());

  SimulateAudioContextStarted(0);
  EXPECT_EQ(1u, GetSignificantActivePlayersCount());
  EXPECT_EQ(1u, GetAudioContextPlayersCount());

  SimulateAudioContextStarted(1);
  EXPECT_EQ(1u, GetSignificantActivePlayersCount());
  EXPECT_EQ(2u, GetAudioContextPlayersCount());

  SimulateAudioContextStopped(0);
  EXPECT_EQ(1u, GetSignificantActivePlayersCount());
  EXPECT_EQ(1u, GetAudioContextPlayersCount());
}

TEST_F(MediaEngagementContentsObserverTest, AreConditionsMet) {
  EXPECT_FALSE(AreConditionsMet());
  EXPECT_FALSE(AreAudioContextConditionsMet());

  SimulateSignificantVideoPlayer(0);
  EXPECT_TRUE(AreConditionsMet());
  EXPECT_FALSE(AreAudioContextConditionsMet());

  SimulateResizeEvent(0, gfx::Size(1, 1));
  EXPECT_FALSE(AreConditionsMet());
  EXPECT_FALSE(AreAudioContextConditionsMet());

  SimulateResizeEventSignificantSize(0);
  EXPECT_TRUE(AreConditionsMet());
  EXPECT_FALSE(AreAudioContextConditionsMet());

  SimulateResizeEvent(
      0,
      gfx::Size(MediaEngagementContentsObserver::kSignificantSize.width(), 1));
  EXPECT_FALSE(AreConditionsMet());
  EXPECT_FALSE(AreAudioContextConditionsMet());

  SimulateResizeEvent(
      0,
      gfx::Size(1, MediaEngagementContentsObserver::kSignificantSize.height()));
  EXPECT_FALSE(AreConditionsMet());
  EXPECT_FALSE(AreAudioContextConditionsMet());
  SimulateResizeEventSignificantSize(0);

  web_contents()->SetAudioMuted(true);
  EXPECT_FALSE(AreConditionsMet());
  EXPECT_FALSE(AreAudioContextConditionsMet());
  web_contents()->SetAudioMuted(false);

  SimulatePlaybackStopped(0);
  EXPECT_FALSE(AreConditionsMet());
  EXPECT_FALSE(AreAudioContextConditionsMet());

  SimulateAudioVideoPlaybackStarted(0);
  EXPECT_TRUE(AreConditionsMet());
  EXPECT_FALSE(AreAudioContextConditionsMet());

  SimulateMutedStateChange(0, true);
  EXPECT_FALSE(AreConditionsMet());
  EXPECT_FALSE(AreAudioContextConditionsMet());

  SimulateSignificantVideoPlayer(1);
  EXPECT_TRUE(AreConditionsMet());
  EXPECT_FALSE(AreAudioContextConditionsMet());

  SimulateAudioContextStarted(0);
  EXPECT_TRUE(AreConditionsMet());
  EXPECT_TRUE(AreAudioContextConditionsMet());

  web_contents()->SetAudioMuted(true);
  EXPECT_FALSE(AreConditionsMet());
  EXPECT_FALSE(AreAudioContextConditionsMet());
  web_contents()->SetAudioMuted(false);

  SimulateAudioContextStopped(0);
  EXPECT_TRUE(AreConditionsMet());
  EXPECT_FALSE(AreAudioContextConditionsMet());
}

TEST_F(MediaEngagementContentsObserverTest, AreConditionsMet_AudioOnly) {
  EXPECT_FALSE(AreConditionsMet());
  EXPECT_FALSE(AreAudioContextConditionsMet());

  SimulateSignificantAudioPlayer(0);

  EXPECT_TRUE(AreConditionsMet());
  EXPECT_FALSE(AreAudioContextConditionsMet());
}

TEST_F(MediaEngagementContentsObserverTest, RecordInsignificantReason) {
  // Play the media.
  SimulateAudioVideoPlaybackStarted(0);
  SimulateResizeEvent(0, gfx::Size(1, 1));
  ExpectNotAddedFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::
          kFrameSizeTooSmall,
      1);
  ExpectNotAddedFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kCount, 1);

  // Resize the frame to full size.
  SimulateResizeEventSignificantSize(0);

  // Resize the frame size.
  SimulateResizeEvent(0, gfx::Size(1, 1));
  SimulateResizeEventSignificantSize(0);
  ExpectRemovedBucketCount(MediaEngagementContentsObserver::
                               InsignificantPlaybackReason::kFrameSizeTooSmall,
                           1);
  ExpectRemovedBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kCount, 1);

  // Pause the player.
  ExpectRemovedBucketCount(MediaEngagementContentsObserver::
                               InsignificantPlaybackReason::kMediaPaused,
                           0);
  SimulatePlaybackStopped(0);
  ExpectRemovedBucketCount(MediaEngagementContentsObserver::
                               InsignificantPlaybackReason::kMediaPaused,
                           1);
  ExpectRemovedBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kCount, 2);
  SimulateAudioVideoPlaybackStarted(0);

  // Mute the player.
  ExpectRemovedBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kAudioMuted,
      0);
  SimulateMutedStateChange(0, true);
  ExpectRemovedBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kAudioMuted,
      1);
  ExpectRemovedBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kCount, 3);

  // Start a video only player.
  ExpectNotAddedFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::
          kNoAudioTrack,
      0);
  SimulatePlaybackStarted(2, false, true);
  SimulateResizeEventSignificantSize(2);
  ExpectNotAddedFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::
          kNoAudioTrack,
      1);
  ExpectNotAddedFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kCount, 2);

  // Make sure we only record not added when we have the full state.
  SimulateAudioVideoPlaybackStarted(3);
  ExpectNotAddedFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kCount, 2);
  SimulateResizeEvent(3, gfx::Size(1, 1));
  ExpectNotAddedFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kCount, 3);

  // Make sure we only record removed when we have the full state.
  SimulateAudioVideoPlaybackStarted(4);
  SimulateMutedStateChange(4, true);
  ExpectRemovedBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kCount, 3);
  SimulateResizeEventSignificantSize(4);
  SimulateMutedStateChange(4, false);
  SimulateMutedStateChange(4, true);
  ExpectRemovedBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kCount, 4);
}

TEST_F(MediaEngagementContentsObserverTest,
       RecordInsignificantReason_NotAdded_AfterFirstTime) {
  SimulatePlaybackStarted(0, false, true);
  SimulateMutedStateChange(0, true);
  SimulateResizeEventSignificantSize(0);

  ExpectNotAddedFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::
          kNoAudioTrack,
      1);
  ExpectNotAddedFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kAudioMuted,
      1);
  ExpectNotAddedFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kCount, 1);

  SimulateMutedStateChange(0, false);

  ExpectNotAddedAfterFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::
          kNoAudioTrack,
      1);
  ExpectNotAddedAfterFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kCount, 1);

  ExpectNotAddedFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::
          kNoAudioTrack,
      1);
  ExpectNotAddedFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kCount, 1);
}

TEST_F(MediaEngagementContentsObserverTest,
       EnsureCleanupAfterNavigation_AudioContext) {
  EXPECT_FALSE(GetAudioContextPlayersCount());

  SimulateAudioContextStarted(0);
  EXPECT_TRUE(GetAudioContextPlayersCount());

  Navigate(GURL("https://example.com"));
  EXPECT_FALSE(GetAudioContextPlayersCount());
}

TEST_F(MediaEngagementContentsObserverTest,
       EnsureCleanupAfterNavigation_Media) {
  EXPECT_FALSE(GetStoredPlayerStatesCount());

  SimulateMutedStateChange(0, true);
  EXPECT_TRUE(GetStoredPlayerStatesCount());

  Navigate(GURL("https://example.com"));
  EXPECT_FALSE(GetStoredPlayerStatesCount());
}

TEST_F(MediaEngagementContentsObserverTest, TimerRunsDependingOnConditions) {
  EXPECT_FALSE(IsTimerRunning());

  SimulateSignificantVideoPlayer(0);
  EXPECT_TRUE(IsTimerRunning());

  SimulateResizeEvent(0, gfx::Size(1, 1));
  EXPECT_FALSE(IsTimerRunning());

  SimulateResizeEvent(
      0,
      gfx::Size(MediaEngagementContentsObserver::kSignificantSize.width(), 1));
  EXPECT_FALSE(IsTimerRunning());

  SimulateResizeEvent(
      0,
      gfx::Size(1, MediaEngagementContentsObserver::kSignificantSize.height()));
  EXPECT_FALSE(IsTimerRunning());
  SimulateResizeEventSignificantSize(0);

  web_contents()->SetAudioMuted(true);
  EXPECT_FALSE(IsTimerRunning());

  web_contents()->SetAudioMuted(false);
  EXPECT_TRUE(IsTimerRunning());

  SimulatePlaybackStopped(0);
  EXPECT_FALSE(IsTimerRunning());

  SimulateAudioVideoPlaybackStarted(0);
  EXPECT_TRUE(IsTimerRunning());

  SimulateMutedStateChange(0, true);
  EXPECT_FALSE(IsTimerRunning());

  SimulateSignificantVideoPlayer(1);
  EXPECT_TRUE(IsTimerRunning());
}

TEST_F(MediaEngagementContentsObserverTest,
       TimerRunsDependingOnConditions_AudioContext) {
  EXPECT_FALSE(IsAudioContextTimerRunning());

  SimulateAudioContextStarted(0);
  EXPECT_TRUE(IsAudioContextTimerRunning());

  web_contents()->SetAudioMuted(true);
  EXPECT_FALSE(IsAudioContextTimerRunning());

  web_contents()->SetAudioMuted(false);
  EXPECT_TRUE(IsAudioContextTimerRunning());

  SimulateAudioContextStopped(0);
  EXPECT_FALSE(IsAudioContextTimerRunning());
}

TEST_F(MediaEngagementContentsObserverTest,
       TimerRunsDependingOnConditions_AudioOnly) {
  EXPECT_FALSE(IsTimerRunning());

  SimulateSignificantAudioPlayer(0);
  EXPECT_TRUE(IsTimerRunning());
}

TEST_F(MediaEngagementContentsObserverTest,
       TimerDoesNotRunIfEntryRecorded_AudioContext) {
  SimulateAudible();
  SimulateSignificantAudioContextPlaybackRecorded();
  EXPECT_TRUE(WasSignificantAudioContextPlaybackRecorded());

  SimulateAudioContextStarted(0);
  EXPECT_FALSE(IsAudioContextTimerRunning());
}

TEST_F(MediaEngagementContentsObserverTest,
       TimerDoesNotRunIfEntryRecorded_Media) {
  SimulateSignificantMediaElementPlaybackRecorded();
  EXPECT_TRUE(WasSignificantPlaybackRecorded());

  SimulateSignificantVideoPlayer(0);
  EXPECT_FALSE(IsTimerRunning());
}

TEST_F(MediaEngagementContentsObserverTest,
       SignificantPlaybackRecordedWhenTimerFires_AudioContext) {
  Navigate(GURL("https://www.example.com"));
  SimulateAudioContextStarted(0);
  SimulateAudible();
  EXPECT_TRUE(IsAudioContextTimerRunning());
  EXPECT_FALSE(WasSignificantAudioContextPlaybackRecorded());

  SimulateAudioContextPlaybackTimerFired();
  EXPECT_TRUE(WasSignificantAudioContextPlaybackRecorded());
}

TEST_F(MediaEngagementContentsObserverTest,
       SignificantPlaybackRecordedWhenTimerFires_Media) {
  Navigate(GURL("https://www.example.com"));
  SimulateSignificantVideoPlayer(0);
  EXPECT_TRUE(IsTimerRunning());
  EXPECT_FALSE(WasSignificantPlaybackRecorded());

  SimulatePlaybackTimerFired();
  EXPECT_TRUE(WasSignificantPlaybackRecorded());
}

TEST_F(MediaEngagementContentsObserverTest, InteractionsRecorded) {
  url::Origin origin = url::Origin::Create(GURL("https://www.example.com"));
  url::Origin origin2 = url::Origin::Create(GURL("https://www.example.org"));
  ExpectScores(origin, 0.0, 0, 0, 0, 0, 0, 0);

  Navigate(origin.GetURL());
  Navigate(origin2.GetURL());
  ExpectScores(origin, 0.0, 1, 0, 0, 0, 0, 0);

  Navigate(origin.GetURL());
  SimulateAudible();
  SimulateSignificantMediaElementPlaybackTimeForPage();

  // We need to navigate to another page to commit the scores.
  ExpectScores(origin, 0.0, 1, 0, 0, 0, 0, 0);
  Navigate(origin2.GetURL());
  ExpectScores(origin, 0.05, 2, 1, 0, 0, 1, 0);

  // Simulate both audio context and media element on the same page.
  Navigate(origin.GetURL());
  SimulateAudible();
  SimulateAudioContextStarted(0);
  SimulateAudioContextPlaybackTimerFired();
  SimulateSignificantMediaElementPlaybackTimeForPage();

  // We need to navigate to another page to commit the scores.
  Navigate(origin2.GetURL());
  ExpectScores(origin, 0.1, 3, 2, 0, 0, 2, 1);
}

TEST_F(MediaEngagementContentsObserverTest,
       SignificantPlaybackNotRecordedIfAudioSilent) {
  SimulateAudioVideoPlaybackStarted(0);
  SimulateInaudible();
  web_contents()->SetAudioMuted(false);
  EXPECT_FALSE(IsTimerRunning());
  EXPECT_FALSE(WasSignificantPlaybackRecorded());
}

TEST_F(MediaEngagementContentsObserverTest, DoNotRecordAudiolessTrack) {
  EXPECT_EQ(0u, GetSignificantActivePlayersCount());

  content::WebContentsObserver::MediaPlayerInfo player_info(true, false);
  SimulatePlaybackStarted(player_info, 0, false);
  EXPECT_EQ(0u, GetSignificantActivePlayersCount());
}

TEST_F(MediaEngagementContentsObserverTest,
       ResetStateOnNavigationWithPlayingPlayers_AudioContext) {
  Navigate(GURL("https://www.google.com"));
  SimulateAudioContextStarted(0);
  EXPECT_TRUE(IsAudioContextTimerRunning());

  Navigate(GURL("https://www.example.com"));
  EXPECT_FALSE(GetAudioContextPlayersCount());
  EXPECT_FALSE(IsAudioContextTimerRunning());
}

TEST_F(MediaEngagementContentsObserverTest,
       ResetStateOnNavigationWithPlayingPlayers_Media) {
  Navigate(GURL("https://www.google.com"));
  SimulateSignificantVideoPlayer(0);
  ForceUpdateTimer(0);
  EXPECT_TRUE(IsTimerRunning());

  Navigate(GURL("https://www.example.com"));
  EXPECT_FALSE(GetSignificantActivePlayersCount());
  EXPECT_FALSE(GetStoredPlayerStatesCount());
  EXPECT_FALSE(IsTimerRunning());
}

TEST_F(MediaEngagementContentsObserverTest, RecordScoreOnPlayback) {
  url::Origin origin1 = url::Origin::Create(GURL("https://www.google.com"));
  url::Origin origin2 = url::Origin::Create(GURL("https://www.google.co.uk"));
  url::Origin origin3 = url::Origin::Create(GURL("https://www.example.com"));

  SetScores(origin1, 24, 20);
  SetScores(origin2, 24, 12);
  SetScores(origin3, 8, 4);
  base::HistogramTester tester;
  tester.ExpectTotalCount(
      MediaEngagementContentsObserver::kHistogramScoreAtPlaybackName, 0);

  Navigate(origin1.GetURL());
  SimulateAudioVideoPlaybackStarted(0);
  tester.ExpectBucketCount(
      MediaEngagementContentsObserver::kHistogramScoreAtPlaybackName, 83, 1);

  Navigate(origin2.GetURL());
  SimulateAudioVideoPlaybackStarted(0);
  SimulateAudioVideoPlaybackStarted(1);
  SimulateMutedStateChange(0, false);
  tester.ExpectBucketCount(
      MediaEngagementContentsObserver::kHistogramScoreAtPlaybackName, 50, 2);

  Navigate(origin3.GetURL());
  SimulateAudioVideoPlaybackStarted(0);
  tester.ExpectBucketCount(
      MediaEngagementContentsObserver::kHistogramScoreAtPlaybackName, 20, 1);
  tester.ExpectTotalCount(
      MediaEngagementContentsObserver::kHistogramScoreAtPlaybackName, 4);

  SimulateMutedStateChange(1, false);
  tester.ExpectTotalCount(
      MediaEngagementContentsObserver::kHistogramScoreAtPlaybackName, 4);

  SimulateAudioVideoPlaybackStarted(1);
  tester.ExpectTotalCount(
      MediaEngagementContentsObserver::kHistogramScoreAtPlaybackName, 5);
}

TEST_F(MediaEngagementContentsObserverTest, DoNotRecordScoreOnPlayback_Muted) {
  url::Origin origin = url::Origin::Create(GURL("https://www.google.com"));
  SetScores(origin, 24, 20);

  base::HistogramTester tester;
  Navigate(origin.GetURL());
  content::WebContentsObserver::MediaPlayerInfo player_info(true, true);
  SimulatePlaybackStarted(player_info, 0, true);
  tester.ExpectTotalCount(
      MediaEngagementContentsObserver::kHistogramScoreAtPlaybackName, 0);

  SimulateMutedStateChange(0, false);
  tester.ExpectBucketCount(
      MediaEngagementContentsObserver::kHistogramScoreAtPlaybackName, 83, 1);
}

TEST_F(MediaEngagementContentsObserverTest,
       DoNotRecordScoreOnPlayback_NoAudioTrack) {
  url::Origin origin = url::Origin::Create(GURL("https://www.google.com"));
  SetScores(origin, 6, 5);

  base::HistogramTester tester;
  Navigate(origin.GetURL());
  content::WebContentsObserver::MediaPlayerInfo player_info(true, false);
  SimulatePlaybackStarted(player_info, 0, false);
  tester.ExpectTotalCount(
      MediaEngagementContentsObserver::kHistogramScoreAtPlaybackName, 0);
}

TEST_F(MediaEngagementContentsObserverTest,
       VisibilityNotRequired_AudioContext) {
  EXPECT_FALSE(IsAudioContextTimerRunning());

  SimulateAudioContextStarted(0);
  EXPECT_TRUE(IsAudioContextTimerRunning());

  SimulateIsVisible();
  EXPECT_TRUE(IsAudioContextTimerRunning());

  SimulateIsHidden();
  EXPECT_TRUE(IsAudioContextTimerRunning());
}

TEST_F(MediaEngagementContentsObserverTest, VisibilityNotRequired_Media) {
  EXPECT_FALSE(IsTimerRunning());

  SimulateSignificantVideoPlayer(0);
  EXPECT_TRUE(IsTimerRunning());

  SimulateIsVisible();
  EXPECT_TRUE(IsTimerRunning());

  SimulateIsHidden();
  EXPECT_TRUE(IsTimerRunning());
}

TEST_F(MediaEngagementContentsObserverTest, RecordUkmMetricsOnDestroy) {
  url::Origin origin = url::Origin::Create(GURL("https://www.google.com"));
  SetScores(origin, 24, 20, 3, 1);
  Navigate(origin.GetURL());

  EXPECT_FALSE(WasSignificantPlaybackRecorded());
  SimulateSignificantVideoPlayer(0);
  SimulateSignificantMediaElementPlaybackTimeForPage();
  SimulateSignificantPlaybackTimeForPlayer(0);
  SimulateSignificantVideoPlayer(1);
  EXPECT_TRUE(WasSignificantPlaybackRecorded());

  SimulateDestroy();
  ExpectScores(origin, 21.0 / 25.0, 25, 21, 5, 2, 1, 0);
  ExpectUkmEntry(origin, 21, 25, 84, 2, 1);
}

TEST_F(MediaEngagementContentsObserverTest,
       RecordUkmMetricsOnDestroy_AudioContextOnly) {
  url::Origin origin = url::Origin::Create(GURL("https://www.google.com"));
  SetScores(origin, 24, 20, 2, 1);
  Navigate(origin.GetURL());

  EXPECT_FALSE(WasSignificantAudioContextPlaybackRecorded());
  SimulateAudioContextStarted(0);
  SimulateAudible();
  SimulateAudioContextPlaybackTimerFired();
  EXPECT_TRUE(WasSignificantAudioContextPlaybackRecorded());
  SimulateDestroy();

  // AudioContext playbacks should count as a significant playback.
  ExpectScores(origin, 21.0 / 25.0, 25, 21, 2, 1, 0, 1);
  ExpectUkmEntry(origin, 21, 25, 84, 0, 0);
}

TEST_F(MediaEngagementContentsObserverTest,
       RecordUkmMetricsOnDestroy_NoPlaybacks) {
  url::Origin origin = url::Origin::Create(GURL("https://www.google.com"));
  SetScores(origin, 24, 20, 2, 1);
  Navigate(origin.GetURL());

  EXPECT_FALSE(WasSignificantPlaybackRecorded());

  SimulateDestroy();
  ExpectScores(origin, 20.0 / 25.0, 25, 20, 2, 1, 0, 0);
  ExpectUkmEntry(origin, 20, 25, 80, 0, 0);
}

TEST_F(MediaEngagementContentsObserverTest, RecordUkmMetricsOnNavigate) {
  url::Origin origin = url::Origin::Create(GURL("https://www.google.com"));
  SetScores(origin, 24, 20, 3, 1);
  Navigate(origin.GetURL());

  EXPECT_FALSE(WasSignificantPlaybackRecorded());
  SimulateSignificantVideoPlayer(0);
  SimulateSignificantMediaElementPlaybackTimeForPage();
  SimulateSignificantPlaybackTimeForPlayer(0);
  SimulateSignificantVideoPlayer(1);
  EXPECT_TRUE(WasSignificantPlaybackRecorded());

  Navigate(GURL("https://www.example.org"));
  ExpectScores(origin, 21.0 / 25.0, 25, 21, 5, 2, 1, 0);
  ExpectUkmEntry(origin, 21, 25, 84, 2, 1);
}

TEST_F(MediaEngagementContentsObserverTest,
       RecordUkmMetricsOnNavigate_AudioContextOnly) {
  url::Origin origin = url::Origin::Create(GURL("https://www.google.com"));
  SetScores(origin, 24, 20, 2, 1);
  Navigate(origin.GetURL());

  EXPECT_FALSE(WasSignificantAudioContextPlaybackRecorded());
  SimulateAudioContextStarted(0);
  SimulateAudible();
  SimulateAudioContextPlaybackTimerFired();
  EXPECT_TRUE(WasSignificantAudioContextPlaybackRecorded());

  Navigate(GURL("https://www.example.org"));

  // AudioContext playbacks should count as a media playback.
  ExpectScores(origin, 21.0 / 25.0, 25, 21, 2, 1, 0, 1);
  ExpectUkmEntry(origin, 21, 25, 84, 0, 0);
}

TEST_F(MediaEngagementContentsObserverTest,
       RecordUkmMetricsOnNavigate_NoPlaybacks) {
  url::Origin origin = url::Origin::Create(GURL("https://www.google.com"));
  SetScores(origin, 27, 6, 2, 1);
  Navigate(origin.GetURL());

  EXPECT_FALSE(WasSignificantPlaybackRecorded());

  Navigate(GURL("https://www.example.org"));
  ExpectScores(origin, 6 / 28.0, 28, 6, 2, 1, 0, 0);
  ExpectUkmEntry(origin, 6, 28, 21, 0, 0);
}

TEST_F(MediaEngagementContentsObserverTest,
       RecordUkmMetrics_MultiplePlaybackTime) {
  url::Origin origin = url::Origin::Create(GURL("https://www.google.com"));
  SetScores(origin, 24, 20, 3, 1);
  Advance15Minutes();
  SetLastPlaybackTime(origin, Now());
  Navigate(origin.GetURL());

  Advance15Minutes();
  const base::Time first = Now();
  SimulateSignificantVideoPlayer(0);
  SimulateSignificantMediaElementPlaybackTimeForPage();
  SimulateSignificantPlaybackTimeForPlayer(0);

  Advance15Minutes();
  SimulateSignificantVideoPlayer(1);
  SimulateSignificantPlaybackTimeForPlayer(1);

  SimulateDestroy();
  ExpectScores(origin, 21.0 / 25.0, 25, 21, 5, 3, 1, 0);
  ExpectLastPlaybackTime(origin, first);
  ExpectUkmEntry(origin, 21, 25, 84, 2, 2);
}

TEST_F(MediaEngagementContentsObserverTest, DoNotCreateSessionOnInternalUrl) {
  Navigate(GURL("chrome://about"));

  // Delete recorded UKM related to the previous navigation to not have to rely
  // on how the SetUp() is made.
  test_ukm_recorder().Purge();

  EXPECT_FALSE(HasSession());

  SimulateDestroy();

  // SessionFinished UKM isn't recorded for internal URLs.
  using Entry = ukm::builders::Media_Engagement_SessionFinished;
  EXPECT_EQ(0u, test_ukm_recorder().GetEntriesByName(Entry::kEntryName).size());
}

TEST_F(MediaEngagementContentsObserverTest, RecordAudiblePlayers_OnDestroy) {
  url::Origin origin = url::Origin::Create(GURL("https://www.google.com"));
  Navigate(origin.GetURL());

  // Start three audible players and three in-audible players and also create
  // one twice.
  SimulateSignificantAudioPlayer(0);
  SimulateSignificantVideoPlayer(1);
  SimulateSignificantVideoPlayer(2);
  SimulateSignificantPlaybackTimeForPlayer(2);

  // This one is video only.
  SimulatePlaybackStarted(3, false, true);

  // This one is muted.
  SimulatePlaybackStarted(
      content::WebContentsObserver::MediaPlayerInfo(true, true), 4, true);

  // This one is stopped.
  SimulatePlaybackStopped(5);

  // Simulate significant playback time for all the players.
  SimulateSignificantMediaElementPlaybackTimeForPage();
  SimulateSignificantPlaybackTimeForPlayer(0);
  SimulateSignificantPlaybackTimeForPlayer(1);
  SimulateSignificantPlaybackTimeForPlayer(2);

  // Test that when we destroy the audible players the scores are recorded.
  SimulateDestroy();
  ExpectScores(origin, 0.05, 1, 1, 3, 3, 1, 0);
}

TEST_F(MediaEngagementContentsObserverTest, RecordAudiblePlayers_OnNavigate) {
  url::Origin origin = url::Origin::Create(GURL("https://www.google.com"));
  Navigate(origin.GetURL());

  // Start three audible players and three in-audible players and also create
  // one twice.
  SimulateSignificantAudioPlayer(0);
  SimulateSignificantVideoPlayer(1);
  SimulateSignificantVideoPlayer(2);
  SimulateSignificantVideoPlayer(2);

  // This one is video only.
  SimulatePlaybackStarted(3, false, true);

  SimulatePlaybackStarted(
      content::WebContentsObserver::MediaPlayerInfo(true, true), 4, true);

  // This one is stopped.
  SimulatePlaybackStopped(5);

  // Simulate significant playback time for all the players.
  SimulateSignificantMediaElementPlaybackTimeForPage();
  SimulateSignificantPlaybackTimeForPlayer(0);
  SimulateSignificantPlaybackTimeForPlayer(1);
  SimulateSignificantPlaybackTimeForPlayer(2);

  // Navigate to a sub page and continue watching.
  Navigate(GURL("https://www.google.com/test"));
  SimulateSignificantAudioPlayer(1);
  SimulateLongMediaPlayback(1);
  ExpectScores(origin, 0.0, 0, 0, 0, 0, 0, 0);

  // Test that when we navigate to a new origin the audible players the scores
  // are recorded.
  Navigate(GURL("https://www.google.co.uk"));
  ExpectScores(origin, 0.05, 1, 1, 4, 3, 1, 0);
}

TEST_F(MediaEngagementContentsObserverTest, TimerSpecificToPlayer) {
  url::Origin origin = url::Origin::Create(GURL("https://www.google.com"));
  Navigate(origin.GetURL());

  SimulateSignificantVideoPlayer(0);
  SimulateLongMediaPlayback(0);
  ForceUpdateTimer(1);

  SimulateDestroy();
  ExpectScores(origin, 0, 1, 0, 1, 0, 0, 0);
}

TEST_F(MediaEngagementContentsObserverTest, PagePlayerTimersDifferent) {
  SimulateSignificantVideoPlayer(0);
  SimulateSignificantVideoPlayer(1);

  EXPECT_TRUE(IsTimerRunning());
  EXPECT_TRUE(IsTimerRunningForPlayer(0));
  EXPECT_TRUE(IsTimerRunningForPlayer(1));
  EXPECT_FALSE(IsAudioContextTimerRunning());

  SimulateMutedStateChange(0, true);

  EXPECT_TRUE(IsTimerRunning());
  EXPECT_FALSE(IsTimerRunningForPlayer(0));
  EXPECT_TRUE(IsTimerRunningForPlayer(1));
  EXPECT_FALSE(IsAudioContextTimerRunning());
}

TEST_F(MediaEngagementContentsObserverTest, SignificantAudibleTabMuted_On) {
  url::Origin origin = url::Origin::Create(GURL("https://www.google.com"));
  Navigate(origin.GetURL());
  SimulateSignificantVideoPlayer(0);

  web_contents()->SetAudioMuted(true);
  SimulateSignificantPlaybackTimeForPlayer(0);

  SimulateDestroy();
  ExpectScores(origin, 0, 1, 0, 1, 0, 0, 0);
}

TEST_F(MediaEngagementContentsObserverTest, SignificantAudibleTabMuted_Off) {
  url::Origin origin = url::Origin::Create(GURL("https://www.google.com"));
  Navigate(origin.GetURL());
  SimulateSignificantVideoPlayer(0);

  SimulateSignificantPlaybackTimeForPlayer(0);

  SimulateDestroy();
  ExpectScores(origin, 0, 1, 0, 1, 1, 0, 0);
}

TEST_F(MediaEngagementContentsObserverTest, RecordPlaybackTime) {
  SimulateSignificantAudioPlayer(0);
  SimulatePlaybackStoppedWithTime(0, false, base::TimeDelta::FromSeconds(3));
  ExpectPlaybackTime(0, base::TimeDelta::FromSeconds(3));

  SimulateSignificantAudioPlayer(0);
  SimulatePlaybackStoppedWithTime(0, false, base::TimeDelta::FromSeconds(6));
  ExpectPlaybackTime(0, base::TimeDelta::FromSeconds(9));

  SimulateSignificantAudioPlayer(0);
  SimulatePlaybackStoppedWithTime(0, true, base::TimeDelta::FromSeconds(2));
  ExpectPlaybackTime(0, base::TimeDelta::FromSeconds(11));

  SimulateSignificantAudioPlayer(0);
  SimulatePlaybackStoppedWithTime(0, false, base::TimeDelta::FromSeconds(2));
  ExpectPlaybackTime(0, base::TimeDelta::FromSeconds(2));
}

TEST_F(MediaEngagementContentsObserverTest, ShortMediaIgnored) {
  url::Origin origin = url::Origin::Create(GURL("https://www.google.com"));
  Navigate(origin.GetURL());

  // Start three audible players.
  SimulateSignificantAudioPlayer(0);
  SimulateSignificantPlaybackTimeForPlayer(0);
  SimulateSignificantVideoPlayer(1);
  SimulatePlaybackStoppedWithTime(1, true, base::TimeDelta::FromSeconds(1));
  SimulateSignificantVideoPlayer(2);
  SimulateSignificantPlaybackTimeForPlayer(2);

  // Navigate to a sub page and continue watching.
  Navigate(GURL("https://www.google.com/test"));
  SimulateSignificantAudioPlayer(1);
  SimulatePlaybackStoppedWithTime(1, true, base::TimeDelta::FromSeconds(2));

  // Test that when we navigate to a new origin the audible players the scores
  // are recorded and we log extra UKM events with the times.
  Navigate(GURL("https://www.google.co.uk"));
  ExpectScores(origin, 0, 1, 0, 2, 2, 0, 0);
  ExpectUkmIgnoredEntries(origin, std::vector<int64_t>{1000, 2000});
}

TEST_F(MediaEngagementContentsObserverTest, TotalTimeUsedInShortCalculation) {
  url::Origin origin = url::Origin::Create(GURL("https://www.google.com"));
  Navigate(origin.GetURL());

  SimulateSignificantAudioPlayer(0);
  SimulatePlaybackStoppedWithTime(0, false, base::TimeDelta::FromSeconds(8));
  SimulateSignificantPlaybackTimeForPlayer(0);

  SimulateSignificantAudioPlayer(0);
  SimulatePlaybackStoppedWithTime(0, true, base::TimeDelta::FromSeconds(2));
  ExpectPlaybackTime(0, base::TimeDelta::FromSeconds(10));

  SimulateDestroy();
  ExpectScores(origin, 0, 1, 0, 1, 1, 0, 0);
  ExpectNoUkmIgnoreEntry();
}

TEST_F(MediaEngagementContentsObserverTest, OnlyIgnoreFinishedMedia) {
  url::Origin origin = url::Origin::Create(GURL("https://www.google.com"));
  Navigate(origin.GetURL());

  SimulateSignificantAudioPlayer(0);
  SimulatePlaybackStoppedWithTime(0, false, base::TimeDelta::FromSeconds(2));

  SimulateDestroy();
  ExpectScores(origin, 0, 1, 0, 1, 0, 0, 0);
  ExpectNoUkmIgnoreEntry();
}

TEST_F(MediaEngagementContentsObserverTest, GetOrCreateSession_SpecialURLs) {
  std::vector<url::Origin> origins = {
      // chrome:// and about: URLs don't use MEI.
      url::Origin::Create(GURL("about:blank")),
      url::Origin::Create(GURL("chrome://settings")),
      // Only http/https URLs use MEI, ignoring other protocals.
      url::Origin::Create(GURL("file:///tmp/")),
      url::Origin::Create(GURL("foobar://")),
  };

  for (const url::Origin& origin : origins)
    EXPECT_EQ(nullptr, GetOrCreateSession(origin, nullptr));
}

TEST_F(MediaEngagementContentsObserverTest, GetOrCreateSession_NoOpener) {
  // Regular URLs with no |opener| have a new session (non-null).
  EXPECT_NE(nullptr,
            GetOrCreateSession(url::Origin::Create(GURL("https://example.com")),
                               nullptr));
}

TEST_F(MediaEngagementContentsObserverTest, GetOrCreateSession_WithOpener) {
  const url::Origin origin =
      url::Origin::Create(GURL("https://www.example.com"));
  const url::Origin cross_origin =
      url::Origin::Create(GURL("https://second.example.com"));

  // Regular URLs with an |opener| from a different origin have a new session.
  std::unique_ptr<content::WebContents> opener(
      content::WebContentsTester::CreateTestWebContents(browser_context(),
                                                        nullptr));
  MediaEngagementContentsObserver* other_observer =
      CreateContentsObserverFor(opener.get());
  content::WebContentsTester::For(opener.get())
      ->NavigateAndCommit(cross_origin.GetURL());
  EXPECT_NE(GetSessionFor(other_observer),
            GetOrCreateSession(origin, opener.get()));

  // Same origin gets the session from the opener.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(origin.GetURL());
  content::WebContentsTester::For(opener.get())
      ->NavigateAndCommit(origin.GetURL());
  EXPECT_EQ(GetSessionFor(other_observer),
            GetOrCreateSession(origin, opener.get()));
}

TEST_F(MediaEngagementContentsObserverTest, IgnoreAudioContextIfDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(media::kRecordWebAudioEngagement);

  Navigate(GURL("https://www.example.com"));
  SimulateAudioContextStarted(0);
  SimulateAudible();

  EXPECT_FALSE(AreAudioContextConditionsMet());
  EXPECT_FALSE(IsAudioContextTimerRunning());
  EXPECT_FALSE(WasSignificantAudioContextPlaybackRecorded());

  SimulateAudioContextPlaybackTimerFired();
  EXPECT_FALSE(WasSignificantAudioContextPlaybackRecorded());
}

#endif  // !defined(OS_ANDROID)
