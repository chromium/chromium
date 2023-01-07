// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_store.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/pooled_sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/media/history/media_history_images_table.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "chrome/browser/media/history/media_history_origin_table.h"
#include "chrome/browser/media/history/media_history_playback_table.h"
#include "chrome/browser/media/history/media_history_session_images_table.h"
#include "chrome/browser/media/history/media_history_session_table.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/media_player_watch_time.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/cpp/media_image.h"
#include "services/media_session/public/cpp/media_metadata.h"
#include "services/media_session/public/cpp/media_position.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media_history {

namespace {

// The error margin for double time comparison. It is 10 seconds because it
// might be equal but it might be close too.
const int kTimeErrorMargin = 10000;

enum class TestState {
  kNormal,

  // Runs the test in incognito mode.
  kIncognito,

  // Runs the test with the "SavingBrowserHistoryDisabled" policy enabled.
  kSavingBrowserHistoryDisabled,
};

}  // namespace

// Runs the test with a param to signify the profile being incognito if true.
class MediaHistoryStoreUnitTest
    : public testing::Test,
      public testing::WithParamInterface<TestState> {
 public:
  MediaHistoryStoreUnitTest() = default;
  void SetUp() override {
    // Set up the profile.
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        HistoryServiceFactory::GetInstance(),
        HistoryServiceFactory::GetDefaultFactory());
    profile_ = profile_builder.Build();

    if (GetParam() == TestState::kSavingBrowserHistoryDisabled) {
      profile_->GetPrefs()->SetBoolean(prefs::kSavingBrowserHistoryDisabled,
                                       true);
    }

    // Sleep the thread to allow the media history store to asynchronously
    // create the database and tables before proceeding with the tests and
    // tearing down the temporary directory.
    WaitForDB();

    // Set up the media history store for OTR.
    otr_service_ = std::make_unique<MediaHistoryKeyedService>(
        profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  }

  void TearDown() override {
    otr_service_->Shutdown();
    WaitForDB();
  }

  void WaitForDB() {
    base::RunLoop run_loop;

    MediaHistoryKeyedService::Get(profile_.get())
        ->PostTaskToDBForTest(run_loop.QuitClosure());

    run_loop.Run();
  }

  mojom::MediaHistoryStatsPtr GetStatsSync(MediaHistoryKeyedService* service) {
    base::RunLoop run_loop;
    mojom::MediaHistoryStatsPtr stats_out;

    service->GetMediaHistoryStats(
        base::BindLambdaForTesting([&](mojom::MediaHistoryStatsPtr stats) {
          stats_out = std::move(stats);
          run_loop.Quit();
        }));

    run_loop.Run();
    return stats_out;
  }

  std::vector<mojom::MediaHistoryOriginRowPtr> GetOriginRowsSync(
      MediaHistoryKeyedService* service) {
    base::RunLoop run_loop;
    std::vector<mojom::MediaHistoryOriginRowPtr> out;

    service->GetOriginRowsForDebug(base::BindLambdaForTesting(
        [&](std::vector<mojom::MediaHistoryOriginRowPtr> rows) {
          out = std::move(rows);
          run_loop.Quit();
        }));

    run_loop.Run();
    return out;
  }

  std::vector<mojom::MediaHistoryPlaybackRowPtr> GetPlaybackRowsSync(
      MediaHistoryKeyedService* service) {
    base::RunLoop run_loop;
    std::vector<mojom::MediaHistoryPlaybackRowPtr> out;

    service->GetMediaHistoryPlaybackRowsForDebug(base::BindLambdaForTesting(
        [&](std::vector<mojom::MediaHistoryPlaybackRowPtr> rows) {
          out = std::move(rows);
          run_loop.Quit();
        }));

    run_loop.Run();
    return out;
  }

  static std::vector<mojom::MediaHistoryPlaybackSessionRowPtr>
  GetPlaybackSessionsSync(MediaHistoryKeyedService* service, int max_sessions) {
    base::RunLoop run_loop;
    std::vector<mojom::MediaHistoryPlaybackSessionRowPtr> out;

    service->GetPlaybackSessions(
        max_sessions,
        base::BindRepeating(
            [](const base::TimeDelta& duration,
               const base::TimeDelta& position) { return true; }),
        base::BindLambdaForTesting(
            [&](std::vector<mojom::MediaHistoryPlaybackSessionRowPtr>
                    sessions) {
              out = std::move(sessions);
              run_loop.Quit();
            }));

    run_loop.Run();
    return out;
  }

  MediaHistoryKeyedService* service() const {
    // If the param is true then we use the OTR service to simulate being in
    // incognito.
    if (GetParam() == TestState::kIncognito)
      return otr_service();

    return MediaHistoryKeyedService::Get(profile_.get());
  }

  MediaHistoryKeyedService* otr_service() const { return otr_service_.get(); }

  bool IsReadOnly() const { return GetParam() != TestState::kNormal; }

  Profile* GetProfile() { return profile_.get(); }

 protected:
  // |features_| must outlive |task_environment_| to avoid TSAN issues.
  base::test::ScopedFeatureList features_;
  content::BrowserTaskEnvironment task_environment_;

 private:
  std::unique_ptr<MediaHistoryKeyedService> otr_service_;
  std::unique_ptr<TestingProfile> profile_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    MediaHistoryStoreUnitTest,
    testing::Values(TestState::kNormal,
                    TestState::kIncognito,
                    TestState::kSavingBrowserHistoryDisabled));

TEST_P(MediaHistoryStoreUnitTest, SavePlayback) {
  const auto now_before = (base::Time::Now() - base::Minutes(1)).ToJsTime();

  // Create a media player watch time and save it to the playbacks table.
  GURL url("http://google.com/test");
  content::MediaPlayerWatchTime watch_time(url, url.DeprecatedGetOriginAsURL(),
                                           base::Seconds(60), base::TimeDelta(),
                                           true, false);
  service()->SavePlayback(watch_time);
  const auto now_after_a = base::Time::Now().ToJsTime();

  // Save the watch time a second time.
  service()->SavePlayback(watch_time);

  // Wait until the playbacks have finished saving.
  WaitForDB();

  const auto now_after_b = base::Time::Now().ToJsTime();

  // Verify that the playback table contains the expected number of items.
  std::vector<mojom::MediaHistoryPlaybackRowPtr> playbacks =
      GetPlaybackRowsSync(service());

  if (IsReadOnly()) {
    EXPECT_TRUE(playbacks.empty());
  } else {
    EXPECT_EQ(2u, playbacks.size());

    EXPECT_EQ("http://google.com/test", playbacks[0]->url.spec());
    EXPECT_FALSE(playbacks[0]->has_audio);
    EXPECT_TRUE(playbacks[0]->has_video);
    EXPECT_EQ(base::Seconds(60), playbacks[0]->watchtime);
    EXPECT_LE(now_before, playbacks[0]->last_updated_time);
    EXPECT_GE(now_after_a, playbacks[0]->last_updated_time);

    EXPECT_EQ("http://google.com/test", playbacks[1]->url.spec());
    EXPECT_FALSE(playbacks[1]->has_audio);
    EXPECT_TRUE(playbacks[1]->has_video);
    EXPECT_EQ(base::Seconds(60), playbacks[1]->watchtime);
    EXPECT_LE(now_before, playbacks[1]->last_updated_time);
    EXPECT_GE(now_after_b, playbacks[1]->last_updated_time);
  }

  // Verify that the origin table contains the expected number of items.
  std::vector<mojom::MediaHistoryOriginRowPtr> origins =
      GetOriginRowsSync(service());

  if (IsReadOnly()) {
    EXPECT_TRUE(origins.empty());
  } else {
    EXPECT_EQ(1u, origins.size());
    EXPECT_EQ("http://google.com", origins[0]->origin.Serialize());
    EXPECT_LE(now_before, origins[0]->last_updated_time);
    EXPECT_GE(now_after_b, origins[0]->last_updated_time);
  }

  // The OTR service should have the same data.
  EXPECT_EQ(origins, GetOriginRowsSync(otr_service()));
  EXPECT_EQ(playbacks, GetPlaybackRowsSync(otr_service()));
}

TEST_P(MediaHistoryStoreUnitTest, SavePlayback_BadOrigin) {
  GURL url("http://google.com/test");
  GURL url2("http://google.co.uk/test");
  content::MediaPlayerWatchTime watch_time(url, url2.DeprecatedGetOriginAsURL(),
                                           base::Seconds(60), base::TimeDelta(),
                                           true, false);
  service()->SavePlayback(watch_time);

  // Verify that the origin and playbacks table are empty.
  auto origins = GetOriginRowsSync(service());
  auto playbacks = GetPlaybackRowsSync(service());

  EXPECT_TRUE(playbacks.empty());
  EXPECT_TRUE(origins.empty());
}

TEST_P(MediaHistoryStoreUnitTest, GetStats) {
  {
    // Check all the tables are empty.
    mojom::MediaHistoryStatsPtr stats = GetStatsSync(service());
    EXPECT_EQ(0, stats->table_row_counts[MediaHistoryOriginTable::kTableName]);
    EXPECT_EQ(0,
              stats->table_row_counts[MediaHistoryPlaybackTable::kTableName]);
    EXPECT_EQ(0, stats->table_row_counts[MediaHistorySessionTable::kTableName]);
    EXPECT_EQ(
        0, stats->table_row_counts[MediaHistorySessionImagesTable::kTableName]);
    EXPECT_EQ(0, stats->table_row_counts[MediaHistoryImagesTable::kTableName]);

    // The OTR service should have the same data.
    EXPECT_EQ(stats, GetStatsSync(otr_service()));
  }

  {
    // Create a media player watch time and save it to the playbacks table.
    GURL url("http://google.com/test");
    content::MediaPlayerWatchTime watch_time(
        url, url.DeprecatedGetOriginAsURL(), base::Milliseconds(123),
        base::Milliseconds(321), true, false);
    service()->SavePlayback(watch_time);
  }

  {
    // Check the tables have records in them.
    mojom::MediaHistoryStatsPtr stats = GetStatsSync(service());

    if (IsReadOnly()) {
      EXPECT_EQ(0,
                stats->table_row_counts[MediaHistoryOriginTable::kTableName]);
      EXPECT_EQ(0,
                stats->table_row_counts[MediaHistoryPlaybackTable::kTableName]);
      EXPECT_EQ(0,
                stats->table_row_counts[MediaHistorySessionTable::kTableName]);
      EXPECT_EQ(
          0,
          stats->table_row_counts[MediaHistorySessionImagesTable::kTableName]);
      EXPECT_EQ(0,
                stats->table_row_counts[MediaHistoryImagesTable::kTableName]);
    } else {
      EXPECT_EQ(1,
                stats->table_row_counts[MediaHistoryOriginTable::kTableName]);
      EXPECT_EQ(1,
                stats->table_row_counts[MediaHistoryPlaybackTable::kTableName]);
      EXPECT_EQ(0,
                stats->table_row_counts[MediaHistorySessionTable::kTableName]);
      EXPECT_EQ(
          0,
          stats->table_row_counts[MediaHistorySessionImagesTable::kTableName]);
      EXPECT_EQ(0,
                stats->table_row_counts[MediaHistoryImagesTable::kTableName]);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(stats, GetStatsSync(otr_service()));
  }
}

TEST_P(MediaHistoryStoreUnitTest, UrlShouldBeUniqueForSessions) {
  GURL url_a("https://www.google.com");
  GURL url_b("https://www.example.org");

  {
    mojom::MediaHistoryStatsPtr stats = GetStatsSync(service());
    EXPECT_EQ(0, stats->table_row_counts[MediaHistorySessionTable::kTableName]);

    // The OTR service should have the same data.
    EXPECT_EQ(stats, GetStatsSync(otr_service()));
  }

  // Save a couple of sessions on different URLs.
  service()->SavePlaybackSession(url_a, media_session::MediaMetadata(),
                                 absl::nullopt,
                                 std::vector<media_session::MediaImage>());
  service()->SavePlaybackSession(url_b, media_session::MediaMetadata(),
                                 absl::nullopt,
                                 std::vector<media_session::MediaImage>());

  // Wait until the sessions have finished saving.
  WaitForDB();

  {
    auto sessions = GetPlaybackSessionsSync(service(), 5);

    if (IsReadOnly()) {
      EXPECT_TRUE(sessions.empty());
    } else {
      EXPECT_EQ(2u, sessions.size());

      for (auto& session : sessions) {
        if (session->url == url_a)
          EXPECT_EQ(1, session->id);
      }
    }

    // The OTR service should have the same data.
    EXPECT_EQ(sessions, GetPlaybackSessionsSync(otr_service(), 5));
  }

  // Save a session on the first URL.
  service()->SavePlaybackSession(url_a, media_session::MediaMetadata(),
                                 absl::nullopt,
                                 std::vector<media_session::MediaImage>());

  // Wait until the sessions have finished saving.
  WaitForDB();

  {
    auto sessions = GetPlaybackSessionsSync(service(), 5);

    if (IsReadOnly()) {
      EXPECT_TRUE(sessions.empty());
    } else {
      EXPECT_EQ(2u, sessions.size());

      for (auto& session : sessions) {
        if (session->url == url_a)
          EXPECT_EQ(3, session->id);
      }
    }

    // The OTR service should have the same data.
    EXPECT_EQ(sessions, GetPlaybackSessionsSync(otr_service(), 5));
  }
}

TEST_P(MediaHistoryStoreUnitTest, SavePlayback_IncrementAggregateWatchtime) {
  GURL url("http://google.com/test");
  GURL url_alt("http://example.org/test");

  const auto url_now_before = base::Time::Now().ToJsTime();

  {
    // Record a watchtime for audio/video for 30 seconds.
    content::MediaPlayerWatchTime watch_time(
        url, url.DeprecatedGetOriginAsURL(), base::Seconds(30),
        base::TimeDelta(), true /* has_video */, true /* has_audio */);
    service()->SavePlayback(watch_time);
    WaitForDB();
  }

  {
    // Record a watchtime for audio/video for 60 seconds.
    content::MediaPlayerWatchTime watch_time(
        url, url.DeprecatedGetOriginAsURL(), base::Seconds(60),
        base::TimeDelta(), true /* has_video */, true /* has_audio */);
    service()->SavePlayback(watch_time);
    WaitForDB();
  }

  {
    // Record an audio-only watchtime for 30 seconds.
    content::MediaPlayerWatchTime watch_time(
        url, url.DeprecatedGetOriginAsURL(), base::Seconds(30),
        base::TimeDelta(), false /* has_video */, true /* has_audio */);
    service()->SavePlayback(watch_time);
    WaitForDB();
  }

  {
    // Record a video-only watchtime for 30 seconds.
    content::MediaPlayerWatchTime watch_time(
        url, url.DeprecatedGetOriginAsURL(), base::Seconds(30),
        base::TimeDelta(), true /* has_video */, false /* has_audio */);
    service()->SavePlayback(watch_time);
    WaitForDB();
  }

  const auto url_now_after = base::Time::Now().ToJsTime();

  {
    // Record a watchtime for audio/video for 60 seconds on a different origin.
    content::MediaPlayerWatchTime watch_time(
        url_alt, url_alt.DeprecatedGetOriginAsURL(), base::Seconds(30),
        base::TimeDelta(), true /* has_video */, true /* has_audio */);
    service()->SavePlayback(watch_time);
    WaitForDB();
  }

  const auto url_alt_after = base::Time::Now().ToJsTime();

  {
    // Check the playbacks were recorded.
    mojom::MediaHistoryStatsPtr stats = GetStatsSync(service());

    if (IsReadOnly()) {
      EXPECT_EQ(0,
                stats->table_row_counts[MediaHistoryOriginTable::kTableName]);
      EXPECT_EQ(0,
                stats->table_row_counts[MediaHistoryPlaybackTable::kTableName]);
    } else {
      EXPECT_EQ(2,
                stats->table_row_counts[MediaHistoryOriginTable::kTableName]);
      EXPECT_EQ(5,
                stats->table_row_counts[MediaHistoryPlaybackTable::kTableName]);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(stats, GetStatsSync(otr_service()));
  }

  std::vector<mojom::MediaHistoryOriginRowPtr> origins =
      GetOriginRowsSync(service());

  if (IsReadOnly()) {
    EXPECT_TRUE(origins.empty());
  } else {
    EXPECT_EQ(2u, origins.size());

    EXPECT_EQ("http://google.com", origins[0]->origin.Serialize());
    EXPECT_EQ(base::Seconds(90), origins[0]->cached_audio_video_watchtime);
    EXPECT_NEAR(url_now_before, origins[0]->last_updated_time,
                kTimeErrorMargin);
    EXPECT_GE(url_now_after, origins[0]->last_updated_time);
    EXPECT_EQ(origins[0]->cached_audio_video_watchtime,
              origins[0]->actual_audio_video_watchtime);

    EXPECT_EQ("http://example.org", origins[1]->origin.Serialize());
    EXPECT_EQ(base::Seconds(30), origins[1]->cached_audio_video_watchtime);
    EXPECT_NEAR(url_now_before, origins[1]->last_updated_time,
                kTimeErrorMargin);
    EXPECT_GE(url_alt_after, origins[1]->last_updated_time);
    EXPECT_EQ(origins[1]->cached_audio_video_watchtime,
              origins[1]->actual_audio_video_watchtime);
  }

  // The OTR service should have the same data.
  EXPECT_EQ(origins, GetOriginRowsSync(otr_service()));
}

TEST_P(MediaHistoryStoreUnitTest, GetOriginsWithHighWatchTime) {
  const GURL url("http://google.com/test");
  const GURL url_alt("http://example.org/test");
  const base::TimeDelta min_watch_time = base::Minutes(30);

  {
    // Record a watch time that isn't high enough to get with our request.
    content::MediaPlayerWatchTime watch_time(
        url, url.DeprecatedGetOriginAsURL(), min_watch_time - base::Seconds(1),
        base::TimeDelta(), true /* has_video */, true /* has_audio */);
    service()->SavePlayback(watch_time);
    WaitForDB();
  }

  {
    // Record a watchtime that we should get with our request.
    content::MediaPlayerWatchTime watch_time(
        url_alt, url_alt.DeprecatedGetOriginAsURL(), min_watch_time,
        base::TimeDelta(), true /* has_video */, true /* has_audio */);
    service()->SavePlayback(watch_time);
    WaitForDB();
  }

  base::RunLoop run_loop;
  std::vector<url::Origin> out;

  service()->GetHighWatchTimeOrigins(
      min_watch_time,
      base::BindLambdaForTesting([&](const std::vector<url::Origin>& origins) {
        out = std::move(origins);
        run_loop.Quit();
      }));

  run_loop.Run();

  if (IsReadOnly()) {
    EXPECT_TRUE(out.empty());
  } else {
    std::vector<url::Origin> expected = {url::Origin::Create(url_alt)};
    EXPECT_EQ(out, expected);
  }
}

}  // namespace media_history
