// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_session.h"

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
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

  MediaEngagementSessionTest()
      : origin_(url::Origin::Create(GURL("https://example.com"))) {}

  ~MediaEngagementSessionTest() override = default;

  void SetUp() override {
    service_ =
        base::WrapUnique(new MediaEngagementService(&profile_, &test_clock_));

    test_ukm_recorder_.UpdateSourceURL(ukm_source_id(), origin_.GetURL());

    // Advance the test clock to a non null value.
    test_clock_.Advance(base::Minutes(15));
  }

  MediaEngagementService* service() const { return service_.get(); }

  const url::Origin& origin() const { return origin_; }

  ukm::SourceId ukm_source_id() const { return ukm_source_id_; }

  ukm::TestAutoSetUkmRecorder& test_ukm_recorder() {
    return test_ukm_recorder_;
  }

  base::SimpleTestClock* test_clock() { return &test_clock_; }

  void SetVisitsAndPlaybacks(int visits, int media_playbacks) {
    MediaEngagementScore score = service()->CreateEngagementScore(origin());
    score.SetVisits(visits);
    score.SetMediaPlaybacks(media_playbacks);
    score.Commit();
  }

  bool ScoreIsHigh() const { return service()->HasHighEngagement(origin()); }

  void RecordPlayback(const url::Origin& origin) {
    MediaEngagementScore score = service_->CreateEngagementScore(origin);
    score.IncrementMediaPlaybacks();
    score.set_last_media_playback_time(service_->clock()->Now());
    score.Commit();
  }

 private:
  const url::Origin origin_;
  base::SimpleTestClock test_clock_;

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<MediaEngagementService> service_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
  ukm::SourceId ukm_source_id_ = ukm::SourceId(1);
};

// SmokeTest checking that IsSameOrigin actually does a same origin check.
TEST_F(MediaEngagementSessionTest, IsSameOrigin) {
  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored,
      ukm_source_id());

  std::vector<GURL> urls = {
      origin().GetURL(),           GURL("http://example.com"),
      GURL("https://example.org"), GURL(),
      GURL("http://google.com"),   GURL("http://foo.example.com"),
  };

  for (const auto& url : urls) {
    EXPECT_EQ(origin().IsSameOriginWith(url), session->IsSameOriginWith(url));
  }
}

// Checks that RecordShortPlaybackIgnored() records the right UKM.
TEST_F(MediaEngagementSessionTest, RecordShortPlaybackIgnored) {
  using Entry = ukm::builders::Media_Engagement_ShortPlaybackIgnored;
  const std::string url_string = origin().GetURL().spec();

  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored,
      ukm_source_id());

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
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored,
      ukm_source_id());

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
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored,
      ukm_source_id());

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

    auto* ukm_entry = ukm_entries[0].get();
    test_ukm_recorder().ExpectEntrySourceHasUrl(ukm_entry, origin().GetURL());
    EXPECT_EQ(2, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kPlayer_Audible_DeltaName));
    EXPECT_EQ(2, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kPlayer_Significant_DeltaName));
  }
}

// Checks that ukm_source_id_ is set after the ctor.
TEST_F(MediaEngagementSessionTest, Constructor_SetsUkmSourceId) {
  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored,
      ukm_source_id());

  EXPECT_NE(ukm::kInvalidSourceId, GetUkmSourceIdForSession(session.get()));
}

// Test that RecordSignificantAudioContextPlayback() sets the
// significant_audio_context_playback_recorded_ boolean to true.
TEST_F(MediaEngagementSessionTest,
       RecordSignificantAudioContextPlayback_SetsBoolean) {
  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored,
      ukm_source_id());

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
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored,
      ukm_source_id());

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
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored,
      ukm_source_id());

  int expected_visits = 0;
  int expected_playbacks = 0;

  {
    MediaEngagementScore score = service()->CreateEngagementScore(origin());
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
    MediaEngagementScore score = service()->CreateEngagementScore(origin());

    EXPECT_EQ(expected_visits, score.visits());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
  }
}

// Test that RecordSignificantMediaElementPlayback() records playback.
TEST_F(MediaEngagementSessionTest,
       RecordSignificantMediaElementPlayback_SetsPendingPlayback) {
  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored,
      ukm_source_id());

  int expected_visits = 0;
  int expected_playbacks = 0;

  {
    MediaEngagementScore score = service()->CreateEngagementScore(origin());
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
    MediaEngagementScore score = service()->CreateEngagementScore(origin());

    EXPECT_EQ(expected_visits, score.visits());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
  }
}

// Test that RecordSignificantAudioContextPlayback and
// RecordSignificantMediaElementPlayback() records a single playback
TEST_F(MediaEngagementSessionTest,
       RecordSignificantPlayback_SetsPendingPlayback) {
  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored,
      ukm_source_id());

  int expected_visits = 0;
  int expected_playbacks = 0;

  {
    MediaEngagementScore score = service()->CreateEngagementScore(origin());
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
    MediaEngagementScore score = service()->CreateEngagementScore(origin());

    EXPECT_EQ(expected_visits, score.visits());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
  }
}

// Test that CommitPendingData reset pending_data_to_commit_ after running.
TEST_F(MediaEngagementSessionTest, CommitPendingData_Reset) {
  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored,
      ukm_source_id());

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
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored,
      ukm_source_id());

  int expected_visits = 0;

  {
    MediaEngagementScore score = service()->CreateEngagementScore(origin());
    expected_visits = score.visits();
  }

  EXPECT_TRUE(HasPendingVisitToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  ++expected_visits;
  {
    MediaEngagementScore score = service()->CreateEngagementScore(origin());
    EXPECT_EQ(expected_visits, score.visits());
  }

  SetPendingDataToCommitForSession(session.get(), true, false, false, false);
  EXPECT_TRUE(HasPendingVisitToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  ++expected_visits;
  {
    MediaEngagementScore score = service()->CreateEngagementScore(origin());
    EXPECT_EQ(expected_visits, score.visits());
  }

  SetSignificantMediaElementPlaybackRecordedForSession(session.get(), true);
  SetPendingDataToCommitForSession(session.get(), false, true, true, true);
  EXPECT_FALSE(HasPendingVisitToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  {
    MediaEngagementScore score = service()->CreateEngagementScore(origin());
    EXPECT_EQ(expected_visits, score.visits());
  }
}

TEST_F(MediaEngagementSessionTest, CommitPendingData_UpdatePlaybackWhenNeeded) {
  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored,
      ukm_source_id());

  int expected_playbacks = 0;

  {
    MediaEngagementScore score = service()->CreateEngagementScore(origin());
    expected_playbacks = score.media_playbacks();
  }

  EXPECT_FALSE(HasPendingMediaElementPlaybackToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  {
    MediaEngagementScore score = service()->CreateEngagementScore(origin());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
  }

  SetSignificantAudioContextPlaybackRecordedForSession(session.get(), true);
  SetPendingDataToCommitForSession(session.get(), false, true, false, false);
  EXPECT_TRUE(HasPendingAudioContextPlaybackToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  ++expected_playbacks;
  {
    MediaEngagementScore score = service()->CreateEngagementScore(origin());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
  }

  SetSignificantAudioContextPlaybackRecordedForSession(session.get(), false);

  SetSignificantMediaElementPlaybackRecordedForSession(session.get(), true);
  SetPendingDataToCommitForSession(session.get(), false, false, true, false);
  EXPECT_TRUE(HasPendingMediaElementPlaybackToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  ++expected_playbacks;
  {
    MediaEngagementScore score = service()->CreateEngagementScore(origin());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
  }

  // Both significant_media_element_playback_recorded_ and pending data need to
  // be true.
  SetSignificantMediaElementPlaybackRecordedForSession(session.get(), false);
  SetPendingDataToCommitForSession(session.get(), false, false, true, false);
  EXPECT_TRUE(HasPendingMediaElementPlaybackToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  {
    MediaEngagementScore score = service()->CreateEngagementScore(origin());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
  }

  // Both significant_audio_context_playback_recorded_ and pending data need to
  // be true.
  SetSignificantAudioContextPlaybackRecordedForSession(session.get(), false);
  SetPendingDataToCommitForSession(session.get(), false, true, false, false);
  EXPECT_TRUE(HasPendingAudioContextPlaybackToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  {
    MediaEngagementScore score = service()->CreateEngagementScore(origin());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
  }

  SetSignificantMediaElementPlaybackRecordedForSession(session.get(), true);
  SetPendingDataToCommitForSession(session.get(), true, false, false, true);
  EXPECT_FALSE(HasPendingMediaElementPlaybackToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  {
    MediaEngagementScore score = service()->CreateEngagementScore(origin());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
  }

  SetSignificantMediaElementPlaybackRecordedForSession(session.get(), false);
  SetPendingDataToCommitForSession(session.get(), true, false, false, true);
  EXPECT_FALSE(HasPendingMediaElementPlaybackToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  {
    MediaEngagementScore score = service()->CreateEngagementScore(origin());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
  }
}

TEST_F(MediaEngagementSessionTest, CommitPendingData_UpdatePlayersWhenNeeded) {
  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored,
      ukm_source_id());

  int expected_media_playbacks = 0;

  {
    MediaEngagementScore score = service()->CreateEngagementScore(origin());
    expected_media_playbacks = score.media_playbacks();
  }

  EXPECT_FALSE(HasPendingPlayersToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  {
    MediaEngagementScore score = service()->CreateEngagementScore(origin());
    EXPECT_EQ(expected_media_playbacks, score.media_playbacks());
  }

  session->RegisterAudiblePlayers(0, 0);
  SetPendingDataToCommitForSession(session.get(), true, true, true, false);
  EXPECT_FALSE(HasPendingPlayersToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  {
    MediaEngagementScore score = service()->CreateEngagementScore(origin());
    EXPECT_EQ(expected_media_playbacks, score.media_playbacks());
  }

  session->RegisterAudiblePlayers(0, 0);
  SetPendingDataToCommitForSession(session.get(), false, false, false, true);
  EXPECT_TRUE(HasPendingPlayersToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  {
    MediaEngagementScore score = service()->CreateEngagementScore(origin());
    EXPECT_EQ(expected_media_playbacks, score.media_playbacks());
  }

  session->RegisterAudiblePlayers(1, 1);
  SetPendingDataToCommitForSession(session.get(), true, true, true, false);
  EXPECT_FALSE(HasPendingPlayersToCommitForSession(session.get()));
  CommitPendingDataForSession(session.get());

  {
    MediaEngagementScore score = service()->CreateEngagementScore(origin());
    EXPECT_EQ(expected_media_playbacks, score.media_playbacks());
  }
}

// SmokeTest that RecordUkmMetrics actually record UKM. The method has little to
// no logic.
TEST_F(MediaEngagementSessionTest, RecordUkmMetrics) {
  const std::string url_string = origin().GetURL().spec();
  using Entry = ukm::builders::Media_Engagement_SessionFinished;

  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kNotRestored,
      ukm_source_id());

  session->RecordSignificantMediaElementPlayback();
  CommitPendingDataForSession(session.get());

  EXPECT_EQ(0u, test_ukm_recorder().GetEntriesByName(Entry::kEntryName).size());

  RecordUkmMetricsForSession(session.get());

  {
    auto ukm_entries = test_ukm_recorder().GetEntriesByName(Entry::kEntryName);
    EXPECT_EQ(1u, ukm_entries.size());

    auto* ukm_entry = ukm_entries[0].get();
    test_ukm_recorder().ExpectEntrySourceHasUrl(ukm_entry, origin().GetURL());
    EXPECT_EQ(1, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kPlaybacks_TotalName));
    EXPECT_EQ(1, *test_ukm_recorder().GetEntryMetric(ukm_entry,
                                                     Entry::kVisits_TotalName));
    EXPECT_EQ(5, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kEngagement_ScoreName));
    EXPECT_EQ(0, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kEngagement_IsHighName));
    EXPECT_EQ(0, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kPlayer_Audible_DeltaName));
    EXPECT_EQ(0, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kPlayer_Significant_DeltaName));
  }

  session->RecordSignificantAudioContextPlayback();
  CommitPendingDataForSession(session.get());

  RecordUkmMetricsForSession(session.get());

  {
    auto ukm_entries = test_ukm_recorder().GetEntriesByName(Entry::kEntryName);
    EXPECT_EQ(2u, ukm_entries.size());

    auto* ukm_entry = ukm_entries[1].get();
    test_ukm_recorder().ExpectEntrySourceHasUrl(ukm_entry, origin().GetURL());

    EXPECT_EQ(2, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kPlaybacks_TotalName));
    EXPECT_EQ(1, *test_ukm_recorder().GetEntryMetric(ukm_entry,
                                                     Entry::kVisits_TotalName));
    EXPECT_EQ(10, *test_ukm_recorder().GetEntryMetric(
                      ukm_entry, Entry::kEngagement_ScoreName));
    EXPECT_EQ(0, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kEngagement_IsHighName));
    EXPECT_EQ(0, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kPlayer_Audible_DeltaName));
    EXPECT_EQ(0, *test_ukm_recorder().GetEntryMetric(
                     ukm_entry, Entry::kPlayer_Significant_DeltaName));
  }
}

TEST_F(MediaEngagementSessionTest, DestructorRecordMetrics) {
  using Entry = ukm::builders::Media_Engagement_SessionFinished;

  const url::Origin other_origin =
      url::Origin::Create(GURL("https://example.org"));
  const ukm::SourceId other_ukm_source_id = ukm::SourceId(2);

  EXPECT_EQ(0u, test_ukm_recorder().GetEntriesByName(Entry::kEntryName).size());

  {
    scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
        service(), origin(), MediaEngagementSession::RestoreType::kNotRestored,
        ukm_source_id());

    // |session| was destructed.
  }

  {
    auto ukm_entries = test_ukm_recorder().GetEntriesByName(Entry::kEntryName);
    EXPECT_EQ(1u, ukm_entries.size());

    test_ukm_recorder().ExpectEntrySourceHasUrl(ukm_entries[0],
                                                origin().GetURL());
  }

  {
    test_ukm_recorder().UpdateSourceURL(other_ukm_source_id,
                                        other_origin.GetURL());

    scoped_refptr<MediaEngagementSession> other_session =
        new MediaEngagementSession(
            service(), other_origin,
            MediaEngagementSession::RestoreType::kNotRestored,
            other_ukm_source_id);
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

  {
    MediaEngagementScore score = service()->CreateEngagementScore(origin());

    expected_visits = score.visits();
    expected_playbacks = score.media_playbacks();
  }

  {
    scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
        service(), origin(), MediaEngagementSession::RestoreType::kNotRestored,
        ukm_source_id());

    // |session| was destructed.
  }

  ++expected_visits;

  {
    MediaEngagementScore score = service()->CreateEngagementScore(origin());
    EXPECT_EQ(expected_visits, score.visits());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
  }

  {
    scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
        service(), origin(), MediaEngagementSession::RestoreType::kNotRestored,
        ukm_source_id());

    session->RecordSignificantMediaElementPlayback();

    // |session| was destructed.
  }

  ++expected_visits;
  ++expected_playbacks;

  {
    MediaEngagementScore score = service()->CreateEngagementScore(origin());
    EXPECT_EQ(expected_visits, score.visits());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
  }

  {
    scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
        service(), origin(), MediaEngagementSession::RestoreType::kNotRestored,
        ukm_source_id());

    session->RegisterAudiblePlayers(2, 2);

    // |session| was destructed.
  }

  ++expected_visits;

  {
    MediaEngagementScore score = service()->CreateEngagementScore(origin());
    EXPECT_EQ(expected_visits, score.visits());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
  }

  // Pretend there is nothing to commit, nothing should change.
  {
    scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
        service(), origin(), MediaEngagementSession::RestoreType::kNotRestored,
        ukm_source_id());

    SetPendingDataToCommitForSession(session.get(), false, false, false, false);

    // |session| was destructed.
  }

  {
    MediaEngagementScore score = service()->CreateEngagementScore(origin());
    EXPECT_EQ(expected_visits, score.visits());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
  }
}

TEST_F(MediaEngagementSessionTest, RestoredSession_SimpleVisitNotRecorded) {
  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kRestored,
      ukm_source_id());

  EXPECT_FALSE(HasPendingVisitToCommitForSession(session.get()));
  EXPECT_FALSE(HasPendingDataToCommitForSession(session.get()));
}

TEST_F(MediaEngagementSessionTest, RestoredSession_PlaybackRecordsVisits) {
  scoped_refptr<MediaEngagementSession> session = new MediaEngagementSession(
      service(), origin(), MediaEngagementSession::RestoreType::kRestored,
      ukm_source_id());

  int expected_visits = 0;
  int expected_playbacks = 0;

  {
    MediaEngagementScore score = service()->CreateEngagementScore(origin());
    expected_visits = score.visits() + 1;
    expected_playbacks = score.media_playbacks() + 1;
  }

  session->RecordSignificantMediaElementPlayback();
  CommitPendingDataForSession(session.get());

  {
    MediaEngagementScore score = service()->CreateEngagementScore(origin());
    EXPECT_EQ(expected_visits, score.visits());
    EXPECT_EQ(expected_playbacks, score.media_playbacks());
  }
}
