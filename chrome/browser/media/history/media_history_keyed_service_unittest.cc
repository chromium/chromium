// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_keyed_service.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/media/history/media_history_images_table.h"
#include "chrome/browser/media/history/media_history_origin_table.h"
#include "chrome/browser/media/history/media_history_playback_table.h"
#include "chrome/browser/media/history/media_history_session_images_table.h"
#include "chrome/browser/media/history/media_history_session_table.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/common/pref_names.h"
#include "components/history/core/test/test_history_database.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/media_player_watch_time.h"
#include "content/public/test/test_utils.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/cpp/media_image.h"
#include "services/media_session/public/cpp/media_metadata.h"
#include "services/media_session/public/cpp/media_position.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_history {

namespace {

std::unique_ptr<KeyedService> BuildTestHistoryService(
    scoped_refptr<base::SequencedTaskRunner> backend_runner,
    content::BrowserContext* context) {
  std::unique_ptr<history::HistoryService> service(
      new history::HistoryService());
  service->set_backend_task_runner_for_testing(std::move(backend_runner));
  service->Init(history::TestHistoryDatabaseParamsForPath(context->GetPath()));
  return service;
}

enum class TestState {
  kNormal,

  // Runs the test with the "SavingBrowserHistoryDisabled" policy enabled.
  kSavingBrowserHistoryDisabled,
};

}  // namespace

class MediaHistoryKeyedServiceTest
    : public testing::Test,
      public testing::WithParamInterface<TestState> {
 public:
  void SetUp() override {
    mock_time_task_runner_ =
        base::MakeRefCounted<base::TestMockTimeTaskRunner>();

    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        HistoryServiceFactory::GetInstance(),
        base::BindRepeating(&BuildTestHistoryService, mock_time_task_runner_));
    profile_ = builder.Build();

    // Sleep the thread to allow the media history store to asynchronously
    // create the database and tables.
    WaitForDB();
  }

  MediaHistoryKeyedService* service() {
    return MediaHistoryKeyedService::Get(profile());
  }

  Profile* profile() { return profile_.get(); }

  void TearDown() override {
    profile()->GetPrefs()->SetBoolean(prefs::kSavingBrowserHistoryDisabled,
                                      false);

    // Destroy the profile, which also stops the history service.
    profile_.reset();

    // Tests that run a history service that uses the mock task runner for
    // backend processing will post tasks there during TearDown. Run them now to
    // avoid leaks.
    mock_time_task_runner_->RunUntilIdle();
  }

  int GetUserDataTableRowCount() {
    int count = 0;
    mojom::MediaHistoryStatsPtr stats = GetStatsSync();

    for (auto& entry : stats->table_row_counts) {
      // The meta table should not count as it does not contain any user data.
      if (entry.first == "meta")
        continue;

      count += entry.second;
    }

    return count;
  }

  mojom::MediaHistoryStatsPtr GetStatsSync() {
    base::RunLoop run_loop;
    mojom::MediaHistoryStatsPtr stats_out;

    service()->GetMediaHistoryStats(base::BindOnce(
        [](mojom::MediaHistoryStatsPtr* stats_out,
           base::RepeatingClosure callback, mojom::MediaHistoryStatsPtr stats) {
          stats_out->Swap(&stats);
          std::move(callback).Run();
        },
        &stats_out, run_loop.QuitClosure()));

    run_loop.Run();
    return stats_out;
  }

  std::set<GURL> GetURLsInTable(const std::string& table) {
    base::RunLoop run_loop;
    std::set<GURL> out;

    service()->GetURLsInTableForTest(
        table, base::BindLambdaForTesting([&](std::set<GURL> urls) {
          out = urls;
          run_loop.Quit();
        }));

    run_loop.Run();
    return out;
  }

  void WaitForDB() {
    base::RunLoop run_loop;
    service()->PostTaskToDBForTest(run_loop.QuitClosure());
    run_loop.Run();
  }

  std::vector<media_session::MediaImage> CreateImageVector(const GURL& url) {
    std::vector<media_session::MediaImage> images;

    media_session::MediaImage image;
    image.src = url;
    image.sizes.push_back(gfx::Size(10, 10));
    image.sizes.push_back(gfx::Size(20, 20));
    images.push_back(image);

    return images;
  }

  void MaybeSetSavingBrowsingHistoryDisabled() {
    if (GetParam() != TestState::kSavingBrowserHistoryDisabled)
      return;

    profile()->GetPrefs()->SetBoolean(prefs::kSavingBrowserHistoryDisabled,
                                      true);
  }

  std::vector<mojom::MediaHistoryOriginRowPtr> GetOriginRowsSync() {
    base::RunLoop run_loop;
    std::vector<mojom::MediaHistoryOriginRowPtr> out;

    service()->GetOriginRowsForDebug(base::BindLambdaForTesting(
        [&](std::vector<mojom::MediaHistoryOriginRowPtr> rows) {
          out = std::move(rows);
          run_loop.Quit();
        }));

    run_loop.Run();
    return out;
  }

  scoped_refptr<base::TestMockTimeTaskRunner> mock_time_task_runner_;

 private:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    MediaHistoryKeyedServiceTest,
    testing::Values(TestState::kNormal,
                    TestState::kSavingBrowserHistoryDisabled));

TEST_P(MediaHistoryKeyedServiceTest, CleanUpDatabaseWhenHistoryIsDeleted) {
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile(), ServiceAccessType::IMPLICIT_ACCESS);
  GURL url("http://google.com/test");

  EXPECT_EQ(0, GetUserDataTableRowCount());

  // Record a playback in the database.
  {
    content::MediaPlayerWatchTime watch_time(
        url, url.DeprecatedGetOriginAsURL(), base::Milliseconds(123),
        base::Milliseconds(321), true, false);

    history->AddPage(url, base::Time::Now(), history::SOURCE_BROWSED);
    service()->SavePlayback(watch_time);

    // Wait until the playbacks have finished saving.
    WaitForDB();
  }

  EXPECT_EQ(2, GetUserDataTableRowCount());

  MaybeSetSavingBrowsingHistoryDisabled();

  {
    base::CancelableTaskTracker task_tracker;

    // Clear all history.
    history->ExpireHistoryBetween(std::set<GURL>(), base::Time(), base::Time(),
                                  /* user_initiated */ true, base::DoNothing(),
                                  &task_tracker);

    mock_time_task_runner_->RunUntilIdle();

    // Wait for the database to update.
    WaitForDB();
  }

  EXPECT_EQ(0, GetUserDataTableRowCount());
}

TEST_P(MediaHistoryKeyedServiceTest, CleanUpDatabaseWhenOriginIsDeleted) {
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile(), ServiceAccessType::IMPLICIT_ACCESS);

  GURL url1a("https://www.google.com/test1A");
  GURL url1b("https://www.google.com/test1B");
  GURL url1c("https://www.google.com/test1C");
  GURL url2a("https://example.com/test2A");
  GURL url2b("https://example.com/test2B");

  GURL media_feed_1("https://www.google.com/media-feed.json");
  GURL media_feed_2("https://example.com/media-feed.json");

  // Images associated with a media session do not need to be on the same origin
  // as where the playback happened.
  GURL url1a_image("https://gstatic.com/test1A.png");
  GURL url1b_image("https://www.google.com/test1B.png");
  GURL url2a_image("https://examplestatic.com/test2B.png");
  GURL shared_image("https://gstatic.com/shared.png");

  // Create a set that has all the URLs.
  std::set<GURL> all_urls;
  all_urls.insert(url1a);
  all_urls.insert(url1b);
  all_urls.insert(url1c);
  all_urls.insert(url2a);
  all_urls.insert(url2b);

  // Create a set that has the URLs that will not be deleted.
  std::set<GURL> remaining;
  remaining.insert(url2a);
  remaining.insert(url2b);

  // Create a set that has all the image URLs.
  std::set<GURL> images;
  images.insert(url1a_image);
  images.insert(url1b_image);
  images.insert(url2a_image);
  images.insert(shared_image);

  // Create a set that has the image URLs that will not be deleted.
  std::set<GURL> remaining_images;
  remaining_images.insert(url2a_image);
  remaining_images.insert(shared_image);

  // Create a set that has all the media feeds.
  std::set<GURL> media_feeds;
  media_feeds.insert(media_feed_1);
  media_feeds.insert(media_feed_2);

  // Create a set that has the remaining media feeds.
  std::set<GURL> remaining_media_feeds;
  remaining_media_feeds.insert(media_feed_2);

  // The tables should be empty.
  EXPECT_EQ(0, GetUserDataTableRowCount());

  // Record a playback in the database for |url1a|.
  {
    content::MediaPlayerWatchTime watch_time(
        url1a, url1a.DeprecatedGetOriginAsURL(), base::Milliseconds(123),
        base::Milliseconds(321), true, false);

    history->AddPage(url1a, base::Time::Now(), history::SOURCE_BROWSED);
    service()->SavePlayback(watch_time);

    service()->SavePlaybackSession(url1a, media_session::MediaMetadata(),
                                   absl::nullopt,
                                   CreateImageVector(url1a_image));
  }

  // Record a playback in the database for |url1b|.
  {
    content::MediaPlayerWatchTime watch_time(
        url1b, url1b.DeprecatedGetOriginAsURL(), base::Milliseconds(123),
        base::Milliseconds(321), true, false);

    history->AddPage(url1b, base::Time::Now(), history::SOURCE_BROWSED);
    service()->SavePlayback(watch_time);

    service()->SavePlaybackSession(url1b, media_session::MediaMetadata(),
                                   absl::nullopt,
                                   CreateImageVector(url1b_image));
  }

  // Record a playback in the database for |url1c|. However, we won't store it
  // in the history to ensure that the history service is clearing data at
  // origin-level.
  {
    content::MediaPlayerWatchTime watch_time(
        url1c, url1c.DeprecatedGetOriginAsURL(), base::Milliseconds(123),
        base::Milliseconds(321), true, false);
    service()->SavePlayback(watch_time);

    service()->SavePlaybackSession(url1c, media_session::MediaMetadata(),
                                   absl::nullopt,
                                   CreateImageVector(shared_image));
  }

  // Record a playback in the database for |url2a|.
  {
    content::MediaPlayerWatchTime watch_time(
        url2a, url2a.DeprecatedGetOriginAsURL(), base::Milliseconds(123),
        base::Milliseconds(321), true, false);

    history->AddPage(url2a, base::Time::Now(), history::SOURCE_BROWSED);
    service()->SavePlayback(watch_time);

    service()->SavePlaybackSession(url2a, media_session::MediaMetadata(),
                                   absl::nullopt,
                                   CreateImageVector(url2a_image));
  }

  // Record a playback in the database for |url2b|.
  {
    content::MediaPlayerWatchTime watch_time(
        url2b, url2b.DeprecatedGetOriginAsURL(), base::Milliseconds(123),
        base::Milliseconds(321), true, false);

    history->AddPage(url2b, base::Time::Now(), history::SOURCE_BROWSED);
    service()->SavePlayback(watch_time);

    service()->SavePlaybackSession(url2b, media_session::MediaMetadata(),
                                   absl::nullopt,
                                   CreateImageVector(shared_image));
  }

  // Wait until the playbacks have finished saving.
  WaitForDB();

  {
    // Check that the tables have the right count in them.
    mojom::MediaHistoryStatsPtr stats = GetStatsSync();
    EXPECT_EQ(2, stats->table_row_counts[MediaHistoryOriginTable::kTableName]);
    EXPECT_EQ(5,
              stats->table_row_counts[MediaHistoryPlaybackTable::kTableName]);
    EXPECT_EQ(5, stats->table_row_counts[MediaHistorySessionTable::kTableName]);

    // There are 10 session images because each session has an image with two
    // sizes.
    EXPECT_EQ(
        10,
        stats->table_row_counts[MediaHistorySessionImagesTable::kTableName]);
    EXPECT_EQ(5, stats->table_row_counts[MediaHistoryImagesTable::kTableName]);
  }

  // Check the URLs are present in the tables.
  EXPECT_EQ(all_urls, GetURLsInTable(MediaHistoryPlaybackTable::kTableName));
  EXPECT_EQ(all_urls, GetURLsInTable(MediaHistorySessionTable::kTableName));
  EXPECT_EQ(images, GetURLsInTable(MediaHistoryImagesTable::kTableName));

  MaybeSetSavingBrowsingHistoryDisabled();

  {
    base::CancelableTaskTracker task_tracker;

    // Expire url1a and url1b.
    std::vector<history::ExpireHistoryArgs> expire_list;
    history::ExpireHistoryArgs args;
    args.urls.insert(url1a);
    args.urls.insert(url1b);
    args.SetTimeRangeForOneDay(base::Time::Now());
    expire_list.push_back(args);

    history->ExpireHistory(expire_list, base::DoNothing(), &task_tracker);
    mock_time_task_runner_->RunUntilIdle();

    // Wait for the database to update.
    WaitForDB();
  }

  {
    // Check that the tables have the right count in them.
    mojom::MediaHistoryStatsPtr stats = GetStatsSync();
    EXPECT_EQ(1, stats->table_row_counts[MediaHistoryOriginTable::kTableName]);
    EXPECT_EQ(2,
              stats->table_row_counts[MediaHistoryPlaybackTable::kTableName]);
    EXPECT_EQ(2, stats->table_row_counts[MediaHistorySessionTable::kTableName]);

    // There are 4 session images because each session has an image with two
    // sizes.
    EXPECT_EQ(
        4, stats->table_row_counts[MediaHistorySessionImagesTable::kTableName]);
    EXPECT_EQ(2, stats->table_row_counts[MediaHistoryImagesTable::kTableName]);
  }

  // Check we only have the remaining URLs in the tables.
  EXPECT_EQ(remaining, GetURLsInTable(MediaHistoryPlaybackTable::kTableName));
  EXPECT_EQ(remaining, GetURLsInTable(MediaHistorySessionTable::kTableName));
  EXPECT_EQ(remaining_images,
            GetURLsInTable(MediaHistoryImagesTable::kTableName));
}

TEST_P(MediaHistoryKeyedServiceTest,
       CleanUpDatabaseWhenOriginIsDeleted_NotMedia) {
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile(), ServiceAccessType::IMPLICIT_ACCESS);

  GURL url1a("https://www.google.com/test1A");
  GURL url1b("https://www.google.com/test1B");
  GURL url1c("https://www.google.com/test1C");
  GURL url2a("https://example.com/test2A");
  GURL url2b("https://example.com/test2B");
  GURL url3("https://www.example.org");

  GURL media_feed_1("https://www.google.com/media-feed.json");
  GURL media_feed_2("https://example.com/media-feed.json");

  // Images associated with a media session do not need to be on the same origin
  // as where the playback happened.
  GURL url1a_image("https://gstatic.com/test1A.png");
  GURL url1b_image("https://www.google.com/test1B.png");
  GURL url2a_image("https://examplestatic.com/test2B.png");
  GURL shared_image("https://gstatic.com/shared.png");

  // Create a set that has all the URLs.
  std::set<GURL> all_urls;
  all_urls.insert(url1a);
  all_urls.insert(url1b);
  all_urls.insert(url1c);
  all_urls.insert(url2a);
  all_urls.insert(url2b);

  // Create a set that has the URLs that will not be deleted.
  std::set<GURL> remaining;
  remaining.insert(url1b);
  remaining.insert(url1c);
  remaining.insert(url2a);
  remaining.insert(url2b);

  // Create a set that has all the image URLs.
  std::set<GURL> images;
  images.insert(url1a_image);
  images.insert(url1b_image);
  images.insert(url2a_image);
  images.insert(shared_image);

  // Create a set that has the image URLs that will not be deleted.
  std::set<GURL> remaining_images;
  remaining_images.insert(url1b_image);
  remaining_images.insert(url2a_image);
  remaining_images.insert(shared_image);

  // Create a set that has all the media feeds.
  std::set<GURL> media_feeds;
  media_feeds.insert(media_feed_1);
  media_feeds.insert(media_feed_2);

  // The tables should be empty.
  EXPECT_EQ(0, GetUserDataTableRowCount());

  // Record a playback in the database for |url1a|.
  {
    content::MediaPlayerWatchTime watch_time(
        url1a, url1a.DeprecatedGetOriginAsURL(), base::Milliseconds(123),
        base::Milliseconds(321), true, false);

    history->AddPage(url1a, base::Time::Now(), history::SOURCE_BROWSED);
    service()->SavePlayback(watch_time);

    service()->SavePlaybackSession(url1a, media_session::MediaMetadata(),
                                   absl::nullopt,
                                   CreateImageVector(url1a_image));
  }

  // Record a playback in the database for |url1b|.
  {
    content::MediaPlayerWatchTime watch_time(
        url1b, url1b.DeprecatedGetOriginAsURL(), base::Milliseconds(123),
        base::Milliseconds(321), true, false);

    history->AddPage(url1b, base::Time::Now(), history::SOURCE_BROWSED);
    service()->SavePlayback(watch_time);

    service()->SavePlaybackSession(url1b, media_session::MediaMetadata(),
                                   absl::nullopt,
                                   CreateImageVector(url1b_image));
  }

  // Record a playback in the database for |url1c|. However, we won't store it
  // in the history to ensure that the history service is clearing data at
  // origin-level.
  {
    content::MediaPlayerWatchTime watch_time(
        url1c, url1c.DeprecatedGetOriginAsURL(), base::Milliseconds(123),
        base::Milliseconds(321), true, false);
    service()->SavePlayback(watch_time);

    service()->SavePlaybackSession(url1c, media_session::MediaMetadata(),
                                   absl::nullopt,
                                   CreateImageVector(shared_image));
  }

  // Record a playback in the database for |url2a|.
  {
    content::MediaPlayerWatchTime watch_time(
        url2a, url2a.DeprecatedGetOriginAsURL(), base::Milliseconds(123),
        base::Milliseconds(321), true, false);

    history->AddPage(url2a, base::Time::Now(), history::SOURCE_BROWSED);
    service()->SavePlayback(watch_time);

    service()->SavePlaybackSession(url2a, media_session::MediaMetadata(),
                                   absl::nullopt,
                                   CreateImageVector(url2a_image));
  }

  // Record a playback in the database for |url2b|.
  {
    content::MediaPlayerWatchTime watch_time(
        url2b, url2b.DeprecatedGetOriginAsURL(), base::Milliseconds(123),
        base::Milliseconds(321), true, false);

    history->AddPage(url2b, base::Time::Now(), history::SOURCE_BROWSED);
    service()->SavePlayback(watch_time);

    service()->SavePlaybackSession(url2b, media_session::MediaMetadata(),
                                   absl::nullopt,
                                   CreateImageVector(shared_image));
  }

  // Record a visit for |url3|.
  history->AddPage(url3, base::Time::Now(), history::SOURCE_BROWSED);

  // Wait until the playbacks have finished saving.
  WaitForDB();

  {
    // Check that the tables have the right count in them.
    mojom::MediaHistoryStatsPtr stats = GetStatsSync();
    EXPECT_EQ(2, stats->table_row_counts[MediaHistoryOriginTable::kTableName]);
    EXPECT_EQ(5,
              stats->table_row_counts[MediaHistoryPlaybackTable::kTableName]);
    EXPECT_EQ(5, stats->table_row_counts[MediaHistorySessionTable::kTableName]);

    // There are 10 session images because each session has an image with two
    // sizes.
    EXPECT_EQ(
        10,
        stats->table_row_counts[MediaHistorySessionImagesTable::kTableName]);
    EXPECT_EQ(5, stats->table_row_counts[MediaHistoryImagesTable::kTableName]);
  }

  // Check the URLs are present in the tables.
  EXPECT_EQ(all_urls, GetURLsInTable(MediaHistoryPlaybackTable::kTableName));
  EXPECT_EQ(all_urls, GetURLsInTable(MediaHistorySessionTable::kTableName));
  EXPECT_EQ(images, GetURLsInTable(MediaHistoryImagesTable::kTableName));

  MaybeSetSavingBrowsingHistoryDisabled();

  {
    base::CancelableTaskTracker task_tracker;

    // Expire url1a and url3.
    std::vector<history::ExpireHistoryArgs> expire_list;
    history::ExpireHistoryArgs args;
    args.urls.insert(url1a);
    args.urls.insert(url3);
    args.SetTimeRangeForOneDay(base::Time::Now());
    expire_list.push_back(args);

    history->ExpireHistory(expire_list, base::DoNothing(), &task_tracker);
    mock_time_task_runner_->RunUntilIdle();

    // Wait for the database to update.
    WaitForDB();
  }

  {
    // Check that the tables have the right count in them.
    mojom::MediaHistoryStatsPtr stats = GetStatsSync();
    EXPECT_EQ(2, stats->table_row_counts[MediaHistoryOriginTable::kTableName]);
    EXPECT_EQ(4,
              stats->table_row_counts[MediaHistoryPlaybackTable::kTableName]);
    EXPECT_EQ(4, stats->table_row_counts[MediaHistorySessionTable::kTableName]);

    // There are 8 session images because each session has an image with two
    // sizes.
    EXPECT_EQ(
        8, stats->table_row_counts[MediaHistorySessionImagesTable::kTableName]);
    EXPECT_EQ(4, stats->table_row_counts[MediaHistoryImagesTable::kTableName]);
  }

  // Check we only have the remaining URLs in the tables.
  EXPECT_EQ(remaining, GetURLsInTable(MediaHistoryPlaybackTable::kTableName));
  EXPECT_EQ(remaining, GetURLsInTable(MediaHistorySessionTable::kTableName));
  EXPECT_EQ(remaining_images,
            GetURLsInTable(MediaHistoryImagesTable::kTableName));
}

TEST_P(MediaHistoryKeyedServiceTest, CleanUpDatabaseWhenURLIsDeleted) {
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile(), ServiceAccessType::IMPLICIT_ACCESS);

  GURL url1a("https://www.google.com/test1A");
  GURL url1b("https://www.google.com/test1B");
  GURL url1c("https://www.google.com/test1C");
  GURL url2a("https://example.com/test2A");
  GURL url2b("https://example.com/test2B");

  // Images associated with a media session do not need to be on the same origin
  // as where the playback happened.
  GURL url1a_image("https://gstatic.com/test1A.png");
  GURL url1c_image("https://www.google.com/test1C.png");
  GURL url2a_image("https://examplestatic.com/test2B.png");
  GURL shared_image("https://gstatic.com/shared.png");

  GURL media_feed_1("https://www.google.com/media-feed.json");
  GURL media_feed_2("https://example.com/media-feed.json");

  // Create a set that has all the URLs.
  std::set<GURL> all_urls;
  all_urls.insert(url1a);
  all_urls.insert(url1b);
  all_urls.insert(url1c);
  all_urls.insert(url2a);
  all_urls.insert(url2b);

  // Create a set that has the URLs that will not be deleted.
  std::set<GURL> remaining;
  remaining.insert(url1c);
  remaining.insert(url2a);
  remaining.insert(url2b);

  // Create a set that has all the image URLs.
  std::set<GURL> images;
  images.insert(url1a_image);
  images.insert(url1c_image);
  images.insert(url2a_image);
  images.insert(shared_image);

  // Create a set that has the image URLs that will not be deleted.
  std::set<GURL> remaining_images;
  remaining_images.insert(url1c_image);
  remaining_images.insert(url2a_image);
  remaining_images.insert(shared_image);

  // Create a set that has all the media feeds.
  std::set<GURL> media_feeds;
  media_feeds.insert(media_feed_1);
  media_feeds.insert(media_feed_2);

  // The tables should be empty.
  EXPECT_EQ(0, GetUserDataTableRowCount());

  // Record a playback in the database for |url1a|.
  {
    content::MediaPlayerWatchTime watch_time(
        url1a, url1a.DeprecatedGetOriginAsURL(), base::Minutes(10),
        base::Milliseconds(321), true, true);

    history->AddPage(url1a, base::Time::Now(), history::SOURCE_BROWSED);
    service()->SavePlayback(watch_time);

    service()->SavePlaybackSession(url1a, media_session::MediaMetadata(),
                                   absl::nullopt,
                                   CreateImageVector(url1a_image));
  }

  // Record a playback in the database for |url1b|.
  {
    content::MediaPlayerWatchTime watch_time(
        url1b, url1b.DeprecatedGetOriginAsURL(), base::Minutes(25),
        base::Milliseconds(321), true, true);

    history->AddPage(url1b, base::Time::Now(), history::SOURCE_BROWSED);
    service()->SavePlayback(watch_time);

    service()->SavePlaybackSession(url1b, media_session::MediaMetadata(),
                                   absl::nullopt,
                                   CreateImageVector(shared_image));
  }

  // Record a playback in the database for |url1c|.
  {
    content::MediaPlayerWatchTime watch_time(
        url1c, url1c.DeprecatedGetOriginAsURL(), base::Milliseconds(123),
        base::Milliseconds(321), true, false);

    history->AddPage(url1c, base::Time::Now(), history::SOURCE_BROWSED);
    service()->SavePlayback(watch_time);

    service()->SavePlaybackSession(url1c, media_session::MediaMetadata(),
                                   absl::nullopt,
                                   CreateImageVector(url1c_image));
  }

  // Record a playback in the database for |url2a|.
  {
    content::MediaPlayerWatchTime watch_time(
        url2a, url2a.DeprecatedGetOriginAsURL(), base::Minutes(10),
        base::Milliseconds(321), true, true);

    history->AddPage(url2a, base::Time::Now(), history::SOURCE_BROWSED);
    service()->SavePlayback(watch_time);

    service()->SavePlaybackSession(url2a, media_session::MediaMetadata(),
                                   absl::nullopt,
                                   CreateImageVector(url2a_image));
  }

  // Record a playback in the database for |url2b|.
  {
    content::MediaPlayerWatchTime watch_time(
        url2b, url2b.DeprecatedGetOriginAsURL(), base::Minutes(20),
        base::Milliseconds(321), true, true);

    history->AddPage(url2b, base::Time::Now(), history::SOURCE_BROWSED);
    service()->SavePlayback(watch_time);

    service()->SavePlaybackSession(url2b, media_session::MediaMetadata(),
                                   absl::nullopt,
                                   CreateImageVector(shared_image));
  }

  // Wait until the playbacks have finished saving.
  WaitForDB();

  {
    // Check that the tables have the right count in them.
    mojom::MediaHistoryStatsPtr stats = GetStatsSync();
    EXPECT_EQ(2, stats->table_row_counts[MediaHistoryOriginTable::kTableName]);
    EXPECT_EQ(5,
              stats->table_row_counts[MediaHistoryPlaybackTable::kTableName]);
    EXPECT_EQ(5, stats->table_row_counts[MediaHistorySessionTable::kTableName]);

    // There are 10 session images because each session has an image with two
    // sizes.
    EXPECT_EQ(
        10,
        stats->table_row_counts[MediaHistorySessionImagesTable::kTableName]);
    EXPECT_EQ(5, stats->table_row_counts[MediaHistoryImagesTable::kTableName]);
  }

  // Check the URLs are present in the tables.
  EXPECT_EQ(all_urls, GetURLsInTable(MediaHistoryPlaybackTable::kTableName));
  EXPECT_EQ(all_urls, GetURLsInTable(MediaHistorySessionTable::kTableName));
  EXPECT_EQ(images, GetURLsInTable(MediaHistoryImagesTable::kTableName));

  // Check the origins have the correct aggregate watchtime.
  {
    auto origins = GetOriginRowsSync();
    ASSERT_EQ(2u, origins.size());

    EXPECT_EQ(base::Minutes(35), origins[0]->cached_audio_video_watchtime);
    EXPECT_EQ(origins[0]->actual_audio_video_watchtime,
              origins[0]->cached_audio_video_watchtime);

    EXPECT_EQ(base::Minutes(30), origins[1]->cached_audio_video_watchtime);
    EXPECT_EQ(origins[1]->actual_audio_video_watchtime,
              origins[1]->cached_audio_video_watchtime);
  }

  MaybeSetSavingBrowsingHistoryDisabled();

  {
    base::CancelableTaskTracker task_tracker;

    // Expire url1a and url1b.
    std::vector<history::ExpireHistoryArgs> expire_list;
    history::ExpireHistoryArgs args;
    args.urls.insert(url1a);
    args.urls.insert(url1b);
    args.SetTimeRangeForOneDay(base::Time::Now());
    expire_list.push_back(args);

    history->ExpireHistory(expire_list, base::DoNothing(), &task_tracker);
    mock_time_task_runner_->RunUntilIdle();

    // Wait for the database to update.
    WaitForDB();
  }

  {
    // Check that the tables have the right count in them.
    mojom::MediaHistoryStatsPtr stats = GetStatsSync();
    EXPECT_EQ(2, stats->table_row_counts[MediaHistoryOriginTable::kTableName]);
    EXPECT_EQ(3,
              stats->table_row_counts[MediaHistoryPlaybackTable::kTableName]);
    EXPECT_EQ(3, stats->table_row_counts[MediaHistorySessionTable::kTableName]);

    // There are 6 session images because each session has an image with two
    // sizes.
    EXPECT_EQ(
        6, stats->table_row_counts[MediaHistorySessionImagesTable::kTableName]);
    EXPECT_EQ(3, stats->table_row_counts[MediaHistoryImagesTable::kTableName]);
  }

  // Check we only have the remaining URLs in the tables.
  EXPECT_EQ(remaining, GetURLsInTable(MediaHistoryPlaybackTable::kTableName));
  EXPECT_EQ(remaining, GetURLsInTable(MediaHistorySessionTable::kTableName));
  EXPECT_EQ(remaining_images,
            GetURLsInTable(MediaHistoryImagesTable::kTableName));

  // Check the origins have the correct aggregate watchtime.
  {
    auto origins = GetOriginRowsSync();
    ASSERT_EQ(2u, origins.size());

    EXPECT_EQ(base::TimeDelta(), origins[0]->cached_audio_video_watchtime);
    EXPECT_EQ(origins[0]->actual_audio_video_watchtime,
              origins[0]->cached_audio_video_watchtime);

    EXPECT_EQ(base::Minutes(30), origins[1]->cached_audio_video_watchtime);
    EXPECT_EQ(origins[1]->actual_audio_video_watchtime,
              origins[1]->cached_audio_video_watchtime);
  }
}

TEST_P(MediaHistoryKeyedServiceTest, SecurityRegressionTest) {
  history::URLRows urls_to_delete = {
      history::URLRow(GURL("https://www.google.com/test1A"))};
  history::DeletionInfo deletion_info =
      history::DeletionInfo::ForUrls(urls_to_delete, std::set<GURL>());
  deletion_info.set_deleted_urls_origin_map({
      {GURL("https://www.google.com/test1B"), {0, base::Time::Now()}},
      {GURL("https://www.google.com/test1C"), {0, base::Time::Now()}},
  });

  service()->OnURLsDeleted(nullptr, deletion_info);
}

}  // namespace media_history
