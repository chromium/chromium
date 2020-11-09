// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_store.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/pooled_sequenced_task_runner.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom-forward.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom.h"
#include "chrome/browser/media/history/media_history_feed_items_table.h"
#include "chrome/browser/media/history/media_history_feeds_table.h"
#include "chrome/browser/media/history/media_history_images_table.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "chrome/browser/media/history/media_history_origin_table.h"
#include "chrome/browser/media/history/media_history_playback_table.h"
#include "chrome/browser/media/history/media_history_session_images_table.h"
#include "chrome/browser/media/history/media_history_session_table.h"
#include "chrome/browser/media/history/media_history_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/common/pref_names.h"
#include "components/history/core/test/test_history_database.h"
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

#if !defined(OS_ANDROID)
#include "chrome/browser/media/feeds/media_feeds_service.h"
#include "chrome/browser/media/feeds/media_feeds_service_factory.h"
#endif

namespace media_history {

namespace {

// The error margin for double time comparison. It is 10 seconds because it
// might be equal but it might be close too.
const int kTimeErrorMargin = 10000;

#if !defined(OS_ANDROID)

// The expected display name for the fetched media feed.
const char kExpectedDisplayName[] = "Test Feed";

// The expected counts and content types for the test feed.
const int kExpectedFetchItemCount = 3;
const int kExpectedFetchPlayNextCount = 2;
const int kExpectedFetchContentTypes =
    static_cast<int>(media_feeds::mojom::MediaFeedItemType::kMovie) |
    static_cast<int>(media_feeds::mojom::MediaFeedItemType::kTVSeries);

// The expected counts and content types for the alternate test feed.
const int kExpectedAltFetchItemCount = 1;
const int kExpectedAltFetchPlayNextCount = 1;
const int kExpectedAltFetchContentTypes =
    static_cast<int>(media_feeds::mojom::MediaFeedItemType::kVideo);

#endif  // !defined(OS_ANDROID)

base::FilePath g_temp_history_dir;

std::unique_ptr<KeyedService> BuildTestHistoryService(
    content::BrowserContext* context) {
  std::unique_ptr<history::HistoryService> service(
      new history::HistoryService());
  service->Init(history::TestHistoryDatabaseParamsForPath(g_temp_history_dir));
  return service;
}

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
    base::HistogramTester histogram_tester;

    // Set up the profile.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetPath(temp_dir_.GetPath());
    g_temp_history_dir = temp_dir_.GetPath();
    profile_ = profile_builder.Build();

    if (GetParam() == TestState::kSavingBrowserHistoryDisabled) {
      profile_->GetPrefs()->SetBoolean(prefs::kSavingBrowserHistoryDisabled,
                                       true);
    }

    HistoryServiceFactory::GetInstance()->SetTestingFactory(
        profile_.get(), base::BindRepeating(&BuildTestHistoryService));

    // Sleep the thread to allow the media history store to asynchronously
    // create the database and tables before proceeding with the tests and
    // tearing down the temporary directory.
    WaitForDB();

    histogram_tester.ExpectBucketCount(
        MediaHistoryStore::kInitResultHistogramName,
        MediaHistoryStore::InitResult::kSuccess, 1);

    // Set up the media history store for OTR.
    otr_service_ = std::make_unique<MediaHistoryKeyedService>(
        profile_->GetPrimaryOTRProfile());
  }

  void TearDown() override { WaitForDB(); }

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

  media::mojom::GetCollectionsResponsePtr GetKaleidoscopeDataSync(
      MediaHistoryKeyedService* service,
      const std::string& gaia_id) {
    base::RunLoop run_loop;
    media::mojom::GetCollectionsResponsePtr out;

    service->GetKaleidoscopeData(
        gaia_id, base::BindLambdaForTesting(
                     [&](media::mojom::GetCollectionsResponsePtr data) {
                       out = std::move(data);
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

  std::vector<media_feeds::mojom::MediaFeedPtr> GetMediaFeedsSync(
      MediaHistoryKeyedService* service,
      const MediaHistoryKeyedService::GetMediaFeedsRequest& request =
          MediaHistoryKeyedService::GetMediaFeedsRequest()) {
    base::RunLoop run_loop;
    std::vector<media_feeds::mojom::MediaFeedPtr> out;

    service->GetMediaFeeds(
        request, base::BindLambdaForTesting(
                     [&](std::vector<media_feeds::mojom::MediaFeedPtr> rows) {
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

  media_history::MediaHistoryKeyedService::MediaFeedFetchResult ResultWithItems(
      const int64_t feed_id,
      std::vector<media_feeds::mojom::MediaFeedItemPtr> items,
      media_feeds::mojom::FetchResult fetch_result) {
    media_history::MediaHistoryKeyedService::MediaFeedFetchResult result;
    result.feed_id = feed_id;
    result.items = std::move(items);
    result.status = fetch_result;
    result.reset_token = test::GetResetTokenSync(service(), feed_id);
    return result;
  }

  media_history::MediaHistoryKeyedService::MediaFeedFetchResult
  SuccessfulResultWithItems(
      const int64_t feed_id,
      std::vector<media_feeds::mojom::MediaFeedItemPtr> items) {
    return ResultWithItems(feed_id, std::move(items),
                           media_feeds::mojom::FetchResult::kSuccess);
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

  media::mojom::GetCollectionsResponsePtr GetExpectedKaleidoscopeData() {
    auto data = media::mojom::GetCollectionsResponse::New();
    data->response = "abcd";
    data->result = media::mojom::GetCollectionsResult::kFailed;
    return data;
  }

 private:
  base::ScopedTempDir temp_dir_;

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
  base::HistogramTester histogram_tester;

  const auto now_before =
      (base::Time::Now() - base::TimeDelta::FromMinutes(1)).ToJsTime();

  // Create a media player watch time and save it to the playbacks table.
  GURL url("http://google.com/test");
  content::MediaPlayerWatchTime watch_time(url, url.GetOrigin(),
                                           base::TimeDelta::FromSeconds(60),
                                           base::TimeDelta(), true, false);
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
    EXPECT_EQ(base::TimeDelta::FromSeconds(60), playbacks[0]->watchtime);
    EXPECT_LE(now_before, playbacks[0]->last_updated_time);
    EXPECT_GE(now_after_a, playbacks[0]->last_updated_time);

    EXPECT_EQ("http://google.com/test", playbacks[1]->url.spec());
    EXPECT_FALSE(playbacks[1]->has_audio);
    EXPECT_TRUE(playbacks[1]->has_video);
    EXPECT_EQ(base::TimeDelta::FromSeconds(60), playbacks[1]->watchtime);
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

  histogram_tester.ExpectBucketCount(
      MediaHistoryStore::kPlaybackWriteResultHistogramName,
      MediaHistoryStore::PlaybackWriteResult::kSuccess, IsReadOnly() ? 0 : 2);
}

TEST_P(MediaHistoryStoreUnitTest, SavePlayback_BadOrigin) {
  GURL url("http://google.com/test");
  GURL url2("http://google.co.uk/test");
  content::MediaPlayerWatchTime watch_time(url, url2.GetOrigin(),
                                           base::TimeDelta::FromSeconds(60),
                                           base::TimeDelta(), true, false);
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
        url, url.GetOrigin(), base::TimeDelta::FromMilliseconds(123),
        base::TimeDelta::FromMilliseconds(321), true, false);
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
  base::HistogramTester histogram_tester;

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
                                 base::nullopt,
                                 std::vector<media_session::MediaImage>());
  service()->SavePlaybackSession(url_b, media_session::MediaMetadata(),
                                 base::nullopt,
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
                                 base::nullopt,
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

  histogram_tester.ExpectBucketCount(
      MediaHistoryStore::kSessionWriteResultHistogramName,
      MediaHistoryStore::SessionWriteResult::kSuccess, IsReadOnly() ? 0 : 3);
}

TEST_P(MediaHistoryStoreUnitTest, SavePlayback_IncrementAggregateWatchtime) {
  GURL url("http://google.com/test");
  GURL url_alt("http://example.org/test");

  const auto url_now_before = base::Time::Now().ToJsTime();

  {
    // Record a watchtime for audio/video for 30 seconds.
    content::MediaPlayerWatchTime watch_time(
        url, url.GetOrigin(), base::TimeDelta::FromSeconds(30),
        base::TimeDelta(), true /* has_video */, true /* has_audio */);
    service()->SavePlayback(watch_time);
    WaitForDB();
  }

  {
    // Record a watchtime for audio/video for 60 seconds.
    content::MediaPlayerWatchTime watch_time(
        url, url.GetOrigin(), base::TimeDelta::FromSeconds(60),
        base::TimeDelta(), true /* has_video */, true /* has_audio */);
    service()->SavePlayback(watch_time);
    WaitForDB();
  }

  {
    // Record an audio-only watchtime for 30 seconds.
    content::MediaPlayerWatchTime watch_time(
        url, url.GetOrigin(), base::TimeDelta::FromSeconds(30),
        base::TimeDelta(), false /* has_video */, true /* has_audio */);
    service()->SavePlayback(watch_time);
    WaitForDB();
  }

  {
    // Record a video-only watchtime for 30 seconds.
    content::MediaPlayerWatchTime watch_time(
        url, url.GetOrigin(), base::TimeDelta::FromSeconds(30),
        base::TimeDelta(), true /* has_video */, false /* has_audio */);
    service()->SavePlayback(watch_time);
    WaitForDB();
  }

  const auto url_now_after = base::Time::Now().ToJsTime();

  {
    // Record a watchtime for audio/video for 60 seconds on a different origin.
    content::MediaPlayerWatchTime watch_time(
        url_alt, url_alt.GetOrigin(), base::TimeDelta::FromSeconds(30),
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
    EXPECT_EQ(base::TimeDelta::FromSeconds(90),
              origins[0]->cached_audio_video_watchtime);
    EXPECT_NEAR(url_now_before, origins[0]->last_updated_time,
                kTimeErrorMargin);
    EXPECT_GE(url_now_after, origins[0]->last_updated_time);
    EXPECT_EQ(origins[0]->cached_audio_video_watchtime,
              origins[0]->actual_audio_video_watchtime);

    EXPECT_EQ("http://example.org", origins[1]->origin.Serialize());
    EXPECT_EQ(base::TimeDelta::FromSeconds(30),
              origins[1]->cached_audio_video_watchtime);
    EXPECT_NEAR(url_now_before, origins[1]->last_updated_time,
                kTimeErrorMargin);
    EXPECT_GE(url_alt_after, origins[1]->last_updated_time);
    EXPECT_EQ(origins[1]->cached_audio_video_watchtime,
              origins[1]->actual_audio_video_watchtime);
  }

  // The OTR service should have the same data.
  EXPECT_EQ(origins, GetOriginRowsSync(otr_service()));
}

TEST_P(MediaHistoryStoreUnitTest, KaleidoscopeData) {
  {
    // The data should be empty at the start.
    auto data = GetKaleidoscopeDataSync(service(), "123");
    EXPECT_TRUE(data.is_null());
  }

  service()->SetKaleidoscopeData(GetExpectedKaleidoscopeData(), "123");
  WaitForDB();

  {
    // We should be able to get the data.
    auto data = GetKaleidoscopeDataSync(service(), "123");

    if (IsReadOnly()) {
      EXPECT_TRUE(data.is_null());
    } else {
      EXPECT_EQ(GetExpectedKaleidoscopeData(), data);
    }
  }

  {
    // Getting with a different GAIA ID should wipe the data and return an
    // empty string.
    auto data = GetKaleidoscopeDataSync(service(), "1234");
    EXPECT_TRUE(data.is_null());
  }

  {
    // The data should be empty for the other GAIA ID too.
    auto data = GetKaleidoscopeDataSync(service(), "123");
    EXPECT_TRUE(data.is_null());
  }

  service()->SetKaleidoscopeData(GetExpectedKaleidoscopeData(), "123");
  WaitForDB();

  {
    // We should be able to get the data.
    auto data = GetKaleidoscopeDataSync(service(), "123");

    if (IsReadOnly()) {
      EXPECT_TRUE(data.is_null());
    } else {
      EXPECT_EQ(GetExpectedKaleidoscopeData(), data);
    }
  }

  service()->DeleteKaleidoscopeData();
  WaitForDB();

  {
    // The data should have been deleted.
    auto data = GetKaleidoscopeDataSync(service(), "123");
    EXPECT_TRUE(data.is_null());
  }
}

TEST_P(MediaHistoryStoreUnitTest, GetOriginsWithHighWatchTime) {
  const GURL url("http://google.com/test");
  const GURL url_alt("http://example.org/test");
  const base::TimeDelta min_watch_time = base::TimeDelta::FromMinutes(30);

  {
    // Record a watch time that isn't high enough to get with our request.
    content::MediaPlayerWatchTime watch_time(
        url, url.GetOrigin(), min_watch_time - base::TimeDelta::FromSeconds(1),
        base::TimeDelta(), true /* has_video */, true /* has_audio */);
    service()->SavePlayback(watch_time);
    WaitForDB();
  }

  {
    // Record a watchtime that we should get with our request.
    content::MediaPlayerWatchTime watch_time(
        url_alt, url_alt.GetOrigin(), min_watch_time, base::TimeDelta(),
        true /* has_video */, true /* has_audio */);
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

#if !defined(OS_ANDROID)

// Runs the tests with the media feeds feature enabled.
class MediaHistoryStoreFeedsTest : public MediaHistoryStoreUnitTest {
 public:
  void SetUp() override {
    features_.InitAndEnableFeature(media::kMediaFeeds);
    MediaHistoryStoreUnitTest::SetUp();
  }

  void DiscoverMediaFeed(const GURL& url) {
    if (auto* service = GetMediaFeedsService())
      service->DiscoverMediaFeed(url);
  }

  media_feeds::MediaFeedsService* GetMediaFeedsService() {
    Profile* profile = GetProfile();
    if (GetParam() == TestState::kIncognito)
      profile = profile->GetPrimaryOTRProfile();

    return media_feeds::MediaFeedsServiceFactory::GetInstance()->GetForProfile(
        profile);
  }

  std::vector<media_feeds::mojom::MediaFeedItemPtr> GetItemsForMediaFeedSync(
      MediaHistoryKeyedService* service,
      const int64_t feed_id) {
    return GetItemsForMediaFeedSync(
        service,
        MediaHistoryKeyedService::GetMediaFeedItemsRequest::CreateItemsForDebug(
            feed_id));
  }

  std::vector<media_feeds::mojom::MediaFeedItemPtr> GetItemsForMediaFeedSync(
      MediaHistoryKeyedService* service,
      MediaHistoryKeyedService::GetMediaFeedItemsRequest request) {
    base::RunLoop run_loop;
    std::vector<media_feeds::mojom::MediaFeedItemPtr> out;

    service->GetMediaFeedItems(
        request,
        base::BindLambdaForTesting(
            [&](std::vector<media_feeds::mojom::MediaFeedItemPtr> rows) {
              out = std::move(rows);
              run_loop.Quit();
            }));

    run_loop.Run();
    return out;
  }

  MediaHistoryKeyedService::PendingSafeSearchCheckList
  GetPendingSafeSearchCheckMediaFeedItemsSync(
      MediaHistoryKeyedService* service) {
    base::RunLoop run_loop;
    MediaHistoryKeyedService::PendingSafeSearchCheckList out;

    service->GetPendingSafeSearchCheckMediaFeedItems(base::BindLambdaForTesting(
        [&](MediaHistoryKeyedService::PendingSafeSearchCheckList rows) {
          out = std::move(rows);
          run_loop.Quit();
        }));

    run_loop.Run();
    return out;
  }

  static media_feeds::mojom::ContentRatingPtr CreateRating(
      const std::string& agency,
      const std::string& value) {
    auto rating = media_feeds::mojom::ContentRating::New();
    rating->agency = agency;
    rating->value = value;
    return rating;
  }

  static media_feeds::mojom::IdentifierPtr CreateIdentifier(
      const media_feeds::mojom::Identifier::Type& type,
      const std::string& value) {
    auto identifier = media_feeds::mojom::Identifier::New();
    identifier->type = type;
    identifier->value = value;
    return identifier;
  }

  static std::vector<media_feeds::mojom::MediaFeedItemPtr> GetExpectedItems(
      int id_start = 0) {
    std::vector<media_feeds::mojom::MediaFeedItemPtr> items;

    {
      auto item = media_feeds::mojom::MediaFeedItem::New();
      item->id = ++id_start;
      item->name = base::ASCIIToUTF16("The Movie");
      item->type = media_feeds::mojom::MediaFeedItemType::kMovie;
      item->date_published = base::Time::FromDeltaSinceWindowsEpoch(
          base::TimeDelta::FromMinutes(10));
      item->is_family_friendly = media_feeds::mojom::IsFamilyFriendly::kYes;
      item->action_status =
          media_feeds::mojom::MediaFeedItemActionStatus::kPotential;
      item->genre.push_back("test");
      item->duration = base::TimeDelta::FromSeconds(30);
      item->live = media_feeds::mojom::LiveDetails::New();
      item->live->start_time = base::Time::FromDeltaSinceWindowsEpoch(
          base::TimeDelta::FromMinutes(20));
      item->live->end_time = base::Time::FromDeltaSinceWindowsEpoch(
          base::TimeDelta::FromMinutes(30));
      item->shown_count = 3;
      item->clicked = true;
      item->author = media_feeds::mojom::Author::New();
      item->author->name = "Media Site";
      item->author->url = GURL("https://www.example.com/author");
      item->action = media_feeds::mojom::Action::New();
      item->action->start_time = base::TimeDelta::FromSeconds(3);
      item->action->url = GURL("https://www.example.com/action");
      item->interaction_counters.emplace(
          media_feeds::mojom::InteractionCounterType::kLike, 10000);
      item->interaction_counters.emplace(
          media_feeds::mojom::InteractionCounterType::kDislike, 20000);
      item->interaction_counters.emplace(
          media_feeds::mojom::InteractionCounterType::kWatch, 30000);
      item->content_ratings.push_back(CreateRating("MPAA", "PG-13"));
      item->content_ratings.push_back(CreateRating("agency", "TEST2"));
      item->identifiers.push_back(CreateIdentifier(
          media_feeds::mojom::Identifier::Type::kPartnerId, "TEST1"));
      item->identifiers.push_back(CreateIdentifier(
          media_feeds::mojom::Identifier::Type::kTMSId, "TEST2"));
      item->tv_episode = media_feeds::mojom::TVEpisode::New();
      item->tv_episode->name = "TV Episode Name";
      item->tv_episode->season_number = 1;
      item->tv_episode->episode_number = 2;
      item->tv_episode->identifiers.push_back(CreateIdentifier(
          media_feeds::mojom::Identifier::Type::kTMSId, "TEST3"));
      item->tv_episode->duration = base::TimeDelta::FromMinutes(40);
      item->tv_episode->live = media_feeds::mojom::LiveDetails::New();
      item->tv_episode->live->start_time =
          base::Time::FromDeltaSinceWindowsEpoch(
              base::TimeDelta::FromSeconds(15));
      item->play_next_candidate = media_feeds::mojom::PlayNextCandidate::New();
      item->play_next_candidate->name = "Next TV Episode Name";
      item->play_next_candidate->season_number = 1;
      item->play_next_candidate->episode_number = 3;
      item->play_next_candidate->duration = base::TimeDelta::FromMinutes(20);
      item->play_next_candidate->action = media_feeds::mojom::Action::New();
      item->play_next_candidate->action->start_time =
          base::TimeDelta::FromSeconds(3);
      item->play_next_candidate->action->url =
          GURL("https://www.example.com/next");
      item->play_next_candidate->identifiers.push_back(CreateIdentifier(
          media_feeds::mojom::Identifier::Type::kTMSId, "TEST4"));
      item->safe_search_result = media_feeds::mojom::SafeSearchResult::kUnknown;

      {
        media_feeds::mojom::MediaImagePtr image =
            media_feeds::mojom::MediaImage::New();
        image->src = GURL("https://www.example.org/image1.png");
        item->images.push_back(std::move(image));
      }

      {
        media_feeds::mojom::MediaImagePtr image =
            media_feeds::mojom::MediaImage::New();
        image->src = GURL("https://www.example.org/image2.png");
        image->size = gfx::Size(10, 10);
        item->images.push_back(std::move(image));
      }

      {
        media_feeds::mojom::MediaImagePtr image =
            media_feeds::mojom::MediaImage::New();
        image->src = GURL("https://www.example.org/episode-image.png");
        item->tv_episode->images.push_back(std::move(image));
      }

      {
        media_feeds::mojom::MediaImagePtr image =
            media_feeds::mojom::MediaImage::New();
        image->src = GURL("https://www.example.org/next-image.png");
        item->play_next_candidate->images.push_back(std::move(image));
      }

      items.push_back(std::move(item));
    }

    {
      auto item = media_feeds::mojom::MediaFeedItem::New();
      item->id = ++id_start;
      item->type = media_feeds::mojom::MediaFeedItemType::kTVSeries;
      item->name = base::ASCIIToUTF16("The TV Series");
      item->action_status =
          media_feeds::mojom::MediaFeedItemActionStatus::kActive;
      item->action = media_feeds::mojom::Action::New();
      item->action->url = GURL("https://www.example.com/action2");
      item->author = media_feeds::mojom::Author::New();
      item->author->name = "Media Site";
      item->safe_search_result = media_feeds::mojom::SafeSearchResult::kSafe;
      items.push_back(std::move(item));
    }

    {
      auto item = media_feeds::mojom::MediaFeedItem::New();
      item->id = ++id_start;
      item->type = media_feeds::mojom::MediaFeedItemType::kTVSeries;
      item->name = base::ASCIIToUTF16("The Live TV Series");
      item->action_status =
          media_feeds::mojom::MediaFeedItemActionStatus::kPotential;
      item->live = media_feeds::mojom::LiveDetails::New();
      item->live->start_time = base::Time::FromDeltaSinceWindowsEpoch(
          base::TimeDelta::FromSeconds(30));
      item->safe_search_result = media_feeds::mojom::SafeSearchResult::kUnsafe;
      items.push_back(std::move(item));
    }

    return items;
  }

  static std::vector<media_feeds::mojom::MediaFeedItemPtr> GetAltExpectedItems(
      int id_start = 0) {
    std::vector<media_feeds::mojom::MediaFeedItemPtr> items;

    {
      auto item = media_feeds::mojom::MediaFeedItem::New();
      item->id = ++id_start;
      item->type = media_feeds::mojom::MediaFeedItemType::kVideo;
      item->name = base::ASCIIToUTF16("The Video");
      item->date_published = base::Time::FromDeltaSinceWindowsEpoch(
          base::TimeDelta::FromMinutes(20));
      item->is_family_friendly = media_feeds::mojom::IsFamilyFriendly::kNo;
      item->action_status =
          media_feeds::mojom::MediaFeedItemActionStatus::kActive;
      item->action = media_feeds::mojom::Action::New();
      item->action->url = GURL("https://www.example.com/action-alt");
      item->safe_search_result = media_feeds::mojom::SafeSearchResult::kUnknown;
      items.push_back(std::move(item));
    }

    return items;
  }

  static std::vector<media_feeds::mojom::MediaImagePtr> GetExpectedLogos() {
    std::vector<media_feeds::mojom::MediaImagePtr> logos;

    {
      media_feeds::mojom::MediaImagePtr image =
          media_feeds::mojom::MediaImage::New();
      image->src = GURL("https://www.example.org/image1.png");
      image->size = gfx::Size(10, 10);
      logos.push_back(std::move(image));
    }

    {
      media_feeds::mojom::MediaImagePtr image =
          media_feeds::mojom::MediaImage::New();
      image->src = GURL("https://www.example.org/image2.png");
      logos.push_back(std::move(image));
    }

    return logos;
  }

  static media_feeds::mojom::UserIdentifierPtr GetExpectedUserIdentifier() {
    auto identifier = media_feeds::mojom::UserIdentifier::New();

    identifier->name = "Becca Hughes";
    identifier->email = "test@chromium.org";

    identifier->image = media_feeds::mojom::MediaImage::New();
    identifier->image->src = GURL("https://www.example.org/image1.png");
    identifier->image->size = gfx::Size(10, 10);

    return identifier;
  }

  base::Optional<media_history::MediaHistoryKeyedService::MediaFeedFetchDetails>
  GetFetchDetailsSync(const int64_t feed_id) {
    base::RunLoop run_loop;
    base::Optional<
        media_history::MediaHistoryKeyedService::MediaFeedFetchDetails>
        out;

    service()->GetMediaFeedFetchDetails(
        feed_id,
        base::BindLambdaForTesting(
            [&](base::Optional<
                media_history::MediaHistoryKeyedService::MediaFeedFetchDetails>
                    details) {
              out = std::move(details);
              run_loop.Quit();
            }));

    run_loop.Run();
    return out;
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         MediaHistoryStoreFeedsTest,
                         testing::Values(TestState::kNormal,
                                         TestState::kIncognito));

TEST_P(MediaHistoryStoreFeedsTest, DiscoverMediaFeed) {
  GURL url_a("https://www.google.com/feed");
  GURL url_b("https://www.google.co.uk/feed");
  GURL url_c("https://www.google.com/feed2");

  DiscoverMediaFeed(url_a);
  DiscoverMediaFeed(url_b);
  WaitForDB();

  {
    // Check the feeds were recorded.
    std::vector<media_feeds::mojom::MediaFeedPtr> feeds =
        GetMediaFeedsSync(service());

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
    } else {
      EXPECT_EQ(2u, feeds.size());

      EXPECT_EQ(1, feeds[0]->id);
      EXPECT_EQ(url_a, feeds[0]->url);
      EXPECT_FALSE(feeds[0]->last_fetch_time.has_value());
      EXPECT_EQ(media_feeds::mojom::FetchResult::kNone,
                feeds[0]->last_fetch_result);
      EXPECT_EQ(0, feeds[0]->fetch_failed_count);
      EXPECT_FALSE(feeds[0]->last_fetch_time_not_cache_hit.has_value());
      EXPECT_EQ(0, feeds[0]->last_fetch_item_count);
      EXPECT_EQ(0, feeds[0]->last_fetch_play_next_count);
      EXPECT_EQ(0, feeds[0]->last_fetch_content_types);
      EXPECT_TRUE(feeds[0]->logos.empty());
      EXPECT_TRUE(feeds[0]->display_name.empty());
      EXPECT_TRUE(feeds[0]->user_identifier.is_null());
      EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
                feeds[0]->safe_search_result);

      EXPECT_EQ(2, feeds[1]->id);
      EXPECT_EQ(url_b, feeds[1]->url);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
  }

  DiscoverMediaFeed(url_c);
  WaitForDB();

  {
    // Check the feeds were recorded.
    std::vector<media_feeds::mojom::MediaFeedPtr> feeds =
        GetMediaFeedsSync(service());

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
    } else {
      EXPECT_EQ(2u, feeds.size());

      EXPECT_EQ(2, feeds[0]->id);
      EXPECT_EQ(url_b, feeds[0]->url);
      EXPECT_EQ(3, feeds[1]->id);
      EXPECT_EQ(url_c, feeds[1]->url);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
  }
}

TEST_P(MediaHistoryStoreFeedsTest, StoreMediaFeedFetchResult) {
  const GURL feed_url("https://www.google.com/feed");
  DiscoverMediaFeed(feed_url);
  WaitForDB();

  // If we are read only we should use -1 as a placeholder feed id because the
  // feed will not have been stored. This is so we can run the rest of the test
  // to ensure a no-op.
  const int feed_id = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[0]->id;

  {
    auto result = SuccessfulResultWithItems(feed_id, GetExpectedItems());
    result.logos = GetExpectedLogos();
    result.display_name = kExpectedDisplayName;
    result.user_identifier = GetExpectedUserIdentifier();
    result.cookie_name_filter = "test";
    result.items[0]->id = 9;
    service()->StoreMediaFeedFetchResult(std::move(result), base::DoNothing());
    WaitForDB();

    // The media items should be stored and the feed should be updated.
    auto feeds = GetMediaFeedsSync(service());
    auto items = GetItemsForMediaFeedSync(service(), feed_id);

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
      EXPECT_TRUE(items.empty());
    } else {
      EXPECT_EQ(feed_id, feeds[0]->id);
      EXPECT_TRUE(feeds[0]->last_fetch_time.has_value());
      EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
                feeds[0]->last_fetch_result);
      EXPECT_EQ(0, feeds[0]->fetch_failed_count);
      EXPECT_TRUE(feeds[0]->last_fetch_time_not_cache_hit.has_value());
      EXPECT_EQ(kExpectedFetchItemCount, feeds[0]->last_fetch_item_count);
      EXPECT_EQ(kExpectedFetchPlayNextCount,
                feeds[0]->last_fetch_play_next_count);
      EXPECT_EQ(kExpectedFetchContentTypes, feeds[0]->last_fetch_content_types);
      EXPECT_EQ(GetExpectedLogos(), feeds[0]->logos);
      EXPECT_EQ(kExpectedDisplayName, feeds[0]->display_name);
      EXPECT_EQ(GetExpectedUserIdentifier(), feeds[0]->user_identifier);
      EXPECT_FALSE(feeds[0]->last_display_time.has_value());
      EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
      EXPECT_EQ("test", feeds[0]->cookie_name_filter);

      EXPECT_EQ(GetExpectedItems(), items);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
    EXPECT_EQ(items, GetItemsForMediaFeedSync(otr_service(), feed_id));
  }

  base::Optional<base::Time> last_fetch_time_not_cache_hit;

  {
    auto result = SuccessfulResultWithItems(feed_id, GetAltExpectedItems());
    result.display_name = kExpectedDisplayName;
    service()->StoreMediaFeedFetchResult(std::move(result), base::DoNothing());
    WaitForDB();

    // The media items should be stored and the feed should be updated.
    auto feeds = GetMediaFeedsSync(service());
    auto items = GetItemsForMediaFeedSync(service(), feed_id);

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
      EXPECT_TRUE(items.empty());
    } else {
      EXPECT_EQ(feed_id, feeds[0]->id);
      EXPECT_TRUE(feeds[0]->last_fetch_time.has_value());
      EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
                feeds[0]->last_fetch_result);
      EXPECT_EQ(0, feeds[0]->fetch_failed_count);
      EXPECT_TRUE(feeds[0]->last_fetch_time_not_cache_hit.has_value());
      EXPECT_EQ(kExpectedAltFetchItemCount, feeds[0]->last_fetch_item_count);
      EXPECT_EQ(kExpectedAltFetchPlayNextCount,
                feeds[0]->last_fetch_play_next_count);
      EXPECT_EQ(kExpectedAltFetchContentTypes,
                feeds[0]->last_fetch_content_types);
      EXPECT_TRUE(feeds[0]->logos.empty());
      EXPECT_EQ(kExpectedDisplayName, feeds[0]->display_name);
      EXPECT_FALSE(feeds[0]->user_identifier);
      EXPECT_FALSE(feeds[0]->last_display_time.has_value());
      EXPECT_TRUE(feeds[0]->cookie_name_filter.empty());

      EXPECT_EQ(GetAltExpectedItems(3), items);

      last_fetch_time_not_cache_hit = feeds[0]->last_fetch_time_not_cache_hit;
    }

    // The OTR service should have the same data.
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
    EXPECT_EQ(items, GetItemsForMediaFeedSync(otr_service(), feed_id));
  }

  {
    auto result = SuccessfulResultWithItems(feed_id, GetAltExpectedItems());
    result.display_name = kExpectedDisplayName;
    service()->StoreMediaFeedFetchResult(std::move(result), base::DoNothing());
    WaitForDB();

    // The media items should be stored and the feed should be updated.
    auto feeds = GetMediaFeedsSync(service());
    auto items = GetItemsForMediaFeedSync(service(), feed_id);

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
      EXPECT_TRUE(items.empty());
    } else {
      EXPECT_EQ(feed_id, feeds[0]->id);
      EXPECT_TRUE(feeds[0]->last_fetch_time.has_value());
      EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
                feeds[0]->last_fetch_result);
      EXPECT_EQ(0, feeds[0]->fetch_failed_count);
      EXPECT_TRUE(feeds[0]->last_fetch_time_not_cache_hit.has_value());
      EXPECT_EQ(kExpectedAltFetchItemCount, feeds[0]->last_fetch_item_count);
      EXPECT_EQ(kExpectedAltFetchPlayNextCount,
                feeds[0]->last_fetch_play_next_count);
      EXPECT_EQ(kExpectedAltFetchContentTypes,
                feeds[0]->last_fetch_content_types);
      EXPECT_TRUE(feeds[0]->logos.empty());
      EXPECT_EQ(kExpectedDisplayName, feeds[0]->display_name);
      EXPECT_FALSE(feeds[0]->last_display_time.has_value());

      EXPECT_EQ(GetAltExpectedItems(4), items);

      EXPECT_EQ(last_fetch_time_not_cache_hit,
                feeds[0]->last_fetch_time_not_cache_hit);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
    EXPECT_EQ(items, GetItemsForMediaFeedSync(otr_service(), feed_id));
  }

  service()->UpdateMediaFeedDisplayTime(feed_id);
  WaitForDB();

  {
    // The media feed should have a display time.
    auto feeds = GetMediaFeedsSync(service());

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
    } else {
      EXPECT_EQ(feed_id, feeds[0]->id);
      EXPECT_TRUE(feeds[0]->last_display_time.has_value());
    }

    // The OTR service should have the same data.
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
  }
}

TEST_P(MediaHistoryStoreFeedsTest, StoreMediaFeedFetchResult_WithEmpty) {
  DiscoverMediaFeed(GURL("https://www.google.com/feed"));
  WaitForDB();

  // If we are read only we should use -1 as a placeholder feed id because the
  // feed will not have been stored. This is so we can run the rest of the test
  // to ensure a no-op.
  const int feed_id = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[0]->id;

  service()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(feed_id, GetExpectedItems()),
      base::DoNothing());
  WaitForDB();

  {
    // The media items should be stored.
    auto items = GetItemsForMediaFeedSync(service(), feed_id);

    if (IsReadOnly()) {
      EXPECT_TRUE(items.empty());
    } else {
      EXPECT_EQ(GetExpectedItems(), items);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(items, GetItemsForMediaFeedSync(otr_service(), feed_id));
  }

  service()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(
          feed_id, std::vector<media_feeds::mojom::MediaFeedItemPtr>()),
      base::DoNothing());
  WaitForDB();

  {
    // There should be no items stored.
    auto items = GetItemsForMediaFeedSync(service(), feed_id);
    EXPECT_TRUE(items.empty());

    // The OTR service should have the same data.
    EXPECT_EQ(items, GetItemsForMediaFeedSync(otr_service(), feed_id));
  }
}

TEST_P(MediaHistoryStoreFeedsTest, StoreMediaFeedFetchResult_MultipleFeeds) {
  const GURL feed_a_url("https://www.google.com/feed");
  const GURL feed_b_url("https://www.google.co.uk/feed");

  DiscoverMediaFeed(feed_a_url);
  DiscoverMediaFeed(feed_b_url);
  WaitForDB();

  // If we are read only we should use -1 as a placeholder feed id because the
  // feed will not have been stored. This is so we can run the rest of the test
  // to ensure a no-op.
  const int feed_id_a = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[0]->id;
  const int feed_id_b = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[1]->id;

  service()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(feed_id_a, GetExpectedItems()),
      base::DoNothing());
  WaitForDB();

  service()->StoreMediaFeedFetchResult(
      ResultWithItems(feed_id_b, GetAltExpectedItems(),
                      media_feeds::mojom::FetchResult::kFailedNetworkError),
      base::DoNothing());
  WaitForDB();

  {
    // Check the feeds were updated.
    auto feeds = GetMediaFeedsSync(service());

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
    } else {
      EXPECT_EQ(2u, feeds.size());

      EXPECT_EQ(feed_id_a, feeds[0]->id);
      EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
                feeds[0]->last_fetch_result);
      EXPECT_EQ(0, feeds[0]->fetch_failed_count);

      EXPECT_EQ(feed_id_b, feeds[1]->id);
      EXPECT_EQ(media_feeds::mojom::FetchResult::kFailedNetworkError,
                feeds[1]->last_fetch_result);
      EXPECT_EQ(1, feeds[1]->fetch_failed_count);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
  }

  {
    // The media items should be stored.
    auto items = GetItemsForMediaFeedSync(service(), feed_id_a);

    if (IsReadOnly()) {
      EXPECT_TRUE(items.empty());
    } else {
      EXPECT_EQ(GetExpectedItems(), items);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(items, GetItemsForMediaFeedSync(otr_service(), feed_id_a));
  }

  {
    // The media items should be stored.
    auto items = GetItemsForMediaFeedSync(service(), feed_id_b);

    if (IsReadOnly()) {
      EXPECT_TRUE(items.empty());
    } else {
      EXPECT_EQ(GetAltExpectedItems(3), items);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(items, GetItemsForMediaFeedSync(otr_service(), feed_id_b));
  }
}

TEST_P(MediaHistoryStoreFeedsTest, RediscoverMediaFeed) {
  GURL feed_url("https://www.google.com/feed");
  DiscoverMediaFeed(feed_url);
  WaitForDB();

  // If we are read only we should use -1 as a placeholder feed id because the
  // feed will not have been stored. This is so we can run the rest of the test
  // to ensure a no-op.
  int feed_id = -1;
  base::Time feed_last_time;

  if (!IsReadOnly()) {
    auto feeds = GetMediaFeedsSync(service());
    feed_id = feeds[0]->id;
    feed_last_time = feeds[0]->last_discovery_time;

    EXPECT_LT(base::Time(), feed_last_time);
    EXPECT_GT(base::Time::Now(), feed_last_time);
    EXPECT_EQ(feed_url, feeds[0]->url);
  }

  service()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(feed_id, GetExpectedItems()),
      base::DoNothing());
  WaitForDB();

  {
    // The media items should be stored.
    auto items = GetItemsForMediaFeedSync(service(), feed_id);

    if (IsReadOnly()) {
      EXPECT_TRUE(items.empty());
    } else {
      EXPECT_EQ(GetExpectedItems(), items);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(items, GetItemsForMediaFeedSync(otr_service(), feed_id));
  }

  // Rediscovering the same feed should not replace the feed.
  DiscoverMediaFeed(feed_url);
  WaitForDB();

  if (!IsReadOnly()) {
    auto feeds = GetMediaFeedsSync(service());

    EXPECT_LE(feed_last_time, feeds[0]->last_discovery_time);
    EXPECT_EQ(feed_id, feeds[0]->id);
    EXPECT_EQ(feed_url, feeds[0]->url);
    EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
              feeds[0]->last_fetch_result);
  }

  {
    // The media items should be stored.
    auto items = GetItemsForMediaFeedSync(service(), feed_id);

    if (IsReadOnly()) {
      EXPECT_TRUE(items.empty());
    } else {
      EXPECT_EQ(GetExpectedItems(), items);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(items, GetItemsForMediaFeedSync(otr_service(), feed_id));
  }

  // Finding a new URL should replace the feed.
  GURL new_url("https://www.google.com/feed2");
  DiscoverMediaFeed(new_url);
  WaitForDB();

  if (!IsReadOnly()) {
    auto feeds = GetMediaFeedsSync(service());

    EXPECT_LE(feed_last_time, feeds[0]->last_discovery_time);
    EXPECT_LT(feed_id, feeds[0]->id);
    EXPECT_EQ(new_url, feeds[0]->url);
    EXPECT_EQ(media_feeds::mojom::FetchResult::kNone,
              feeds[0]->last_fetch_result);
  }

  {
    // The media items should be deleted.
    auto items = GetItemsForMediaFeedSync(service(), feed_id);
    EXPECT_TRUE(items.empty());

    // The OTR service should have the same data.
    EXPECT_EQ(items, GetItemsForMediaFeedSync(otr_service(), feed_id));
  }
}

TEST_P(MediaHistoryStoreFeedsTest, StoreMediaFeedFetchResult_IncreaseFailed) {
  DiscoverMediaFeed(GURL("https://www.google.com/feed"));
  WaitForDB();

  // If we are read only we should use -1 as a placeholder feed id because the
  // feed will not have been stored. This is so we can run the rest of the test
  // to ensure a no-op.
  const int feed_id = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[0]->id;

  {
    auto result =
        ResultWithItems(feed_id, GetExpectedItems(),
                        media_feeds::mojom::FetchResult::kFailedNetworkError);
    result.logos = GetExpectedLogos();
    result.display_name = kExpectedDisplayName;
    service()->StoreMediaFeedFetchResult(std::move(result), base::DoNothing());
    WaitForDB();

    // The fetch failed count should have been increased.
    auto feeds = GetMediaFeedsSync(service());

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
    } else {
      EXPECT_EQ(feed_id, feeds[0]->id);
      EXPECT_EQ(media_feeds::mojom::FetchResult::kFailedNetworkError,
                feeds[0]->last_fetch_result);
      EXPECT_EQ(1, feeds[0]->fetch_failed_count);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
  }

  {
    auto result =
        ResultWithItems(feed_id, GetExpectedItems(),
                        media_feeds::mojom::FetchResult::kFailedBackendError);
    result.logos = GetExpectedLogos();
    result.display_name = kExpectedDisplayName;
    service()->StoreMediaFeedFetchResult(std::move(result), base::DoNothing());
    WaitForDB();

    // The fetch failed count should have been increased.
    auto feeds = GetMediaFeedsSync(service());

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
    } else {
      EXPECT_EQ(feed_id, feeds[0]->id);
      EXPECT_EQ(media_feeds::mojom::FetchResult::kFailedBackendError,
                feeds[0]->last_fetch_result);
      EXPECT_EQ(2, feeds[0]->fetch_failed_count);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
  }

  {
    auto result = SuccessfulResultWithItems(feed_id, GetExpectedItems());
    result.logos = GetExpectedLogos();
    result.display_name = kExpectedDisplayName;
    service()->StoreMediaFeedFetchResult(std::move(result), base::DoNothing());
    WaitForDB();

    // The fetch failed count should have been reset.
    auto feeds = GetMediaFeedsSync(service());

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
    } else {
      EXPECT_EQ(feed_id, feeds[0]->id);
      EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
                feeds[0]->last_fetch_result);
      EXPECT_EQ(0, feeds[0]->fetch_failed_count);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
  }
}

TEST_P(MediaHistoryStoreFeedsTest, StoreMediaFeedFetchResult_CheckLogoMax) {
  DiscoverMediaFeed(GURL("https://www.google.com/feed"));
  WaitForDB();

  // If we are read only we should use -1 as a placeholder feed id because the
  // feed will not have been stored. This is so we can run the rest of the test
  // to ensure a no-op.
  const int feed_id = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[0]->id;

  std::vector<media_feeds::mojom::MediaImagePtr> logos;

  {
    media_feeds::mojom::MediaImagePtr image =
        media_feeds::mojom::MediaImage::New();
    image->src = GURL("https://www.example.org/image1.png");
    logos.push_back(std::move(image));
  }

  {
    media_feeds::mojom::MediaImagePtr image =
        media_feeds::mojom::MediaImage::New();
    image->src = GURL("https://www.example.org/image2.png");
    logos.push_back(std::move(image));
  }

  {
    media_feeds::mojom::MediaImagePtr image =
        media_feeds::mojom::MediaImage::New();
    image->src = GURL("https://www.example.org/image3.png");
    logos.push_back(std::move(image));
  }

  {
    media_feeds::mojom::MediaImagePtr image =
        media_feeds::mojom::MediaImage::New();
    image->src = GURL("https://www.example.org/image4.png");
    logos.push_back(std::move(image));
  }

  {
    media_feeds::mojom::MediaImagePtr image =
        media_feeds::mojom::MediaImage::New();
    image->src = GURL("https://www.example.org/image5.png");
    logos.push_back(std::move(image));
  }

  {
    media_feeds::mojom::MediaImagePtr image =
        media_feeds::mojom::MediaImage::New();
    image->src = GURL("https://www.example.org/image6.png");
    logos.push_back(std::move(image));
  }

  auto result =
      ResultWithItems(feed_id, GetExpectedItems(),
                      media_feeds::mojom::FetchResult::kFailedNetworkError);
  result.logos = std::move(logos);
  result.display_name = kExpectedDisplayName;
  service()->StoreMediaFeedFetchResult(std::move(result), base::DoNothing());
  WaitForDB();

  {
    // The feed should have at most 5 logos.
    auto feeds = GetMediaFeedsSync(service());

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
    } else {
      EXPECT_EQ(feed_id, feeds[0]->id);
      EXPECT_EQ(5u, feeds[0]->logos.size());
    }

    // The OTR service should have the same data.
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
  }
}

TEST_P(MediaHistoryStoreFeedsTest, StoreMediaFeedFetchResult_CheckImageMax) {
  DiscoverMediaFeed(GURL("https://www.google.com/feed"));
  WaitForDB();

  // If we are read only we should use -1 as a placeholder feed id because the
  // feed will not have been stored. This is so we can run the rest of the test
  // to ensure a no-op.
  const int feed_id = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[0]->id;

  auto item = media_feeds::mojom::MediaFeedItem::New();
  item->name = base::ASCIIToUTF16("The Movie");
  item->type = media_feeds::mojom::MediaFeedItemType::kMovie;
  item->safe_search_result = media_feeds::mojom::SafeSearchResult::kUnknown;

  {
    media_feeds::mojom::MediaImagePtr image =
        media_feeds::mojom::MediaImage::New();
    image->src = GURL("https://www.example.org/image1.png");
    item->images.push_back(std::move(image));
  }

  {
    media_feeds::mojom::MediaImagePtr image =
        media_feeds::mojom::MediaImage::New();
    image->src = GURL("https://www.example.org/image2.png");
    item->images.push_back(std::move(image));
  }

  {
    media_feeds::mojom::MediaImagePtr image =
        media_feeds::mojom::MediaImage::New();
    image->src = GURL("https://www.example.org/image3.png");
    item->images.push_back(std::move(image));
  }

  {
    media_feeds::mojom::MediaImagePtr image =
        media_feeds::mojom::MediaImage::New();
    image->src = GURL("https://www.example.org/image4.png");
    item->images.push_back(std::move(image));
  }

  {
    media_feeds::mojom::MediaImagePtr image =
        media_feeds::mojom::MediaImage::New();
    image->src = GURL("https://www.example.org/image5.png");
    item->images.push_back(std::move(image));
  }

  {
    media_feeds::mojom::MediaImagePtr image =
        media_feeds::mojom::MediaImage::New();
    image->src = GURL("https://www.example.org/image6.png");
    item->images.push_back(std::move(image));
  }

  std::vector<media_feeds::mojom::MediaFeedItemPtr> items;
  items.push_back(std::move(item));

  auto result = SuccessfulResultWithItems(feed_id, std::move(items));
  result.logos = GetExpectedLogos();
  result.display_name = kExpectedDisplayName;
  service()->StoreMediaFeedFetchResult(std::move(result), base::DoNothing());
  WaitForDB();

  {
    // The item should have at most 5 images.
    auto items = GetItemsForMediaFeedSync(service(), feed_id);

    if (IsReadOnly()) {
      EXPECT_TRUE(items.empty());
    } else {
      EXPECT_EQ(5u, items[0]->images.size());
    }

    // The OTR service should have the same data.
    EXPECT_EQ(items, GetItemsForMediaFeedSync(otr_service(), feed_id));
  }
}

TEST_P(MediaHistoryStoreFeedsTest,
       StoreMediaFeedFetchResult_DefaultSafeSearchResult) {
  DiscoverMediaFeed(GURL("https://www.google.com/feed"));
  WaitForDB();

  // If we are read only we should use -1 as a placeholder feed id because the
  // feed will not have been stored. This is so we can run the rest of the test
  // to ensure a no-op.
  const int feed_id = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[0]->id;

  auto item = media_feeds::mojom::MediaFeedItem::New();
  item->name = base::ASCIIToUTF16("The Movie");
  item->type = media_feeds::mojom::MediaFeedItemType::kMovie;

  std::vector<media_feeds::mojom::MediaFeedItemPtr> items;
  items.push_back(std::move(item));

  auto result = SuccessfulResultWithItems(feed_id, GetExpectedItems());
  result.logos = GetExpectedLogos();
  result.display_name = kExpectedDisplayName;
  service()->StoreMediaFeedFetchResult(std::move(result), base::DoNothing());
  WaitForDB();

  {
    // The item should set a default safe search result.
    auto items = GetItemsForMediaFeedSync(service(), feed_id);

    if (IsReadOnly()) {
      EXPECT_TRUE(items.empty());
    } else {
      EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
                items[0]->safe_search_result);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(items, GetItemsForMediaFeedSync(otr_service(), feed_id));
  }
}

TEST_P(MediaHistoryStoreFeedsTest, SafeSearchCheck) {
  const GURL feed_url_a("https://www.google.com/feed");
  const GURL feed_url_b("https://www.google.co.uk/feed");

  DiscoverMediaFeed(feed_url_a);
  DiscoverMediaFeed(feed_url_b);
  WaitForDB();

  // If we are read only we should use -1 as a placeholder feed id because the
  // feed will not have been stored. This is so we can run the rest of the test
  // to ensure a no-op.
  const int feed_id_a = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[0]->id;
  const int feed_id_b = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[1]->id;

  media_history::MediaHistoryKeyedService::MediaFeedFetchResult result_a;
  result_a.feed_id = feed_id_a;
  result_a.status = media_feeds::mojom::FetchResult::kSuccess;
  result_a.items = GetExpectedItems();
  service()->StoreMediaFeedFetchResult(std::move(result_a), base::DoNothing());
  WaitForDB();

  media_history::MediaHistoryKeyedService::MediaFeedFetchResult result_b;
  result_b.feed_id = feed_id_b;
  result_b.status = media_feeds::mojom::FetchResult::kSuccess;
  result_b.items = GetAltExpectedItems();
  service()->StoreMediaFeedFetchResult(std::move(result_b), base::DoNothing());
  WaitForDB();

  std::map<media_history::MediaHistoryKeyedService::SafeSearchID,
           media_feeds::mojom::SafeSearchResult>
      found_ids;
  std::map<media_history::MediaHistoryKeyedService::SafeSearchID,
           media_feeds::mojom::SafeSearchResult>
      found_ids_unsafe;

  {
    // Media items from all feeds should be in the pending items list.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync(service());
    auto feeds = GetMediaFeedsSync(
        service(), MediaHistoryKeyedService::GetMediaFeedsRequest());

    if (IsReadOnly()) {
      EXPECT_TRUE(pending_items.empty());
      EXPECT_TRUE(feeds.empty());
    } else {
      ASSERT_EQ(2u, feeds.size());
      EXPECT_EQ(4u, pending_items.size());

      std::set<GURL> found_urls;
      for (auto& item : pending_items) {
        EXPECT_NE(0, item->id.second);
        found_ids.emplace(item->id,
                          media_feeds::mojom::SafeSearchResult::kSafe);
        found_ids_unsafe.emplace(item->id,
                                 media_feeds::mojom::SafeSearchResult::kUnsafe);

        for (auto& url : item->urls) {
          found_urls.insert(url);
        }
      }

      std::set<GURL> expected_urls;
      expected_urls.insert(GURL("https://www.example.com/action"));
      expected_urls.insert(GURL("https://www.example.com/next"));
      expected_urls.insert(GURL("https://www.example.com/action-alt"));
      expected_urls.insert(feed_url_a);
      expected_urls.insert(feed_url_b);
      EXPECT_EQ(expected_urls, found_urls);

      EXPECT_EQ(1, feeds[0]->last_fetch_safe_item_count);
      EXPECT_EQ(0, feeds[1]->last_fetch_safe_item_count);
    }
  }

  service()->StoreMediaFeedItemSafeSearchResults(found_ids);
  WaitForDB();

  {
    // The pending item list should be empty and the safe counters should be
    // set.
    EXPECT_TRUE(GetPendingSafeSearchCheckMediaFeedItemsSync(service()).empty());

    auto feeds = GetMediaFeedsSync(
        service(), MediaHistoryKeyedService::GetMediaFeedsRequest());

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
    } else {
      ASSERT_EQ(2u, feeds.size());

      EXPECT_EQ(feed_id_a, feeds[0]->id);
      EXPECT_EQ(2, feeds[0]->last_fetch_safe_item_count);
      EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kSafe,
                feeds[0]->safe_search_result);

      EXPECT_EQ(feed_id_b, feeds[1]->id);
      EXPECT_EQ(1, feeds[1]->last_fetch_safe_item_count);
      EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kSafe,
                feeds[1]->safe_search_result);
    }
  }

  service()->StoreMediaFeedItemSafeSearchResults(found_ids_unsafe);
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync(
        service(), MediaHistoryKeyedService::GetMediaFeedsRequest());

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
    } else {
      ASSERT_EQ(2u, feeds.size());

      EXPECT_EQ(feed_id_a, feeds[0]->id);
      EXPECT_EQ(1, feeds[0]->last_fetch_safe_item_count);
      EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnsafe,
                feeds[0]->safe_search_result);

      EXPECT_EQ(feed_id_b, feeds[1]->id);
      EXPECT_EQ(0, feeds[1]->last_fetch_safe_item_count);
      EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnsafe,
                feeds[1]->safe_search_result);
    }
  }
}

TEST_P(MediaHistoryStoreFeedsTest, GetMediaFeedsSortByWatchtimePercentile) {
  // We add 111 origins with watchtime and feeds for all but one of these. Half
  // of the feeds will have items.
  const unsigned kNumberOfOrigins = 111;
  const unsigned kNumberOfFeeds = 110;

  // The starting percentile always has one percentage value taken off. This
  // is because we have one extra origin with the highest watchtime that does
  // not have a feed.
  const double kPercentageValue = 100.0 / kNumberOfOrigins;
  const double kStartingPercentile = 100 - kPercentageValue;

  // Generate a bunch of media feeds.
  std::set<GURL> feeds;
  for (unsigned i = 0; i < kNumberOfOrigins; i++) {
    GURL url(base::StringPrintf("https://www.google%i.com/feed", i));
    feeds.insert(url);

    // Each origin will have a ascending amount of watchtime from 0 to
    // |kNumberOfOrigins|.
    auto watchtime = base::TimeDelta::FromMinutes(i);
    content::MediaPlayerWatchTime watch_time(url, url.GetOrigin(), watchtime,
                                             base::TimeDelta(), true, true);

    service()->SavePlayback(watch_time);

    if (i < kNumberOfFeeds) {
      DiscoverMediaFeed(url);

      if (i == 0) {
        service()->StoreMediaFeedFetchResult(
            SuccessfulResultWithItems(i + 1, GetExpectedItems()),
            base::DoNothing());
      } else if (i % 2 == 0) {
        service()->StoreMediaFeedFetchResult(
            SuccessfulResultWithItems(i + 1, GetAltExpectedItems()),
            base::DoNothing());
      }
    }
  }

  WaitForDB();

  {
    // Check the feeds and origins were stored.
    auto feeds = GetMediaFeedsSync(service());
    auto origins = GetOriginRowsSync(service());

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
      EXPECT_TRUE(origins.empty());
    } else {
      EXPECT_EQ(kNumberOfFeeds, feeds.size());
      EXPECT_EQ(kNumberOfOrigins, origins.size());

      int i = 0;
      for (auto& origin : origins) {
        auto watchtime = base::TimeDelta::FromMinutes(i);

        EXPECT_EQ(watchtime, origin->cached_audio_video_watchtime);
        EXPECT_EQ(watchtime, origin->actual_audio_video_watchtime);

        i++;
      }
    }

    // The OTR service should have the same data.
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
    EXPECT_EQ(origins, GetOriginRowsSync(otr_service()));
  }

  {
    // Check the media feed sorting by works for top feeds for fetch.
    auto feeds = GetMediaFeedsSync(
        service(),
        MediaHistoryKeyedService::GetMediaFeedsRequest::CreateTopFeedsForFetch(
            kNumberOfFeeds, base::TimeDelta()));

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
    } else {
      EXPECT_EQ(kNumberOfFeeds, feeds.size());

      unsigned count = kNumberOfFeeds;
      double percentile = kStartingPercentile;
      double last_percentile = 101.0;
      for (auto& feed : feeds) {
        GURL url(
            base::StringPrintf("https://www.google%i.com/feed", count - 1));

        EXPECT_EQ(count, feed->id);
        EXPECT_EQ(url, feed->url);
        EXPECT_NEAR(percentile, feed->origin_audio_video_watchtime_percentile,
                    1);
        EXPECT_GT(last_percentile,
                  feed->origin_audio_video_watchtime_percentile);

        last_percentile = feed->origin_audio_video_watchtime_percentile;
        percentile = percentile - kPercentageValue;
        count--;
      }
    }
  }

  {
    // Check the media feed sorting works for top feeds with a limit.
    auto feeds = GetMediaFeedsSync(
        service(),
        MediaHistoryKeyedService::GetMediaFeedsRequest::CreateTopFeedsForFetch(
            10, base::TimeDelta()));

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
    } else {
      EXPECT_EQ(10u, feeds.size());

      unsigned count = kNumberOfFeeds;
      double percentile = kStartingPercentile;
      double last_percentile = 101.0;
      for (auto& feed : feeds) {
        GURL url(
            base::StringPrintf("https://www.google%i.com/feed", count - 1));

        EXPECT_EQ(count, feed->id);
        EXPECT_EQ(url, feed->url);
        EXPECT_NEAR(percentile, feed->origin_audio_video_watchtime_percentile,
                    1);
        EXPECT_GT(last_percentile,
                  feed->origin_audio_video_watchtime_percentile);

        last_percentile = feed->origin_audio_video_watchtime_percentile;
        percentile = percentile - kPercentageValue;
        count--;
      }
    }
  }

  {
    // Check the media feed sorting works for top feeds with a minimum watchtime
    // requirement and ranking.
    auto feeds = GetMediaFeedsSync(
        service(),
        MediaHistoryKeyedService::GetMediaFeedsRequest::CreateTopFeedsForFetch(
            kNumberOfFeeds, base::TimeDelta::FromMinutes(30)));

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
    } else {
      EXPECT_EQ(80u, feeds.size());

      unsigned count = kNumberOfFeeds;
      double percentile = kStartingPercentile;
      double last_percentile = 101.0;
      for (auto& feed : feeds) {
        GURL url(
            base::StringPrintf("https://www.google%i.com/feed", count - 1));

        EXPECT_EQ(count, feed->id);
        EXPECT_EQ(url, feed->url);
        EXPECT_NEAR(percentile, feed->origin_audio_video_watchtime_percentile,
                    1);
        EXPECT_GT(last_percentile,
                  feed->origin_audio_video_watchtime_percentile);

        last_percentile = feed->origin_audio_video_watchtime_percentile;
        percentile = percentile - kPercentageValue;
        count--;
      }
    }
  }

  {
    // Check the media feed sorting works for top feeds with a minimum watchtime
    // requirement, ranking and limit.
    auto feeds = GetMediaFeedsSync(
        service(),
        MediaHistoryKeyedService::GetMediaFeedsRequest::CreateTopFeedsForFetch(
            10, base::TimeDelta::FromMinutes(30)));

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
    } else {
      EXPECT_EQ(10u, feeds.size());

      unsigned count = kNumberOfFeeds;
      double percentile = kStartingPercentile;
      double last_percentile = 101.0;
      for (auto& feed : feeds) {
        GURL url(
            base::StringPrintf("https://www.google%i.com/feed", count - 1));

        EXPECT_EQ(count, feed->id);
        EXPECT_EQ(url, feed->url);
        EXPECT_NEAR(percentile, feed->origin_audio_video_watchtime_percentile,
                    1);
        EXPECT_GT(last_percentile,
                  feed->origin_audio_video_watchtime_percentile);

        last_percentile = feed->origin_audio_video_watchtime_percentile;
        percentile = percentile - kPercentageValue;
        count--;
      }
    }
  }

  {
    // Check the media feed fetched items for display works.
    auto feeds = GetMediaFeedsSync(
        service(),
        MediaHistoryKeyedService::GetMediaFeedsRequest::
            CreateTopFeedsForDisplay(kNumberOfFeeds, 1, false, base::nullopt));

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
    } else {
      EXPECT_EQ(55u, feeds.size());

      unsigned count = kNumberOfFeeds;
      double percentile = kStartingPercentile;
      double last_percentile = 101.0;
      for (auto& feed : feeds) {
        const int i = count - 1;

        if (i % 2 != 0)
          continue;

        GURL url(base::StringPrintf("https://www.google%i.com/feed", i));

        EXPECT_EQ(count, feed->id);
        EXPECT_EQ(url, feed->url);
        EXPECT_NEAR(percentile, feed->origin_audio_video_watchtime_percentile,
                    1);
        EXPECT_GT(last_percentile,
                  feed->origin_audio_video_watchtime_percentile);

        last_percentile = feed->origin_audio_video_watchtime_percentile;
        percentile = percentile - kPercentageValue;
        count--;
      }
    }
  }

  {
    // Check the media feed fetched items for display works for safe search.
    auto feeds = GetMediaFeedsSync(
        service(),
        MediaHistoryKeyedService::GetMediaFeedsRequest::
            CreateTopFeedsForDisplay(kNumberOfFeeds, 1, true, base::nullopt));

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
    } else {
      ASSERT_EQ(1u, feeds.size());
      EXPECT_EQ(1, feeds[0]->id);
    }
  }

  {
    // Check the media feed fetched items for display works with a content type
    // filter for web video content.
    auto feeds = GetMediaFeedsSync(
        service(), MediaHistoryKeyedService::GetMediaFeedsRequest::
                       CreateTopFeedsForDisplay(
                           kNumberOfFeeds, 1, false,
                           media_feeds::mojom::MediaFeedItemType::kVideo));

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
    } else {
      ASSERT_EQ(54u, feeds.size());
    }
  }

  {
    // Check the media feed fetched items for display works with a content type
    // filter for movies.
    auto feeds = GetMediaFeedsSync(
        service(), MediaHistoryKeyedService::GetMediaFeedsRequest::
                       CreateTopFeedsForDisplay(
                           kNumberOfFeeds, 1, false,
                           media_feeds::mojom::MediaFeedItemType::kMovie));

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
    } else {
      ASSERT_EQ(1u, feeds.size());
    }
  }
}

TEST_P(MediaHistoryStoreFeedsTest, FeedItemsClickAndShown) {
  DiscoverMediaFeed(GURL("https://www.google.com/feed"));
  WaitForDB();

  // If we are read only we should use -1 as a placeholder feed id because the
  // feed will not have been stored. This is so we can run the rest of the test
  // to ensure a no-op.
  const int feed_id = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[0]->id;

  auto result = SuccessfulResultWithItems(feed_id, GetExpectedItems());
  result.logos = GetExpectedLogos();
  result.display_name = kExpectedDisplayName;
  service()->StoreMediaFeedFetchResult(std::move(result), base::DoNothing());
  WaitForDB();

  {
    // The media items should be stored.
    auto items = GetItemsForMediaFeedSync(service(), feed_id);

    if (IsReadOnly()) {
      EXPECT_TRUE(items.empty());
    } else {
      EXPECT_EQ(3u, items[0]->shown_count);
      EXPECT_TRUE(items[0]->clicked);

      EXPECT_EQ(0u, items[1]->shown_count);
      EXPECT_FALSE(items[1]->clicked);

      EXPECT_EQ(0u, items[2]->shown_count);
      EXPECT_FALSE(items[2]->clicked);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(items, GetItemsForMediaFeedSync(otr_service(), feed_id));
  }

  std::set<int64_t> ids;
  ids.insert(1);
  ids.insert(2);

  // Increment the shown count.
  service()->IncrementMediaFeedItemsShownCount(ids);
  WaitForDB();

  {
    // The media items should have been incremented.
    auto items = GetItemsForMediaFeedSync(service(), feed_id);

    if (IsReadOnly()) {
      EXPECT_TRUE(items.empty());
    } else {
      EXPECT_EQ(4u, items[0]->shown_count);
      EXPECT_TRUE(items[0]->clicked);

      EXPECT_EQ(1u, items[1]->shown_count);
      EXPECT_FALSE(items[1]->clicked);

      EXPECT_EQ(0u, items[2]->shown_count);
      EXPECT_FALSE(items[2]->clicked);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(items, GetItemsForMediaFeedSync(otr_service(), feed_id));
  }

  // Increment the shown count.
  service()->IncrementMediaFeedItemsShownCount(ids);
  WaitForDB();

  {
    // The media items should have been incremented.
    auto items = GetItemsForMediaFeedSync(service(), feed_id);

    if (IsReadOnly()) {
      EXPECT_TRUE(items.empty());
    } else {
      EXPECT_EQ(5u, items[0]->shown_count);
      EXPECT_TRUE(items[0]->clicked);

      EXPECT_EQ(2u, items[1]->shown_count);
      EXPECT_FALSE(items[1]->clicked);

      EXPECT_EQ(0u, items[2]->shown_count);
      EXPECT_FALSE(items[2]->clicked);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(items, GetItemsForMediaFeedSync(otr_service(), feed_id));
  }

  // Mark the item as clicked.
  service()->MarkMediaFeedItemAsClicked(2);
  WaitForDB();

  {
    // The media item should have been clicked.
    auto items = GetItemsForMediaFeedSync(service(), feed_id);

    if (IsReadOnly()) {
      EXPECT_TRUE(items.empty());
    } else {
      EXPECT_EQ(5u, items[0]->shown_count);
      EXPECT_TRUE(items[0]->clicked);

      EXPECT_EQ(2u, items[1]->shown_count);
      EXPECT_TRUE(items[1]->clicked);

      EXPECT_EQ(0u, items[2]->shown_count);
      EXPECT_FALSE(items[2]->clicked);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(items, GetItemsForMediaFeedSync(otr_service(), feed_id));
  }
}

TEST_P(MediaHistoryStoreFeedsTest, ResetMediaFeed) {
  const GURL feed_url_a("https://www.google.com/feed");
  const GURL feed_url_b("https://www.google.co.uk/feed");

  DiscoverMediaFeed(feed_url_a);
  DiscoverMediaFeed(feed_url_b);
  WaitForDB();

  // If we are read only we should use -1 as a placeholder feed id because the
  // feed will not have been stored. This is so we can run the rest of the test
  // to ensure a no-op.
  const int feed_id_a = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[0]->id;
  const int feed_id_b = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[1]->id;

  auto result = SuccessfulResultWithItems(feed_id_a, GetExpectedItems());
  result.logos = GetExpectedLogos();
  result.display_name = kExpectedDisplayName;
  result.user_identifier = GetExpectedUserIdentifier();
  service()->StoreMediaFeedFetchResult(std::move(result), base::DoNothing());
  service()->UpdateMediaFeedDisplayTime(feed_id_a);

  auto alt_result = SuccessfulResultWithItems(feed_id_b, GetAltExpectedItems());
  alt_result.logos = GetExpectedLogos();
  alt_result.display_name = kExpectedDisplayName;
  service()->StoreMediaFeedFetchResult(std::move(alt_result),
                                       base::DoNothing());
  service()->UpdateMediaFeedDisplayTime(feed_id_b);
  WaitForDB();

  {
    // The media items should be stored and the feed should be updated.
    auto feeds = GetMediaFeedsSync(service());
    auto items_a = GetItemsForMediaFeedSync(service(), feed_id_a);
    auto items_b = GetItemsForMediaFeedSync(service(), feed_id_b);

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
      EXPECT_TRUE(items_a.empty());
      EXPECT_TRUE(items_b.empty());
    } else {
      EXPECT_EQ(feed_id_a, feeds[0]->id);
      EXPECT_FALSE(feeds[0]->last_discovery_time.is_null());
      EXPECT_TRUE(feeds[0]->last_fetch_time.has_value());
      EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
                feeds[0]->last_fetch_result);
      EXPECT_EQ(0, feeds[0]->fetch_failed_count);
      EXPECT_TRUE(feeds[0]->last_fetch_time_not_cache_hit.has_value());
      EXPECT_EQ(kExpectedFetchItemCount, feeds[0]->last_fetch_item_count);
      EXPECT_EQ(kExpectedFetchPlayNextCount,
                feeds[0]->last_fetch_play_next_count);
      EXPECT_EQ(kExpectedFetchContentTypes, feeds[0]->last_fetch_content_types);
      EXPECT_EQ(GetExpectedLogos(), feeds[0]->logos);
      EXPECT_EQ(kExpectedDisplayName, feeds[0]->display_name);
      EXPECT_TRUE(feeds[0]->last_display_time.has_value());
      EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
      EXPECT_EQ(GetExpectedUserIdentifier(), feeds[0]->user_identifier);

      EXPECT_EQ(feed_id_b, feeds[1]->id);

      EXPECT_EQ(GetExpectedItems(), items_a);
      EXPECT_EQ(GetAltExpectedItems(3), items_b);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
    EXPECT_EQ(items_a, GetItemsForMediaFeedSync(otr_service(), feed_id_a));
    EXPECT_EQ(items_b, GetItemsForMediaFeedSync(otr_service(), feed_id_b));
  }

  if (auto* service = GetMediaFeedsService()) {
    service->ResetMediaFeed(url::Origin::Create(feed_url_a),
                            media_feeds::mojom::ResetReason::kCookies);
    WaitForDB();
  }

  {
    // The feed should have been reset.
    auto feeds = GetMediaFeedsSync(service());
    auto items_a = GetItemsForMediaFeedSync(service(), feed_id_a);
    auto items_b = GetItemsForMediaFeedSync(service(), feed_id_b);

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
      EXPECT_TRUE(items_a.empty());
      EXPECT_TRUE(items_b.empty());
    } else {
      EXPECT_EQ(feed_id_a, feeds[0]->id);
      EXPECT_FALSE(feeds[0]->last_discovery_time.is_null());
      EXPECT_FALSE(feeds[0]->last_fetch_time.has_value());
      EXPECT_EQ(media_feeds::mojom::FetchResult::kNone,
                feeds[0]->last_fetch_result);
      EXPECT_EQ(0, feeds[0]->fetch_failed_count);
      EXPECT_FALSE(feeds[0]->last_fetch_time_not_cache_hit.has_value());
      EXPECT_EQ(0, feeds[0]->last_fetch_item_count);
      EXPECT_EQ(0, feeds[0]->last_fetch_play_next_count);
      EXPECT_EQ(0, feeds[0]->last_fetch_content_types);
      EXPECT_TRUE(feeds[0]->logos.empty());
      EXPECT_TRUE(feeds[0]->display_name.empty());
      EXPECT_TRUE(feeds[0]->last_display_time.has_value());
      EXPECT_EQ(media_feeds::mojom::ResetReason::kCookies,
                feeds[0]->reset_reason);
      EXPECT_FALSE(feeds[0]->user_identifier);

      EXPECT_EQ(feed_id_b, feeds[1]->id);

      EXPECT_TRUE(items_a.empty());
      EXPECT_EQ(GetAltExpectedItems(3), items_b);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
    EXPECT_EQ(items_a, GetItemsForMediaFeedSync(otr_service(), feed_id_a));
    EXPECT_EQ(items_b, GetItemsForMediaFeedSync(otr_service(), feed_id_b));
  }

  {
    auto result = SuccessfulResultWithItems(feed_id_a, GetExpectedItems());
    result.logos = GetExpectedLogos();
    result.display_name = kExpectedDisplayName;
    service()->StoreMediaFeedFetchResult(std::move(result), base::DoNothing());
    service()->UpdateMediaFeedDisplayTime(feed_id_a);
    WaitForDB();

    // The media items and feed should be repopulated.
    auto feeds = GetMediaFeedsSync(service());
    auto items_a = GetItemsForMediaFeedSync(service(), feed_id_a);
    auto items_b = GetItemsForMediaFeedSync(service(), feed_id_b);

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
      EXPECT_TRUE(items_a.empty());
      EXPECT_TRUE(items_b.empty());
    } else {
      EXPECT_EQ(feed_id_a, feeds[0]->id);
      EXPECT_FALSE(feeds[0]->last_discovery_time.is_null());
      EXPECT_TRUE(feeds[0]->last_fetch_time.has_value());
      EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
                feeds[0]->last_fetch_result);
      EXPECT_EQ(0, feeds[0]->fetch_failed_count);
      EXPECT_TRUE(feeds[0]->last_fetch_time_not_cache_hit.has_value());
      EXPECT_EQ(kExpectedFetchItemCount, feeds[0]->last_fetch_item_count);
      EXPECT_EQ(kExpectedFetchPlayNextCount,
                feeds[0]->last_fetch_play_next_count);
      EXPECT_EQ(kExpectedFetchContentTypes, feeds[0]->last_fetch_content_types);
      EXPECT_EQ(GetExpectedLogos(), feeds[0]->logos);
      EXPECT_EQ(kExpectedDisplayName, feeds[0]->display_name);
      EXPECT_TRUE(feeds[0]->last_display_time.has_value());
      EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);

      EXPECT_EQ(feed_id_b, feeds[1]->id);

      EXPECT_EQ(GetExpectedItems(4), items_a);
      EXPECT_EQ(GetAltExpectedItems(3), items_b);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
    EXPECT_EQ(items_a, GetItemsForMediaFeedSync(otr_service(), feed_id_a));
    EXPECT_EQ(items_b, GetItemsForMediaFeedSync(otr_service(), feed_id_b));
  }
}

TEST_P(MediaHistoryStoreFeedsTest, ResetMediaFeedDueToCacheClearing) {
  const GURL feed_url_a("https://www.google.com/feed");
  const GURL feed_url_b("https://www.google.co.uk/feed");

  DiscoverMediaFeed(feed_url_a);
  DiscoverMediaFeed(feed_url_b);
  WaitForDB();

  // If we are read only we should use -1 as a placeholder feed id because the
  // feed will not have been stored. This is so we can run the rest of the test
  // to ensure a no-op.
  const int feed_id_a = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[0]->id;
  const int feed_id_b = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[1]->id;

  auto result = SuccessfulResultWithItems(feed_id_a, GetExpectedItems());
  result.logos = GetExpectedLogos();
  result.display_name = kExpectedDisplayName;
  service()->StoreMediaFeedFetchResult(std::move(result), base::DoNothing());
  WaitForDB();

  auto alt_result = SuccessfulResultWithItems(feed_id_b, GetAltExpectedItems());
  alt_result.logos = GetExpectedLogos();
  alt_result.display_name = kExpectedDisplayName;
  service()->StoreMediaFeedFetchResult(std::move(alt_result),
                                       base::DoNothing());
  WaitForDB();

  service()->UpdateMediaFeedDisplayTime(feed_id_a);
  service()->UpdateMediaFeedDisplayTime(feed_id_b);
  WaitForDB();

  {
    // The media items should be stored and the feed should be stored.
    auto feeds = GetMediaFeedsSync(service());
    auto items_a = GetItemsForMediaFeedSync(service(), feed_id_a);
    auto items_b = GetItemsForMediaFeedSync(service(), feed_id_b);

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
      EXPECT_TRUE(items_a.empty());
      EXPECT_TRUE(items_b.empty());
    } else {
      ASSERT_EQ(2u, feeds.size());

      for (auto& feed : feeds) {
        EXPECT_FALSE(feed->last_discovery_time.is_null());
        EXPECT_TRUE(feed->last_fetch_time.has_value());
        EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
                  feed->last_fetch_result);
        EXPECT_EQ(0, feed->fetch_failed_count);
        EXPECT_TRUE(feed->last_fetch_time_not_cache_hit.has_value());
        EXPECT_NE(0, feed->last_fetch_item_count);
        EXPECT_NE(0, feed->last_fetch_play_next_count);
        EXPECT_NE(0, feed->last_fetch_content_types);
        EXPECT_EQ(GetExpectedLogos(), feed->logos);
        EXPECT_EQ(kExpectedDisplayName, feed->display_name);
        EXPECT_TRUE(feed->last_display_time.has_value());
        EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feed->reset_reason);
      }

      EXPECT_EQ(GetExpectedItems(), items_a);
      EXPECT_EQ(GetAltExpectedItems(3), items_b);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
    EXPECT_EQ(items_a, GetItemsForMediaFeedSync(otr_service(), feed_id_a));
    EXPECT_EQ(items_b, GetItemsForMediaFeedSync(otr_service(), feed_id_b));
  }

  service()->ResetMediaFeedDueToCacheClearing(
      base::Time(), base::Time::Now() - base::TimeDelta::FromDays(1),
      base::BindRepeating([](const GURL& url) { return true; }),
      base::DoNothing());
  WaitForDB();

  {
    // The media items should not have been affected since we cleared yesterday.
    auto feeds = GetMediaFeedsSync(service());
    auto items_a = GetItemsForMediaFeedSync(service(), feed_id_a);
    auto items_b = GetItemsForMediaFeedSync(service(), feed_id_b);

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
      EXPECT_TRUE(items_a.empty());
      EXPECT_TRUE(items_b.empty());
    } else {
      ASSERT_EQ(2u, feeds.size());

      for (auto& feed : feeds) {
        EXPECT_FALSE(feed->last_discovery_time.is_null());
        EXPECT_TRUE(feed->last_fetch_time.has_value());
        EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
                  feed->last_fetch_result);
        EXPECT_EQ(0, feed->fetch_failed_count);
        EXPECT_TRUE(feed->last_fetch_time_not_cache_hit.has_value());
        EXPECT_NE(0, feed->last_fetch_item_count);
        EXPECT_NE(0, feed->last_fetch_play_next_count);
        EXPECT_NE(0, feed->last_fetch_content_types);
        EXPECT_EQ(GetExpectedLogos(), feed->logos);
        EXPECT_EQ(kExpectedDisplayName, feed->display_name);
        EXPECT_TRUE(feed->last_display_time.has_value());
        EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feed->reset_reason);
      }

      EXPECT_EQ(GetExpectedItems(), items_a);
      EXPECT_EQ(GetAltExpectedItems(3), items_b);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
    EXPECT_EQ(items_a, GetItemsForMediaFeedSync(otr_service(), feed_id_a));
    EXPECT_EQ(items_b, GetItemsForMediaFeedSync(otr_service(), feed_id_b));
  }

  service()->ResetMediaFeedDueToCacheClearing(
      base::Time(), base::Time::Max(),
      base::BindRepeating([](const GURL& url) { return true; }),
      base::DoNothing());
  WaitForDB();

  {
    // The feeds should have been reset.
    auto feeds = GetMediaFeedsSync(service());
    auto items_a = GetItemsForMediaFeedSync(service(), feed_id_a);
    auto items_b = GetItemsForMediaFeedSync(service(), feed_id_b);

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
      EXPECT_TRUE(items_a.empty());
      EXPECT_TRUE(items_b.empty());
    } else {
      ASSERT_EQ(2u, feeds.size());

      for (auto& feed : feeds) {
        EXPECT_FALSE(feed->last_discovery_time.is_null());
        EXPECT_FALSE(feed->last_fetch_time.has_value());
        EXPECT_EQ(media_feeds::mojom::FetchResult::kNone,
                  feed->last_fetch_result);
        EXPECT_EQ(0, feed->fetch_failed_count);
        EXPECT_FALSE(feed->last_fetch_time_not_cache_hit.has_value());
        EXPECT_EQ(0, feed->last_fetch_item_count);
        EXPECT_EQ(0, feed->last_fetch_play_next_count);
        EXPECT_EQ(0, feed->last_fetch_content_types);
        EXPECT_TRUE(feed->logos.empty());
        EXPECT_TRUE(feed->display_name.empty());
        EXPECT_TRUE(feed->last_display_time.has_value());
        EXPECT_EQ(media_feeds::mojom::ResetReason::kCache, feed->reset_reason);
      }

      EXPECT_TRUE(items_a.empty());
      EXPECT_TRUE(items_b.empty());
    }

    // The OTR service should have the same data.
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
    EXPECT_EQ(items_a, GetItemsForMediaFeedSync(otr_service(), feed_id_a));
    EXPECT_EQ(items_b, GetItemsForMediaFeedSync(otr_service(), feed_id_b));
  }

  {
    auto result = SuccessfulResultWithItems(feed_id_a, GetExpectedItems());
    result.logos = GetExpectedLogos();
    result.display_name = kExpectedDisplayName;
    service()->StoreMediaFeedFetchResult(std::move(result), base::DoNothing());
    service()->UpdateMediaFeedDisplayTime(feed_id_a);
    WaitForDB();

    // The media items and feed should be repopulated.
    auto feeds = GetMediaFeedsSync(service());
    auto items_a = GetItemsForMediaFeedSync(service(), feed_id_a);
    auto items_b = GetItemsForMediaFeedSync(service(), feed_id_b);

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
      EXPECT_TRUE(items_a.empty());
      EXPECT_TRUE(items_b.empty());
    } else {
      EXPECT_EQ(feed_id_a, feeds[0]->id);
      EXPECT_FALSE(feeds[0]->last_discovery_time.is_null());
      EXPECT_TRUE(feeds[0]->last_fetch_time.has_value());
      EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
                feeds[0]->last_fetch_result);
      EXPECT_EQ(0, feeds[0]->fetch_failed_count);
      EXPECT_TRUE(feeds[0]->last_fetch_time_not_cache_hit.has_value());
      EXPECT_EQ(kExpectedFetchItemCount, feeds[0]->last_fetch_item_count);
      EXPECT_EQ(kExpectedFetchPlayNextCount,
                feeds[0]->last_fetch_play_next_count);
      EXPECT_EQ(kExpectedFetchContentTypes, feeds[0]->last_fetch_content_types);
      EXPECT_EQ(GetExpectedLogos(), feeds[0]->logos);
      EXPECT_EQ(kExpectedDisplayName, feeds[0]->display_name);
      EXPECT_TRUE(feeds[0]->last_display_time.has_value());
      EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);

      EXPECT_EQ(feed_id_b, feeds[1]->id);

      EXPECT_EQ(GetExpectedItems(4), items_a);
      EXPECT_TRUE(items_b.empty());
    }

    // The OTR service should have the same data.
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
    EXPECT_EQ(items_a, GetItemsForMediaFeedSync(otr_service(), feed_id_a));
    EXPECT_EQ(items_b, GetItemsForMediaFeedSync(otr_service(), feed_id_b));
  }

  service()->ResetMediaFeedDueToCacheClearing(
      base::Time(), base::Time::Max(),
      base::BindRepeating([](const GURL& url) { return false; }),
      base::DoNothing());
  WaitForDB();

  {
    // The media items and feed should still be populated because the filter
    // returned false.
    auto feeds = GetMediaFeedsSync(service());
    auto items_a = GetItemsForMediaFeedSync(service(), feed_id_a);
    auto items_b = GetItemsForMediaFeedSync(service(), feed_id_b);

    if (IsReadOnly()) {
      EXPECT_TRUE(feeds.empty());
      EXPECT_TRUE(items_a.empty());
      EXPECT_TRUE(items_b.empty());
    } else {
      EXPECT_EQ(feed_id_a, feeds[0]->id);
      EXPECT_FALSE(feeds[0]->last_discovery_time.is_null());
      EXPECT_TRUE(feeds[0]->last_fetch_time.has_value());
      EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
                feeds[0]->last_fetch_result);
      EXPECT_EQ(0, feeds[0]->fetch_failed_count);
      EXPECT_TRUE(feeds[0]->last_fetch_time_not_cache_hit.has_value());
      EXPECT_EQ(kExpectedFetchItemCount, feeds[0]->last_fetch_item_count);
      EXPECT_EQ(kExpectedFetchPlayNextCount,
                feeds[0]->last_fetch_play_next_count);
      EXPECT_EQ(kExpectedFetchContentTypes, feeds[0]->last_fetch_content_types);
      EXPECT_EQ(GetExpectedLogos(), feeds[0]->logos);
      EXPECT_EQ(kExpectedDisplayName, feeds[0]->display_name);
      EXPECT_TRUE(feeds[0]->last_display_time.has_value());
      EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);

      EXPECT_EQ(feed_id_b, feeds[1]->id);

      EXPECT_EQ(GetExpectedItems(4), items_a);
      EXPECT_TRUE(items_b.empty());
    }

    // The OTR service should have the same data.
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
    EXPECT_EQ(items_a, GetItemsForMediaFeedSync(otr_service(), feed_id_a));
    EXPECT_EQ(items_b, GetItemsForMediaFeedSync(otr_service(), feed_id_b));
  }
}

TEST_P(MediaHistoryStoreFeedsTest, DeleteMediaFeed) {
  DiscoverMediaFeed(GURL("https://www.google.com/feed"));
  DiscoverMediaFeed(GURL("https://www.google.co.uk/feed"));
  WaitForDB();

  // If we are read only we should use -1 as a placeholder feed id because the
  // feed will not have been stored. This is so we can run the rest of the test
  // to ensure a no-op.
  const int feed_id_a = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[0]->id;
  const int feed_id_b = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[1]->id;

  service()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(feed_id_a, GetExpectedItems()),
      base::DoNothing());
  WaitForDB();

  service()->StoreMediaFeedFetchResult(
      ResultWithItems(feed_id_b, GetAltExpectedItems(),
                      media_feeds::mojom::FetchResult::kFailedNetworkError),
      base::DoNothing());
  WaitForDB();

  {
    // Check the feed and items are stored.
    auto stats = GetStatsSync(service());
    auto feeds = GetMediaFeedsSync(service());

    if (IsReadOnly()) {
      EXPECT_EQ(0, stats->table_row_counts[MediaHistoryFeedsTable::kTableName]);
      EXPECT_EQ(
          0, stats->table_row_counts[MediaHistoryFeedItemsTable::kTableName]);
    } else {
      ASSERT_EQ(2, stats->table_row_counts[MediaHistoryFeedsTable::kTableName]);
      EXPECT_EQ(
          4, stats->table_row_counts[MediaHistoryFeedItemsTable::kTableName]);

      EXPECT_EQ(feed_id_a, feeds[0]->id);
      EXPECT_EQ(feed_id_b, feeds[1]->id);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(stats, GetStatsSync(otr_service()));
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
  }

  service()->DeleteMediaFeed(feed_id_a, base::DoNothing());
  WaitForDB();

  {
    // Check the first feed was deleted.
    auto stats = GetStatsSync(service());
    auto feeds = GetMediaFeedsSync(service());

    if (IsReadOnly()) {
      EXPECT_EQ(0, stats->table_row_counts[MediaHistoryFeedsTable::kTableName]);
      EXPECT_EQ(
          0, stats->table_row_counts[MediaHistoryFeedItemsTable::kTableName]);
    } else {
      ASSERT_EQ(1, stats->table_row_counts[MediaHistoryFeedsTable::kTableName]);
      EXPECT_EQ(
          1, stats->table_row_counts[MediaHistoryFeedItemsTable::kTableName]);

      EXPECT_EQ(feed_id_b, feeds[0]->id);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(stats, GetStatsSync(otr_service()));
    EXPECT_EQ(feeds, GetMediaFeedsSync(otr_service()));
  }
}

TEST_P(MediaHistoryStoreFeedsTest, GetMediaFeedFetchDetails) {
  const GURL feed_url("https://www.google.com/feed");

  DiscoverMediaFeed(feed_url);
  WaitForDB();

  // If we are read only we should use -1 as a placeholder feed id because the
  // feed will not have been stored. This is so we can run the rest of the test
  // to ensure a no-op.
  const int feed_id = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[0]->id;

  {
    auto details = GetFetchDetailsSync(feed_id);

    if (IsReadOnly()) {
      EXPECT_FALSE(details.has_value());
    } else {
      EXPECT_FALSE(details->reset_token.has_value());
      EXPECT_EQ(feed_url, details->url);
      EXPECT_EQ(media_feeds::mojom::FetchResult::kNone,
                details->last_fetch_result);
    }
  }

  service()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(feed_id, GetExpectedItems()),
      base::DoNothing());
  WaitForDB();

  {
    auto details = GetFetchDetailsSync(feed_id);

    if (IsReadOnly()) {
      EXPECT_FALSE(details.has_value());
    } else {
      EXPECT_FALSE(details->reset_token.has_value());
      EXPECT_EQ(feed_url, details->url);
      EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
                details->last_fetch_result);
    }
  }

  if (auto* service = GetMediaFeedsService()) {
    service->ResetMediaFeed(url::Origin::Create(feed_url),
                            media_feeds::mojom::ResetReason::kCookies);
    WaitForDB();
  }

  base::Optional<base::UnguessableToken> token;
  {
    // The feed should have been reset and the token should have been generated.
    auto details = GetFetchDetailsSync(feed_id);

    if (IsReadOnly()) {
      EXPECT_FALSE(details.has_value());
    } else {
      EXPECT_TRUE(details->reset_token.has_value());
      EXPECT_EQ(feed_url, details->url);
      EXPECT_EQ(media_feeds::mojom::FetchResult::kNone,
                details->last_fetch_result);

      token = details->reset_token;
    }
  }

  if (auto* service = GetMediaFeedsService()) {
    service->ResetMediaFeed(url::Origin::Create(feed_url),
                            media_feeds::mojom::ResetReason::kVisit);
    WaitForDB();
  }

  {
    // A new token should have been generated.
    auto details = GetFetchDetailsSync(feed_id);

    if (IsReadOnly()) {
      EXPECT_FALSE(details.has_value());
    } else {
      EXPECT_TRUE(details->reset_token.has_value());
      EXPECT_NE(token, details->reset_token);
      EXPECT_EQ(feed_url, details->url);
      EXPECT_EQ(media_feeds::mojom::FetchResult::kNone,
                details->last_fetch_result);
    }
  }
}

TEST_P(MediaHistoryStoreFeedsTest, GetContinueWatching) {
  const GURL feed_url("https://www.google.com/feed");

  DiscoverMediaFeed(feed_url);
  WaitForDB();

  // If we are read only we should use -1 as a placeholder feed id because the
  // feed will not have been stored. This is so we can run the rest of the test
  // to ensure a no-op.
  const int feed_id = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[0]->id;

  service()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(feed_id, GetExpectedItems()),
      base::DoNothing());
  WaitForDB();

  {
    auto items = GetItemsForMediaFeedSync(
        service(), MediaHistoryKeyedService::GetMediaFeedItemsRequest::
                       CreateItemsForContinueWatching(5, false, base::nullopt));

    if (IsReadOnly()) {
      EXPECT_TRUE(items.empty());
    } else {
      // We should have the first item because it has play next details and the
      // second item because it has an active action status.
      ASSERT_EQ(2u, items.size());
      EXPECT_EQ(2, items[0]->id);
      EXPECT_EQ(1, items[1]->id);
    }
  }

  {
    auto items = GetItemsForMediaFeedSync(
        service(), MediaHistoryKeyedService::GetMediaFeedItemsRequest::
                       CreateItemsForContinueWatching(5, true, base::nullopt));

    if (IsReadOnly()) {
      EXPECT_TRUE(items.empty());
    } else {
      // We should only return the second item because it is the only one that
      // is safe.
      ASSERT_EQ(1u, items.size());
      EXPECT_EQ(2, items[0]->id);
    }
  }

  {
    auto items = GetItemsForMediaFeedSync(
        service(), MediaHistoryKeyedService::GetMediaFeedItemsRequest::
                       CreateItemsForContinueWatching(1, false, base::nullopt));

    if (IsReadOnly()) {
      EXPECT_TRUE(items.empty());
    } else {
      // We should only return the second item because we are limiting to one
      // item.
      ASSERT_EQ(1u, items.size());
      EXPECT_EQ(2, items[0]->id);
    }
  }

  {
    auto items = GetItemsForMediaFeedSync(
        service(),
        MediaHistoryKeyedService::GetMediaFeedItemsRequest::
            CreateItemsForContinueWatching(
                5, false, media_feeds::mojom::MediaFeedItemType::kMovie));

    if (IsReadOnly()) {
      EXPECT_TRUE(items.empty());
    } else {
      // We should only return the second item because we are limiting to
      // Movies.
      ASSERT_EQ(1u, items.size());
      EXPECT_EQ(1, items[0]->id);
      EXPECT_EQ(media_feeds::mojom::MediaFeedItemType::kMovie, items[0]->type);
    }
  }

  {
    auto items = GetItemsForMediaFeedSync(
        service(),
        MediaHistoryKeyedService::GetMediaFeedItemsRequest::
            CreateItemsForContinueWatching(
                5, false, media_feeds::mojom::MediaFeedItemType::kTVSeries));

    if (IsReadOnly()) {
      EXPECT_TRUE(items.empty());
    } else {
      // We should only return the second item because we are limiting to TV
      // series.
      ASSERT_EQ(1u, items.size());
      EXPECT_EQ(2, items[0]->id);
      EXPECT_EQ(media_feeds::mojom::MediaFeedItemType::kTVSeries,
                items[0]->type);
    }
  }
}

TEST_P(MediaHistoryStoreFeedsTest, GetItemsForFeed) {
  const GURL feed_url("https://www.google.com/feed");

  DiscoverMediaFeed(feed_url);
  WaitForDB();

  // If we are read only we should use -1 as a placeholder feed id because the
  // feed will not have been stored. This is so we can run the rest of the test
  // to ensure a no-op.
  const int feed_id = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[0]->id;

  service()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(feed_id, GetExpectedItems()),
      base::DoNothing());
  WaitForDB();

  {
    auto items = GetItemsForMediaFeedSync(
        service(),
        MediaHistoryKeyedService::GetMediaFeedItemsRequest::CreateItemsForFeed(
            1, 5, false, base::nullopt));

    if (IsReadOnly()) {
      EXPECT_TRUE(items.empty());
    } else {
      // We should have the third item because the others have continue
      // watching details are have been removed.
      ASSERT_EQ(1u, items.size());
      EXPECT_EQ(3, items[0]->id);
    }
  }

  {
    auto items = GetItemsForMediaFeedSync(
        service(),
        MediaHistoryKeyedService::GetMediaFeedItemsRequest::CreateItemsForFeed(
            1, 5, true, base::nullopt));

    // Do not return anything since all the feed items are "unsafe".
    EXPECT_TRUE(items.empty());
  }

  {
    auto items = GetItemsForMediaFeedSync(
        service(),
        MediaHistoryKeyedService::GetMediaFeedItemsRequest::CreateItemsForFeed(
            1, 0, false, base::nullopt));

    // Do not return anything since the limit is 0.
    EXPECT_TRUE(items.empty());
  }

  {
    auto items = GetItemsForMediaFeedSync(
        service(),
        MediaHistoryKeyedService::GetMediaFeedItemsRequest::CreateItemsForFeed(
            1, 5, false, media_feeds::mojom::MediaFeedItemType::kTVSeries));

    if (IsReadOnly()) {
      EXPECT_TRUE(items.empty());
    } else {
      // We should have the third item because the others have continue
      // watching details are have been removed and it is also a TV series.
      ASSERT_EQ(1u, items.size());
      EXPECT_EQ(3, items[0]->id);
      EXPECT_EQ(media_feeds::mojom::MediaFeedItemType::kTVSeries,
                items[0]->type);
    }
  }

  {
    auto items = GetItemsForMediaFeedSync(
        service(),
        MediaHistoryKeyedService::GetMediaFeedItemsRequest::CreateItemsForFeed(
            1, 5, false, media_feeds::mojom::MediaFeedItemType::kMovie));

    // Do not return anything since we don't have any movies.
    EXPECT_TRUE(items.empty());
  }
}

TEST_P(MediaHistoryStoreFeedsTest, GetSelectedFeedsForFetch) {
  const GURL feed_url_a("https://www.google.com/feed");
  const GURL feed_url_b("https://www.google.co.uk/feed");
  const GURL feed_url_c("https://www.google.co.tv/feed");

  DiscoverMediaFeed(feed_url_a);
  DiscoverMediaFeed(feed_url_b);
  DiscoverMediaFeed(feed_url_c);
  WaitForDB();

  // If we are read only we should use -1 as a placeholder feed id because the
  // feed will not have been stored. This is so we can run the rest of the test
  // to ensure a no-op.
  const int feed_id_a = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[0]->id;
  const int feed_id_b = IsReadOnly() ? -1 : GetMediaFeedsSync(service())[1]->id;

  service()->UpdateFeedUserStatus(
      feed_id_a, media_feeds::mojom::FeedUserStatus::kDisabled);
  service()->UpdateFeedUserStatus(feed_id_b,
                                  media_feeds::mojom::FeedUserStatus::kEnabled);
  WaitForDB();

  auto feeds = GetMediaFeedsSync(
      service(), MediaHistoryKeyedService::GetMediaFeedsRequest::
                     CreateSelectedFeedsForFetch());

  if (IsReadOnly()) {
    EXPECT_TRUE(feeds.empty());
  } else {
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(feed_id_b, feeds[0]->id);
  }
}

#endif  // !defined(OS_ANDROID)

}  // namespace media_history
