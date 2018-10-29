// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_session.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

class MediaEngagementSessionTest : public testing::Test {
 public:
  // Helper methods to call non-public methods on the MediaEngagementSession
  // class.
  static ukm::SourceId GetUkmSourceIdForSession(
      MediaEngagementSession* session) {
    return session->ukm_source_id_;
  }

  static ukm::UkmRecorder* GetUkmRecorderForSession(
      MediaEngagementSession* session) {
    return session->GetUkmRecorder();
  }

  static bool HasPendingVisitToCommitForSession(
      MediaEngagementSession* session) {
    return session->pending_data_to_commit_.visit;
  }

  static bool HasPendingAudioContextPlaybackToCommitForSession(
      MediaEngagementSession* session) {
    return session->pending_data_to_commit_.audio_context_playback;
  }

  static bool HasPendingMediaElementPlaybackToCommitForSession(
      MediaEngagementSession* session) {
    return session->pending_data_to_commit_.media_element_playback;
  }

  static bool HasPendingPlayersToCommitForSession(
      MediaEngagementSession* session) {
    return session->pending_data_to_commit_.players;
  }

  static bool HasPendingDataToCommitForSession(
      MediaEngagementSession* session) {
    return session->HasPendingDataToCommit();
  }

  static int32_t GetAudiblePlayersDeltaForSession(
      MediaEngagementSession* session) {
    return session->audible_players_delta_;
  }

  static int32_t GetSignificantPlayersDeltaForSession(
      MediaEngagementSession* session) {
    return session->significant_players_delta_;
  }

  static int32_t GetAudiblePlayersTotalForSession(
      MediaEngagementSession* session) {
    return session->audible_players_total_;
  }

  static int32_t GetSignificantPlayersTotalForSession(
      MediaEngagementSession* session) {
    return session->significant_players_total_;
  }

  static void SetPendingDataToCommitForSession(MediaEngagementSession* session,
                                               bool visit,
                                               bool audio_context_playback,
                                               bool media_element_playback,
                                               bool players) {
    session->pending_data_to_commit_ = {visit, audio_context_playback,
                                        media_element_playback, players};
  }

  static void SetSignificantAudioContextPlaybackRecordedForSession(
      MediaEngagementSession* session,
      bool value) {
    session->significant_audio_context_playback_recorded_ = value;
  }

  static void SetSignificantMediaElementPlaybackRecordedForSession(
      MediaEngagementSession* session,
      bool value) {
    session->significant_media_element_playback_recorded_ = value;
  }

  static void CommitPendingDataForSession(MediaEngagementSession* session) {
    session->CommitPendingData();
  }

  static void RecordUkmMetricsForSession(MediaEngagementSession* session) {
    session->RecordUkmMetrics();
  }

  static base::TimeDelta GetTimeSincePlaybackForSession(
      MediaEngagementSession* session) {
    return session->time_since_playback_for_ukm_;
  }

  MediaEngagementSessionTest()
      : origin_(url::Origin::Create(GURL("https://example.com"))) {}

  ~MediaEngagementSessionTest() override = default;

  void SetUp() override {
    service_ =
        base::WrapUnique(new MediaEngagementService(&profile_, &test_clock_));

    // Advance the test clock to a non null value.
    test_clock_.Advance(base::TimeDelta::FromMinutes(15));
  }

  MediaEngagementService* service() const { return service_.get(); }

  const url::Origin& origin() const { return origin_; }

  const ukm::TestAutoSetUkmRecorder& test_ukm_recorder() const {
    return test_ukm_recorder_;
  }

  base::SimpleTestClock* test_clock() { return &test_clock_; }

  void SetVisitsAndPlaybacks(int visits, int media_playbacks) {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());
    score.SetVisits(visits);
    score.SetMediaPlaybacks(media_playbacks);
    score.Commit();
  }

  bool ScoreIsHigh() const {
    return service()->HasHighEngagement(origin().GetURL());
  }

  void RecordPlayback(GURL url) {
    MediaEngagementScore score = service_->CreateEngagementScore(url);
    score.IncrementMediaPlaybacks();
    score.set_last_media_playback_time(service_->clock()->Now());
    score.Commit();
  }

 private:
  const url::Origin origin_;
  base::SimpleTestClock test_clock_;

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  std::unique_ptr<MediaEngagementService> service_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
};

// SmokeTest checking that IsSameOrigin actually does a same origin check.
TEST_F(MediaEngagementSessionTest, IsSameOrigin) {
  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored);

  std::vector<url::Origin> origins = {
      origin(),
      url::Origin::Create(GURL("http://example.com")),
      url::Origin::Create(GURL("https://example.org")),
      url::Origin(),
      url::Origin::Create(GURL("http://google.com")),
      url::Origin::Create(GURL("http://foo.example.com")),
  };

  for (const auto& orig : origins) {
    EXPECT_EQ(origin().IsSameOriginWith(orig), session->IsSameOriginWith(orig));
  }
}

// Checks that RecordShortPlaybackIgnored() records the right UKM.
TEST_F(MediaEngagementSessionTest, RecordShortPlaybackIgnored) {
  using Entry = ukm::builders::Media_Engagement_ShortPlaybackIgnored;
  const std::string url_string = origin().GetURL().spec();

  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored);

  EXPECT_EQ(0u, test_ukm_recorder().GetEntriesByName(Entry::kEntryName).size());

  session->RecordShortPlaybackIgnored(42);

  {
    auto ukm_entries = test_ukm_recorder().GetEntriesByName(Entry::kEntryName);

    EXPECT_EQ(1u, ukm_entries.size());
    test_ukm_recorder().ExpectEntrySourceHasUrl(ukm_entries[0],
                                                origin().GetURL());
    EXPECT_EQ(42, *test_ukm_recorder().GetEntryMetric(ukm_entries[0],
                                                      Entry::kLengthName));
  }

  session->RecordShortPlaybackIgnored(1337);

  {
    auto ukm_entries = test_ukm_recorder().GetEntriesByName(Entry::kEntryName);

    EXPECT_EQ(2u, ukm_entries.size());
    test_ukm_recorder().ExpectEntrySourceHasUrl(ukm_entries[1],
                                                origin().GetURL());
    EXPECT_EQ(1337, *test_ukm_recorder().GetEntryMetric(ukm_entries[1],
                                                        Entry::kLengthName));
  }
}

// Set of tests for RegisterAudiblePlayers().
TEST_F(MediaEngagementSessionTest, RegisterAudiblePlayers) {
  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored);

  // Initial checks.
  EXPECT_EQ(0, GetAudiblePlayersDeltaForSession(session.get()));
  EXPECT_EQ(0, GetSignificantPlayersDeltaForSession(session.get()));
  EXPECT_FALSE(HasPendingPlayersToCommitForSession(session.get()));

  // Registering (0,0) should be a no-op.
  {
    session->RegisterAudiblePlayers(0, 0);
    EXPECT_EQ(0, GetAudiblePlayersDeltaForSession(session.get()));
    EXPECT_EQ(0, GetSignificantPlayersDeltaForSession(session.get()));
    EXPECT_FALSE(HasPendingPlayersToCommitForSession(session.get()));
  }

  // Registering any value will trigger data to commit.
  {
    session->RegisterAudiblePlayers(1, 1);
    EXPECT_EQ(1, GetAudiblePlayersDeltaForSession(session.get()));
    EXPECT_EQ(1, GetSignificantPlayersDeltaForSession(session.get()));
    EXPECT_TRUE(HasPendingPlayersToCommitForSession(session.get()));
  }
}

TEST_F(MediaEngagementSessionTest, TotalPlayers) {
  using Entry = ukm::builders::Media_Engagement_SessionFinished;
  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored);

  // Initial checks.
  EXPECT_EQ(0, GetAudiblePlayersTotalForSession(session.get()));
  EXPECT_EQ(0, GetSignificantPlayersTotalForSession(session.get()));
  EXPECT_FALSE(HasPendingPlayersToCommitForSession(session.get()));

  // Registering players doesn't increment totals.
  session->RegisterAudiblePlayers(1, 1);
  EXPECT_EQ(0, GetAudiblePlayersTotalForSession(session.get()));
  EXPECT_EQ(0, GetSignificantPlayersTotalForSession(session.get()));

  // Commiting data increment totals.
  session->RegisterAudiblePlayers(1, 1);
  CommitPendingDataForSession(session.get());
  EXPECT_EQ(2, GetAudiblePlayersTotalForSession(session.get()));
  EXPECT_EQ(2, GetSignificantPlayersTotalForSession(session.get()));

  // Totals values have been saved to UKM as delta.
  RecordUkmMetricsForSession(session.get());
  {
    auto ukm_entries = test_ukm_recorder().GetEntriesByName(Entry::kEntryName);
    EXPECT_EQ(1u, ukm_entries.size());

    auto* ukm_entry = ukm_entries[0];
    test_ukm_recorder().ExpectEntrySourceHasUrl(ukm_entry, origin().GetURL());
    EXPECT_EQ(2, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kPlayer_Audible_DeltaName));
    EXPECT_EQ(2, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kPlayer_Significant_DeltaName));
  }
}

// Checks that ukm_source_id_ is set when GetUkmRecorder is called.
TEST_F(MediaEngagementSessionTest, GetUkmRecorder_SetsUkmSourceId) {
  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored);

  EXPECT_EQ(ukm::kInvalidSourceId, GetUkmSourceIdForSession(session.get()));

  GetUkmRecorderForSession(session.get());
  EXPECT_NE(ukm::kInvalidSourceId, GetUkmSourceIdForSession(session.get()));
}

// Checks that GetUkmRecorder() does not return nullptr.
TEST_F(MediaEngagementSessionTest, GetUkmRecorder_NotNull) {
  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored);

  EXPECT_NE(nullptr, GetUkmRecorderForSession(session.get()));
}

// Test that RecordSignificantAudioContextPlayback() sets the
// significant_audio_context_playback_recorded_ boolean to true.
TEST_F(MediaEngagementSessionTest,
       RecordSignificantAudioContextPlayback_SetsBoolean) {
  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored);

  EXPECT_FALSE(session->significant_audio_context_playback_recorded());
  EXPECT_FALSE(session->WasSignificantPlaybackRecorded());

  session->RecordSignificantAudioContextPlayback();

  EXPECT_TRUE(session->significant_audio_context_playback_recorded());
  EXPECT_TRUE(session->WasSignificantPlaybackRecorded());
}

// Test that RecordSignificantMediaElementPlayback() sets the
// significant_media_element_playback_recorded_ boolean to true.
TEST_F(MediaEngagementSessionTest,
       RecordSignificantMediaElementPlayback_SetsBoolean) {
  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored);

  EXPECT_FALSE(session->significant_media_element_playback_recorded());
  EXPECT_FALSE(session->WasSignificantPlaybackRecorded());

  session->RecordSignificantMediaElementPlayback();

  EXPECT_TRUE(session->significant_media_element_playback_recorded());
  EXPECT_TRUE(session->WasSignificantPlaybackRecorded());
}

// Test that RecordSignificantAudioContextPlayback() records playback.
TEST_F(MediaEngagementSessionTest,
       RecordSignificantAudioContextPlayback_SetsPendingPlayback) {
  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored);

  int expected_visits = 0;
  int expected_playbacks = 0;

  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());
    expected_visits = score.visits() + 1;
    expected_playbacks = score.media_playbacks() + 1;
  }

  EXPECT_FALSE(HasPendingAudioContextPlaybackToCommitForSession(session.get()));
  EXPECT_TRUE(HasPendingVisitToCommitForSession(session.get()));

  session->RecordSignificantAudioContextPlayback();
  EXPECT_TRUE(HasPendingDataToCommitForSession(session.get()));

  CommitPendingDataForSession(session.get());
  EXPECT_FALSE(HasPendingDataToCommitForSession(session.get()));

  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());

    EXPECT_EQ(expected_visits, score.visits());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
    EXPECT_EQ(expected_playbacks, score.audio_context_playbacks());
  }
}

// Test that RecordSignificantMediaElementPlayback() records playback.
TEST_F(MediaEngagementSessionTest,
       RecordSignificantMediaElementPlayback_SetsPendingPlayback) {
  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored);

  int expected_visits = 0;
  int expected_playbacks = 0;

  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());
    expected_visits = score.visits() + 1;
    expected_playbacks = score.media_playbacks() + 1;
  }

  EXPECT_FALSE(HasPendingMediaElementPlaybackToCommitForSession(session.get()));
  EXPECT_TRUE(HasPendingVisitToCommitForSession(session.get()));

  session->RecordSignificantMediaElementPlayback();
  EXPECT_TRUE(HasPendingDataToCommitForSession(session.get()));

  CommitPendingDataForSession(session.get());
  EXPECT_FALSE(HasPendingDataToCommitForSession(session.get()));

  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());

    EXPECT_EQ(expected_visits, score.visits());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
    EXPECT_EQ(expected_playbacks, score.media_element_playbacks());
  }
}

// Test that RecordSignificantAudioContextPlayback and
// RecordSignificantMediaElementPlayback() records a single playback
TEST_F(MediaEngagementSessionTest,
       RecordSignificantPlayback_SetsPendingPlayback) {
  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored);

  int expected_visits = 0;
  int expected_playbacks = 0;

  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());
    expected_visits = score.visits() + 1;
    expected_playbacks = score.media_playbacks() + 1;
  }

  EXPECT_FALSE(HasPendingAudioContextPlaybackToCommitForSession(session.get()));
  EXPECT_FALSE(HasPendingMediaElementPlaybackToCommitForSession(session.get()));
  EXPECT_TRUE(HasPendingVisitToCommitForSession(session.get()));

  session->RecordSignificantAudioContextPlayback();
  session->RecordSignificantMediaElementPlayback();
  EXPECT_TRUE(HasPendingDataToCommitForSession(session.get()));

  CommitPendingDataForSession(session.get());
  EXPECT_FALSE(HasPendingDataToCommitForSession(session.get()));

  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());

    EXPECT_EQ(expected_visits, score.visits());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
    EXPECT_EQ(expected_playbacks, score.audio_context_playbacks());
    EXPECT_EQ(expected_playbacks, score.media_element_playbacks());
  }
}

// Test that CommitPendingData reset pending_data_to_commit_ after running.
TEST_F(MediaEngagementSessionTest, CommitPendingData_Reset) {
  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored);

  EXPECT_TRUE(HasPendingDataToCommitForSession(session.get()));

  CommitPendingDataForSession(session.get());
  EXPECT_FALSE(HasPendingDataToCommitForSession(session.get()));

  SetSignificantMediaElementPlaybackRecordedForSession(session.get(), true);
  SetPendingDataToCommitForSession(session.get(), true, true, true, true);
  EXPECT_TRUE(HasPendingDataToCommitForSession(session.get()));

  CommitPendingDataForSession(session.get());
  EXPECT_FALSE(HasPendingDataToCommitForSession(session.get()));
}

// Test that CommitPendingData only update visits field when needed.
TEST_F(MediaEngagementSessionTest, CommitPendingData_UpdateVisitsAsNeeded) {
  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored);

  int expected_visits = 0;

  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());
    expected_visits = score.visits();
  }

  EXPECT_TRUE(HasPendingVisitToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  ++expected_visits;
  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());
    EXPECT_EQ(expected_visits, score.visits());
  }

  SetPendingDataToCommitForSession(session.get(), true, false, false, false);
  EXPECT_TRUE(HasPendingVisitToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  ++expected_visits;
  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());
    EXPECT_EQ(expected_visits, score.visits());
  }

  SetSignificantMediaElementPlaybackRecordedForSession(session.get(), true);
  SetPendingDataToCommitForSession(session.get(), false, true, true, true);
  EXPECT_FALSE(HasPendingVisitToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());
    EXPECT_EQ(expected_visits, score.visits());
  }
}

TEST_F(MediaEngagementSessionTest, CommitPendingData_UpdatePlaybackWhenNeeded) {
  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored);

  int expected_playbacks = 0;

  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());
    expected_playbacks = score.media_playbacks();
  }

  EXPECT_FALSE(HasPendingMediaElementPlaybackToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
  }

  SetSignificantAudioContextPlaybackRecordedForSession(session.get(), true);
  SetPendingDataToCommitForSession(session.get(), false, true, false, false);
  EXPECT_TRUE(HasPendingAudioContextPlaybackToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  ++expected_playbacks;
  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
  }

  SetSignificantAudioContextPlaybackRecordedForSession(session.get(), false);

  SetSignificantMediaElementPlaybackRecordedForSession(session.get(), true);
  SetPendingDataToCommitForSession(session.get(), false, false, true, false);
  EXPECT_TRUE(HasPendingMediaElementPlaybackToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  ++expected_playbacks;
  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
  }

  // Both significant_media_element_playback_recorded_ and pending data need to
  // be true.
  SetSignificantMediaElementPlaybackRecordedForSession(session.get(), false);
  SetPendingDataToCommitForSession(session.get(), false, false, true, false);
  EXPECT_TRUE(HasPendingMediaElementPlaybackToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
  }

  // Both significant_audio_context_playback_recorded_ and pending data need to
  // be true.
  SetSignificantAudioContextPlaybackRecordedForSession(session.get(), false);
  SetPendingDataToCommitForSession(session.get(), false, true, false, false);
  EXPECT_TRUE(HasPendingAudioContextPlaybackToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
  }

  SetSignificantMediaElementPlaybackRecordedForSession(session.get(), true);
  SetPendingDataToCommitForSession(session.get(), true, false, false, true);
  EXPECT_FALSE(HasPendingMediaElementPlaybackToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
  }

  SetSignificantMediaElementPlaybackRecordedForSession(session.get(), false);
  SetPendingDataToCommitForSession(session.get(), true, false, false, true);
  EXPECT_FALSE(HasPendingMediaElementPlaybackToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
  }
}

TEST_F(MediaEngagementSessionTest, CommitPendingData_UpdatePlayersWhenNeeded) {
  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored);

  int expected_audible_playbacks = 0;
  int expected_significant_playbacks = 0;

  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());
    expected_audible_playbacks = score.audible_playbacks();
    expected_significant_playbacks = score.significant_playbacks();
  }

  EXPECT_FALSE(HasPendingPlayersToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());
    EXPECT_EQ(expected_audible_playbacks, score.audible_playbacks());
    EXPECT_EQ(expected_significant_playbacks, score.significant_playbacks());
  }

  session->RegisterAudiblePlayers(0, 0);
  SetPendingDataToCommitForSession(session.get(), true, true, true, false);
  EXPECT_FALSE(HasPendingPlayersToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());
    EXPECT_EQ(expected_audible_playbacks, score.audible_playbacks());
    EXPECT_EQ(expected_significant_playbacks, score.significant_playbacks());
  }

  session->RegisterAudiblePlayers(0, 0);
  SetPendingDataToCommitForSession(session.get(), false, false, false, true);
  EXPECT_TRUE(HasPendingPlayersToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());
    EXPECT_EQ(expected_audible_playbacks, score.audible_playbacks());
    EXPECT_EQ(expected_significant_playbacks, score.significant_playbacks());
  }

  session->RegisterAudiblePlayers(1, 1);
  SetPendingDataToCommitForSession(session.get(), true, true, true, false);
  EXPECT_FALSE(HasPendingPlayersToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());
    EXPECT_EQ(expected_audible_playbacks, score.audible_playbacks());
    EXPECT_EQ(expected_significant_playbacks, score.significant_playbacks());
  }

  session->RegisterAudiblePlayers(0, 0);
  SetPendingDataToCommitForSession(session.get(), false, false, false, true);
  EXPECT_TRUE(HasPendingPlayersToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  ++expected_audible_playbacks;
  ++expected_significant_playbacks;
  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());
    EXPECT_EQ(expected_audible_playbacks, score.audible_playbacks());
    EXPECT_EQ(expected_significant_playbacks, score.significant_playbacks());
  }

  session->RegisterAudiblePlayers(1, 1);
  SetPendingDataToCommitForSession(session.get(), false, false, false, true);
  EXPECT_TRUE(HasPendingPlayersToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  ++expected_audible_playbacks;
  ++expected_significant_playbacks;
  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());
    EXPECT_EQ(expected_audible_playbacks, score.audible_playbacks());
    EXPECT_EQ(expected_significant_playbacks, score.significant_playbacks());
  }
}

// SmokeTest that RecordUkmMetrics actually record UKM. The method has little to
// no logic.
TEST_F(MediaEngagementSessionTest, RecordUkmMetrics) {
  const std::string url_string = origin().GetURL().spec();
  using Entry = ukm::builders::Media_Engagement_SessionFinished;

  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored);

  session->RecordSignificantMediaElementPlayback();
  CommitPendingDataForSession(session.get());

  EXPECT_EQ(0u, test_ukm_recorder().GetEntriesByName(Entry::kEntryName).size());

  RecordUkmMetricsForSession(session.get());

  {
    auto ukm_entries = test_ukm_recorder().GetEntriesByName(Entry::kEntryName);
    EXPECT_EQ(1u, ukm_entries.size());

    auto* ukm_entry = ukm_entries[0];
    test_ukm_recorder().ExpectEntrySourceHasUrl(ukm_entry, origin().GetURL());
    EXPECT_EQ(0, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kPlaybacks_AudioContextTotalName));
    EXPECT_EQ(1, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kPlaybacks_MediaElementTotalName));
    EXPECT_EQ(1, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kPlaybacks_TotalName));
    EXPECT_EQ(1, *test_ukm_recorder().GetEntryMetric(ukm_entry,
                                                     Entry::kVisits_TotalName));
    EXPECT_EQ(5, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kEngagement_ScoreName));
    EXPECT_EQ(1, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kPlaybacks_DeltaName));
    EXPECT_EQ(0, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kEngagement_IsHighName));
    EXPECT_EQ(0, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kPlayer_Audible_DeltaName));
    EXPECT_EQ(0, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kPlayer_Audible_TotalName));
    EXPECT_EQ(0, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kPlayer_Significant_DeltaName));
    EXPECT_EQ(0, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kPlayer_Significant_TotalName));
    EXPECT_EQ(0, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kPlaybacks_SecondsSinceLastName));
    EXPECT_EQ(0, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kEngagement_IsHigh_ChangesName));
    EXPECT_EQ(0, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kEngagement_IsHigh_ChangedName));
  }

  session->RecordSignificantAudioContextPlayback();
  CommitPendingDataForSession(session.get());

  RecordUkmMetricsForSession(session.get());

  {
    auto ukm_entries = test_ukm_recorder().GetEntriesByName(Entry::kEntryName);
    EXPECT_EQ(2u, ukm_entries.size());

    auto* ukm_entry = ukm_entries[1];
    test_ukm_recorder().ExpectEntrySourceHasUrl(ukm_entry, origin().GetURL());
    EXPECT_EQ(1, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kPlaybacks_AudioContextTotalName));
    EXPECT_EQ(1, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kPlaybacks_MediaElementTotalName));
    EXPECT_EQ(2, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kPlaybacks_TotalName));
    EXPECT_EQ(1, *test_ukm_recorder().GetEntryMetric(ukm_entry,
                                                     Entry::kVisits_TotalName));
    EXPECT_EQ(10, *test_ukm_recorder().GetEntryMetric(
                      ukm_entry, Entry::kEngagement_ScoreName));
    EXPECT_EQ(1, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kPlaybacks_DeltaName));
    EXPECT_EQ(0, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kEngagement_IsHighName));
    EXPECT_EQ(0, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kPlayer_Audible_DeltaName));
    EXPECT_EQ(0, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kPlayer_Audible_TotalName));
    EXPECT_EQ(0, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kPlayer_Significant_DeltaName));
    EXPECT_EQ(0, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kPlayer_Significant_TotalName));
    EXPECT_EQ(0, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kPlaybacks_SecondsSinceLastName));
    EXPECT_EQ(0, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kEngagement_IsHigh_ChangesName));
    EXPECT_EQ(0, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kEngagement_IsHigh_ChangedName));
  }
}

TEST_F(MediaEngagementSessionTest, RecordUkmMetrics_Changed_NowHigh) {
  const std::string url_string = origin().GetURL().spec();
  using Entry = ukm::builders::Media_Engagement_SessionFinished;

  // Set the visits and playbacks to just below the threshold so the next
  // significant playback will result in the playback being high.
  SetVisitsAndPlaybacks(19, 5);
  EXPECT_FALSE(ScoreIsHigh());

  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored);

  session->RecordSignificantMediaElementPlayback();
  CommitPendingDataForSession(session.get());

  EXPECT_EQ(0u, test_ukm_recorder().GetEntriesByName(Entry::kEntryName).size());

  RecordUkmMetricsForSession(session.get());

  {
    auto ukm_entries = test_ukm_recorder().GetEntriesByName(Entry::kEntryName);
    EXPECT_EQ(1u, ukm_entries.size());

    auto* ukm_entry = ukm_entries[0];
    test_ukm_recorder().ExpectEntrySourceHasUrl(ukm_entry, origin().GetURL());
    EXPECT_EQ(1u, *test_ukm_recorder().GetEntryMetric(
                      ukm_entry, Entry::kEngagement_IsHigh_ChangedName));
  }
}

TEST_F(MediaEngagementSessionTest, RecordUkmMetrics_Changed_WasHigh) {
  const std::string url_string = origin().GetURL().spec();
  using Entry = ukm::builders::Media_Engagement_SessionFinished;

  // Set the visits and playbacks to just above the lower threshold and the is
  // high bit to true so the next visit will cross the threshold.
  SetVisitsAndPlaybacks(20, 20);
  SetVisitsAndPlaybacks(20, 4);
  EXPECT_TRUE(ScoreIsHigh());

  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored);

  CommitPendingDataForSession(session.get());

  EXPECT_EQ(0u, test_ukm_recorder().GetEntriesByName(Entry::kEntryName).size());

  RecordUkmMetricsForSession(session.get());

  {
    auto ukm_entries = test_ukm_recorder().GetEntriesByName(Entry::kEntryName);
    EXPECT_EQ(1u, ukm_entries.size());

    auto* ukm_entry = ukm_entries[0];
    test_ukm_recorder().ExpectEntrySourceHasUrl(ukm_entry, origin().GetURL());
    EXPECT_EQ(1u, *test_ukm_recorder().GetEntryMetric(
                      ukm_entry, Entry::kEngagement_IsHigh_ChangedName));
  }
}

TEST_F(MediaEngagementSessionTest, DestructorRecordMetrics) {
  using Entry = ukm::builders::Media_Engagement_SessionFinished;

  const url::Origin other_origin =
      url::Origin::Create(GURL("https://example.org"));

  EXPECT_EQ(0u, test_ukm_recorder().GetEntriesByName(Entry::kEntryName).size());

  {
    scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
        service(), origin(), MediaEngagementSession::RestoreType::kNotRestored);

    // |session| was destructed.
  }

  {
    auto ukm_entries = test_ukm_recorder().GetEntriesByName(Entry::kEntryName);
    EXPECT_EQ(1u, ukm_entries.size());

    test_ukm_recorder().ExpectEntrySourceHasUrl(ukm_entries[0],
                                                origin().GetURL());
  }

  {
    scoped_refptr<MediaEngagementSession> other_session =
        new MediaEngagementSession(
            service(), other_origin,
            MediaEngagementSession::RestoreType::kNotRestored);
    // |other_session| was destructed.
  }

  {
    auto ukm_entries = test_ukm_recorder().GetEntriesByName(Entry::kEntryName);
    EXPECT_EQ(2u, ukm_entries.size());

    test_ukm_recorder().ExpectEntrySourceHasUrl(ukm_entries[0],
                                                origin().GetURL());
    test_ukm_recorder().ExpectEntrySourceHasUrl(ukm_entries[1],
                                                other_origin.GetURL());
  }
}

TEST_F(MediaEngagementSessionTest, DestructorCommitDataIfNeeded) {
  int expected_visits = 0;
  int expected_playbacks = 0;
  int expected_audible_playbacks = 0;
  int expected_significant_playbacks = 0;

  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());

    expected_visits = score.visits();
    expected_playbacks = score.media_playbacks();
    expected_audible_playbacks = score.audible_playbacks();
    expected_significant_playbacks = score.significant_playbacks();
  }

  {
    scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
        service(), origin(), MediaEngagementSession::RestoreType::kNotRestored);

    // |session| was destructed.
  }

  ++expected_visits;

  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());
    EXPECT_EQ(expected_visits, score.visits());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
    EXPECT_EQ(expected_audible_playbacks, score.audible_playbacks());
    EXPECT_EQ(expected_significant_playbacks, score.significant_playbacks());
  }

  {
    scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
        service(), origin(), MediaEngagementSession::RestoreType::kNotRestored);

    session->RecordSignificantMediaElementPlayback();

    // |session| was destructed.
  }

  ++expected_visits;
  ++expected_playbacks;

  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());
    EXPECT_EQ(expected_visits, score.visits());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
    EXPECT_EQ(expected_audible_playbacks, score.audible_playbacks());
    EXPECT_EQ(expected_significant_playbacks, score.significant_playbacks());
  }

  {
    scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
        service(), origin(), MediaEngagementSession::RestoreType::kNotRestored);

    session->RegisterAudiblePlayers(2, 2);

    // |session| was destructed.
  }

  ++expected_visits;
  expected_audible_playbacks += 2;
  expected_significant_playbacks += 2;

  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());
    EXPECT_EQ(expected_visits, score.visits());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
    EXPECT_EQ(expected_audible_playbacks, score.audible_playbacks());
    EXPECT_EQ(expected_significant_playbacks, score.significant_playbacks());
  }

  // Pretend there is nothing to commit, nothing should change.
  {
    scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
        service(), origin(), MediaEngagementSession::RestoreType::kNotRestored);

    SetPendingDataToCommitForSession(session.get(), false, false, false, false);

    // |session| was destructed.
  }

  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());
    EXPECT_EQ(expected_visits, score.visits());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
    EXPECT_EQ(expected_audible_playbacks, score.audible_playbacks());
    EXPECT_EQ(expected_significant_playbacks, score.significant_playbacks());
  }
}

// Tests that the TimeSinceLastPlayback is set to zero if there is no previous
// record.
TEST_F(MediaEngagementSessionTest, TimeSinceLastPlayback_NoPreviousRecord) {
  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored);

  EXPECT_TRUE(GetTimeSincePlaybackForSession(session.get()).is_zero());

  // Advance in time and play.
  test_clock()->Advance(base::TimeDelta::FromSeconds(42));
  session->RecordSignificantMediaElementPlayback();

  EXPECT_TRUE(GetTimeSincePlaybackForSession(session.get()).is_zero());
}

// Tests that the TimeSinceLastPlayback is set to the delta when there is a
// previous record.
TEST_F(MediaEngagementSessionTest, TimeSinceLastPlayback_PreviousRecord) {
  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored);

  EXPECT_TRUE(GetTimeSincePlaybackForSession(session.get()).is_zero());

  // Advance in time and play.
  test_clock()->Advance(base::TimeDelta::FromSeconds(42));
  RecordPlayback(origin().GetURL());

  test_clock()->Advance(base::TimeDelta::FromSeconds(42));
  session->RecordSignificantMediaElementPlayback();
  CommitPendingDataForSession(session.get());

  EXPECT_EQ(42, GetTimeSincePlaybackForSession(session.get()).InSeconds());
}

TEST_F(MediaEngagementSessionTest, RestoredSession_SimpleVisitNotRecorded) {
  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kRestored);

  EXPECT_FALSE(HasPendingVisitToCommitForSession(session.get()));
  EXPECT_FALSE(HasPendingDataToCommitForSession(session.get()));
}

TEST_F(MediaEngagementSessionTest, RestoredSession_PlaybackRecordsVisits) {
  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kRestored);

  int expected_visits = 0;
  int expected_playbacks = 0;

  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());
    expected_visits = score.visits() + 1;
    expected_playbacks = score.media_playbacks() + 1;
  }

  session->RecordSignificantMediaElementPlayback();
  CommitPendingDataForSession(session.get());

  {
    MediaEngagementScore score =
        service()->CreateEngagementScore(origin().GetURL());
    EXPECT_EQ(expected_visits, score.visits());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
  }
}

TEST_F(MediaEngagementSessionTest, StatusHistogram_NoPlayback) {
  base::HistogramTester histogram_tester;

  histogram_tester.ExpectTotalCount("Media.Engagement.Session", 0);
  histogram_tester.ExpectTotalCount("Media.Engagement.Session.Restored", 0);

  {
    scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
        service(), origin(), MediaEngagementSession::RestoreType::kNotRestored);
  }

  histogram_tester.ExpectTotalCount("Media.Engagement.Session", 1);
  histogram_tester.ExpectTotalCount("Media.Engagement.Session.Restored", 0);

  histogram_tester.ExpectBucketCount("Media.Engagement.Session", 0, 1);
}

TEST_F(MediaEngagementSessionTest, StatusHistogram_AudioContext_Playback) {
  base::HistogramTester histogram_tester;

  histogram_tester.ExpectTotalCount("Media.Engagement.Session", 0);
  histogram_tester.ExpectTotalCount("Media.Engagement.Session.Restored", 0);

  {
    scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
        service(), origin(), MediaEngagementSession::RestoreType::kNotRestored);

    session->RecordSignificantAudioContextPlayback();
  }

  histogram_tester.ExpectTotalCount("Media.Engagement.Session", 2);
  histogram_tester.ExpectTotalCount("Media.Engagement.Session.Restored", 0);

  histogram_tester.ExpectBucketCount("Media.Engagement.Session", 0, 1);
  histogram_tester.ExpectBucketCount("Media.Engagement.Session", 1, 1);
}

TEST_F(MediaEngagementSessionTest, StatusHistogram_Media_Playback) {
  base::HistogramTester histogram_tester;

  histogram_tester.ExpectTotalCount("Media.Engagement.Session", 0);
  histogram_tester.ExpectTotalCount("Media.Engagement.Session.Restored", 0);

  {
    scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
        service(), origin(), MediaEngagementSession::RestoreType::kNotRestored);

    session->RecordSignificantMediaElementPlayback();
  }

  histogram_tester.ExpectTotalCount("Media.Engagement.Session", 2);
  histogram_tester.ExpectTotalCount("Media.Engagement.Session.Restored", 0);

  histogram_tester.ExpectBucketCount("Media.Engagement.Session", 0, 1);
  histogram_tester.ExpectBucketCount("Media.Engagement.Session", 1, 1);
}

TEST_F(MediaEngagementSessionTest, StatusHistogram_Restored_NoPlayback) {
  base::HistogramTester histogram_tester;

  histogram_tester.ExpectTotalCount("Media.Engagement.Session", 0);
  histogram_tester.ExpectTotalCount("Media.Engagement.Session.Restored", 0);

  {
    scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
        service(), origin(), MediaEngagementSession::RestoreType::kRestored);
  }

  histogram_tester.ExpectTotalCount("Media.Engagement.Session", 1);
  histogram_tester.ExpectTotalCount("Media.Engagement.Session.Restored", 1);

  histogram_tester.ExpectBucketCount("Media.Engagement.Session", 0, 1);
  histogram_tester.ExpectBucketCount("Media.Engagement.Session.Restored", 0, 1);
}

TEST_F(MediaEngagementSessionTest,
       StatusHistogram_Restored_AudioContext_Playback) {
  base::HistogramTester histogram_tester;

  histogram_tester.ExpectTotalCount("Media.Engagement.Session", 0);
  histogram_tester.ExpectTotalCount("Media.Engagement.Session.Restored", 0);

  {
    scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
        service(), origin(), MediaEngagementSession::RestoreType::kRestored);

    session->RecordSignificantAudioContextPlayback();
  }

  histogram_tester.ExpectTotalCount("Media.Engagement.Session", 2);
  histogram_tester.ExpectTotalCount("Media.Engagement.Session.Restored", 2);

  histogram_tester.ExpectBucketCount("Media.Engagement.Session", 0, 1);
  histogram_tester.ExpectBucketCount("Media.Engagement.Session", 1, 1);
  histogram_tester.ExpectBucketCount("Media.Engagement.Session.Restored", 0, 1);
  histogram_tester.ExpectBucketCount("Media.Engagement.Session.Restored", 1, 1);
}

TEST_F(MediaEngagementSessionTest, StatusHistogram_Restored_Media_Playback) {
  base::HistogramTester histogram_tester;

  histogram_tester.ExpectTotalCount("Media.Engagement.Session", 0);
  histogram_tester.ExpectTotalCount("Media.Engagement.Session.Restored", 0);

  {
    scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
        service(), origin(), MediaEngagementSession::RestoreType::kRestored);

    session->RecordSignificantMediaElementPlayback();
  }

  histogram_tester.ExpectTotalCount("Media.Engagement.Session", 2);
  histogram_tester.ExpectTotalCount("Media.Engagement.Session.Restored", 2);

  histogram_tester.ExpectBucketCount("Media.Engagement.Session", 0, 1);
  histogram_tester.ExpectBucketCount("Media.Engagement.Session", 1, 1);
  histogram_tester.ExpectBucketCount("Media.Engagement.Session.Restored", 0, 1);
  histogram_tester.ExpectBucketCount("Media.Engagement.Session.Restored", 1, 1);
}
