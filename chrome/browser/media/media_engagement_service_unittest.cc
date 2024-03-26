// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/media/media_engagement_score.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/test_history_database.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

base::FilePath g_temp_history_dir;

// History is automatically expired after 90 days.
base::TimeDelta kHistoryExpirationThreshold = base::Days(90);

// Waits until a change is observed in media engagement content settings.
class MediaEngagementChangeWaiter : public content_settings::Observer {
 public:
  explicit MediaEngagementChangeWaiter(Profile* profile) : profile_(profile) {
    HostContentSettingsMapFactory::GetForProfile(profile)->AddObserver(this);
  }

  MediaEngagementChangeWaiter(const MediaEngagementChangeWaiter&) = delete;
  MediaEngagementChangeWaiter& operator=(const MediaEngagementChangeWaiter&) =
      delete;

  ~MediaEngagementChangeWaiter() override {
    HostContentSettingsMapFactory::GetForProfile(profile_)->RemoveObserver(
        this);
  }

  // Overridden from content_settings::Observer:
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override {
    if (content_type_set.Contains(ContentSettingsType::MEDIA_ENGAGEMENT))
      Proceed();
  }

  void Wait() { run_loop_.Run(); }

 private:
  void Proceed() { run_loop_.Quit(); }

  raw_ptr<Profile> profile_;
  base::RunLoop run_loop_;
};

base::Time GetReferenceTime() {
  static constexpr base::Time::Exploded kReferenceTime = {.year = 2015,
                                                          .month = 1,
                                                          .day_of_week = 5,
                                                          .day_of_month = 30,
                                                          .hour = 11};
  base::Time out_time;
  EXPECT_TRUE(base::Time::FromLocalExploded(kReferenceTime, &out_time));
  return out_time;
}

std::unique_ptr<KeyedService> BuildTestHistoryService(
    scoped_refptr<base::SequencedTaskRunner> backend_runner,
    content::BrowserContext* context) {
  std::unique_ptr<history::HistoryService> service(
      new history::HistoryService());
  if (backend_runner)
    service->set_backend_task_runner_for_testing(std::move(backend_runner));
  service->Init(history::TestHistoryDatabaseParamsForPath(g_temp_history_dir));
  return service;
}

// Blocks until the HistoryBackend is completely destroyed, to ensure the
// destruction tasks do not interfere with a newer instance of
// HistoryService/HistoryBackend.
void BlockUntilHistoryBackendDestroyed(Profile* profile) {
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfileWithoutCreating(profile);

  // Nothing to destroy
  if (!history_service)
    return;

  base::RunLoop run_loop;
  history_service->SetOnBackendDestroyTask(run_loop.QuitClosure());
  HistoryServiceFactory::ShutdownForProfile(profile);
  run_loop.Run();
}

}  // namespace

class MediaEngagementServiceTest : public ChromeRenderViewHostTestHarness,
                                   public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    mock_time_task_runner_ =
        base::MakeRefCounted<base::TestMockTimeTaskRunner>();

    if (GetParam()) {
      scoped_feature_list_.InitWithFeatures(
          {media::kRecordMediaEngagementScores,
           media::kMediaEngagementHTTPSOnly},
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {media::kRecordMediaEngagementScores},
          {media::kMediaEngagementHTTPSOnly});
    }
    ChromeRenderViewHostTestHarness::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    g_temp_history_dir = temp_dir_.GetPath();
    ConfigureHistoryService(nullptr);

    test_clock_.SetNow(GetReferenceTime());
    service_ = base::WrapUnique(StartNewMediaEngagementService());
  }

  MediaEngagementService* service() const { return service_.get(); }

  MediaEngagementService* StartNewMediaEngagementService() {
    MediaEngagementService* service =
        new MediaEngagementService(profile(), &test_clock_);
    base::RunLoop().RunUntilIdle();
    return service;
  }

  void ConfigureHistoryService(
      scoped_refptr<base::SequencedTaskRunner> backend_runner) {
    HistoryServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildTestHistoryService,
                                       std::move(backend_runner)));
  }

  // Properly shuts down the HistoryService associated with |profile()| and then
  // creates new one that will run using the |backend_runner|.
  void RestartHistoryService(
      scoped_refptr<base::SequencedTaskRunner> backend_runner) {
    // Triggers destruction of the existing HistoryService and waits for all
    // cleanup work to be done.
    service()->SetHistoryServiceForTesting(nullptr);
    BlockUntilHistoryBackendDestroyed(profile());

    // Force the creation of a new HistoryService that runs its backend on
    // |backend_runner|.
    ConfigureHistoryService(std::move(backend_runner));
    history::HistoryService* history = HistoryServiceFactory::GetForProfile(
        profile(), ServiceAccessType::IMPLICIT_ACCESS);
    service()->SetHistoryServiceForTesting(history);
  }

  void RecordVisitAndPlaybackAndAdvanceClock(const url::Origin& origin) {
    RecordVisit(origin);
    AdvanceClock();
    RecordPlayback(origin);
  }

  void TearDown() override {
    service_->Shutdown();

    // Tests that run a history service that uses the mock task runner for
    // backend processing will post tasks there during TearDown. Run them now to
    // avoid leaks.
    mock_time_task_runner_->RunUntilIdle();
    service_.reset();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  void AdvanceClock() { test_clock_.SetNow(Now() + base::Hours(1)); }

  void RecordVisit(const url::Origin& origin) { service_->RecordVisit(origin); }

  void RecordPlayback(const url::Origin& origin) {
    RecordPlaybackForService(service_.get(), origin);
  }

  void RecordPlaybackForService(MediaEngagementService* service,
                                const url::Origin& origin) {
    MediaEngagementScore score = service->CreateEngagementScore(origin);
    score.IncrementMediaPlaybacks();
    score.set_last_media_playback_time(service->clock()->Now());
    score.Commit();
  }

  void ExpectScores(MediaEngagementService* service,
                    const url::Origin& origin,
                    double expected_score,
                    int expected_visits,
                    int expected_media_playbacks,
                    base::Time expected_last_media_playback_time) {
    EXPECT_EQ(service->GetEngagementScore(origin), expected_score);
    EXPECT_EQ(service->GetScoreMapForTesting()[origin], expected_score);

    MediaEngagementScore score = service->CreateEngagementScore(origin);
    EXPECT_EQ(expected_visits, score.visits());
    EXPECT_EQ(expected_media_playbacks, score.media_playbacks());
    EXPECT_EQ(expected_last_media_playback_time,
              score.last_media_playback_time());
  }

  void ExpectScores(const url::Origin& origin,
                    double expected_score,
                    int expected_visits,
                    int expected_media_playbacks,
                    base::Time expected_last_media_playback_time) {
    ExpectScores(service_.get(), origin, expected_score, expected_visits,
                 expected_media_playbacks, expected_last_media_playback_time);
  }

  void SetScores(const url::Origin& origin, int visits, int media_playbacks) {
    MediaEngagementScore score = service_->CreateEngagementScore(origin);
    score.SetVisits(visits);
    score.SetMediaPlaybacks(media_playbacks);
    score.Commit();
  }

  void SetLastMediaPlaybackTime(const url::Origin& origin,
                                base::Time last_media_playback_time) {
    MediaEngagementScore score = service_->CreateEngagementScore(origin);
    score.last_media_playback_time_ = last_media_playback_time;
    score.Commit();
  }

  double GetActualScore(const url::Origin& origin) {
    return service_->CreateEngagementScore(origin).actual_score();
  }

  std::map<url::Origin, double> GetScoreMapForTesting() const {
    return service_->GetScoreMapForTesting();
  }

  void ClearDataBetweenTime(base::Time begin, base::Time end) {
    service_->ClearDataBetweenTime(begin, end);
  }

  base::Time Now() { return test_clock_.Now(); }

  base::Time TimeNotSet() const { return base::Time(); }

  void SetNow(base::Time now) { test_clock_.SetNow(now); }

  std::vector<media::mojom::MediaEngagementScoreDetailsPtr> GetAllScoreDetails()
      const {
    return service_->GetAllScoreDetails();
  }

  bool HasHighEngagement(const url::Origin& origin) const {
    return service_->HasHighEngagement(origin);
  }

  void SetSchemaVersion(int version) { service_->SetSchemaVersion(version); }

  std::vector<MediaEngagementScore> GetAllStoredScores(
      const MediaEngagementService* service) const {
    return service->GetAllStoredScores();
  }

  std::vector<MediaEngagementScore> GetAllStoredScores() const {
    return GetAllStoredScores(service_.get());
  }

 protected:
  scoped_refptr<base::TestMockTimeTaskRunner> mock_time_task_runner_;

 private:
  base::ScopedTempDir temp_dir_;

  base::SimpleTestClock test_clock_;

  std::unique_ptr<MediaEngagementService> service_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(MediaEngagementServiceTest, MojoSerialization) {
  EXPECT_EQ(0u, GetAllScoreDetails().size());

  RecordVisitAndPlaybackAndAdvanceClock(
      url::Origin::Create(GURL("http://www.example.com")));
  RecordVisitAndPlaybackAndAdvanceClock(
      url::Origin::Create(GURL("https://www.example.com")));

  EXPECT_EQ(GetParam() ? 1u : 2u, GetAllScoreDetails().size());
}

TEST_P(MediaEngagementServiceTest, RestrictedToHTTPAndHTTPS) {
  std::vector<url::Origin> origins = {
      url::Origin::Create(GURL("ftp://www.google.com/")),
      url::Origin::Create(GURL("file://blah")),
      url::Origin::Create(GURL("chrome://")),
      url::Origin::Create(GURL("about://config")),
      url::Origin::Create(GURL("http://example.com")),
      url::Origin::Create(GURL("https://example.com")),
  };

  for (const url::Origin& origin : origins) {
    RecordVisitAndPlaybackAndAdvanceClock(origin);

    if (origin.scheme() == url::kHttpsScheme ||
        (origin.scheme() == url::kHttpScheme && !GetParam())) {
      ExpectScores(origin, 0.05, 1, 1, Now());
    } else {
      ExpectScores(origin, 0.0, 0, 0, TimeNotSet());
    }
  }
}

TEST_P(MediaEngagementServiceTest,
       HandleRecordVisitAndPlaybackAndAdvanceClockion) {
  url::Origin origin1 = url::Origin::Create(GURL("https://www.google.com"));
  ExpectScores(origin1, 0.0, 0, 0, TimeNotSet());
  RecordVisitAndPlaybackAndAdvanceClock(origin1);
  ExpectScores(origin1, 0.05, 1, 1, Now());

  RecordVisit(origin1);
  ExpectScores(origin1, 0.05, 2, 1, Now());

  RecordPlayback(origin1);
  ExpectScores(origin1, 0.1, 2, 2, Now());
  base::Time origin1_time = Now();

  url::Origin origin2 = url::Origin::Create(GURL("https://www.google.co.uk"));
  RecordVisitAndPlaybackAndAdvanceClock(origin2);
  ExpectScores(origin2, 0.05, 1, 1, Now());
  ExpectScores(origin1, 0.1, 2, 2, origin1_time);
}

TEST_P(MediaEngagementServiceTest, IncognitoEngagementService) {
  url::Origin origin1 = url::Origin::Create(GURL("https://www.google.fr/"));
  url::Origin origin2 = url::Origin::Create(GURL("https://www.google.com/"));
  url::Origin origin3 = url::Origin::Create(GURL("https://drive.google.com/"));
  url::Origin origin4 = url::Origin::Create(GURL("https://maps.google.com/"));

  RecordVisitAndPlaybackAndAdvanceClock(origin1);
  base::Time origin1_time = Now();
  RecordVisitAndPlaybackAndAdvanceClock(origin2);

  MediaEngagementService* incognito_service = MediaEngagementService::Get(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  ExpectScores(incognito_service, origin1, 0.05, 1, 1, origin1_time);
  ExpectScores(incognito_service, origin2, 0.05, 1, 1, Now());
  ExpectScores(incognito_service, origin3, 0.0, 0, 0, TimeNotSet());

  incognito_service->RecordVisit(origin3);
  ExpectScores(incognito_service, origin3, 0.0, 1, 0, TimeNotSet());
  ExpectScores(origin3, 0.0, 0, 0, TimeNotSet());

  incognito_service->RecordVisit(origin2);
  ExpectScores(incognito_service, origin2, 0.05, 2, 1, Now());
  ExpectScores(origin2, 0.05, 1, 1, Now());

  RecordVisitAndPlaybackAndAdvanceClock(origin3);
  ExpectScores(incognito_service, origin3, 0.0, 1, 0, TimeNotSet());
  ExpectScores(origin3, 0.05, 1, 1, Now());

  ExpectScores(incognito_service, origin4, 0.0, 0, 0, TimeNotSet());
  RecordVisitAndPlaybackAndAdvanceClock(origin4);
  ExpectScores(incognito_service, origin4, 0.05, 1, 1, Now());
  ExpectScores(origin4, 0.05, 1, 1, Now());
}

TEST_P(MediaEngagementServiceTest, IncognitoOverrideRegularProfile) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://example.org"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://example.com"));

  SetScores(kOrigin1, MediaEngagementScore::GetScoreMinVisits(), 1);
  SetScores(kOrigin2, 1, 0);

  ExpectScores(kOrigin1, 0.05, MediaEngagementScore::GetScoreMinVisits(), 1,
               TimeNotSet());
  ExpectScores(kOrigin2, 0.0, 1, 0, TimeNotSet());

  MediaEngagementService* incognito_service = MediaEngagementService::Get(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  ExpectScores(incognito_service, kOrigin1, 0.05,
               MediaEngagementScore::GetScoreMinVisits(), 1, TimeNotSet());
  ExpectScores(incognito_service, kOrigin2, 0.0, 1, 0, TimeNotSet());

  // Scores should be the same in incognito and regular profile.
  {
    std::vector<std::pair<url::Origin, double>> kExpectedResults = {
        {kOrigin2, 0.0},
        {kOrigin1, 0.05},
    };

    const auto& scores = GetAllStoredScores();
    const auto& incognito_scores = GetAllStoredScores(incognito_service);

    EXPECT_EQ(kExpectedResults.size(), scores.size());
    EXPECT_EQ(kExpectedResults.size(), incognito_scores.size());

    for (size_t i = 0; i < scores.size(); ++i) {
      EXPECT_EQ(kExpectedResults[i].first, scores[i].origin());
      EXPECT_EQ(kExpectedResults[i].second, scores[i].actual_score());

      EXPECT_EQ(kExpectedResults[i].first, incognito_scores[i].origin());
      EXPECT_EQ(kExpectedResults[i].second, incognito_scores[i].actual_score());
    }
  }

  incognito_service->RecordVisit(kOrigin1);
  RecordPlaybackForService(incognito_service, kOrigin2);

  // Score shouldn't have changed in regular profile.
  {
    std::vector<std::pair<url::Origin, double>> kExpectedResults = {
        {kOrigin2, 0.0},
        {kOrigin1, 0.05},
    };

    const auto& scores = GetAllStoredScores();
    EXPECT_EQ(kExpectedResults.size(), scores.size());

    for (size_t i = 0; i < scores.size(); ++i) {
      EXPECT_EQ(kExpectedResults[i].first, scores[i].origin());
      EXPECT_EQ(kExpectedResults[i].second, scores[i].actual_score());
    }
  }

  // Incognito scores should have the same number of entries but have new
  // values.
  {
    std::vector<std::pair<url::Origin, double>> kExpectedResults = {
        {kOrigin2, 0.05},
        {kOrigin1, 1.0 / 21.0},
    };

    const auto& scores = GetAllStoredScores(incognito_service);
    EXPECT_EQ(kExpectedResults.size(), scores.size());

    for (size_t i = 0; i < scores.size(); ++i) {
      EXPECT_EQ(kExpectedResults[i].first, scores[i].origin());
      EXPECT_EQ(kExpectedResults[i].second, scores[i].actual_score());
    }
  }
}

TEST_P(MediaEngagementServiceTest, CleanupOriginsOnHistoryDeletion) {
  url::Origin origin1 = url::Origin::Create(GURL("https://www.google.com/"));
  url::Origin origin2 = url::Origin::Create(GURL("https://drive.google.com/"));
  url::Origin origin3 = url::Origin::Create(GURL("https://deleted.com/"));
  url::Origin origin4 = url::Origin::Create(GURL("https://notdeleted.com"));

  GURL url1a = GURL("https://www.google.com/search?q=asdf");
  GURL url1b = GURL("https://www.google.com/maps/search?q=asdf");
  GURL url3a = GURL("https://deleted.com/test");

  // origin1 will have a score that is high enough to not return zero
  // and we will ensure it has the same score. origin2 will have a score
  // that is zero and will remain zero. origin3 will have a score
  // and will be cleared. origin4 will have a normal score.
  SetScores(origin1, MediaEngagementScore::GetScoreMinVisits() + 2, 14);
  SetScores(origin2, 2, 1);
  SetScores(origin3, 2, 1);
  SetScores(origin4, MediaEngagementScore::GetScoreMinVisits(), 10);

  base::Time today = GetReferenceTime();
  base::Time yesterday = GetReferenceTime() - base::Days(1);
  base::Time yesterday_afternoon =
      GetReferenceTime() - base::Days(1) + base::Hours(4);
  base::Time yesterday_week = GetReferenceTime() - base::Days(8);
  SetNow(today);

  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile(), ServiceAccessType::IMPLICIT_ACCESS);

  history->AddPage(origin1.GetURL(), yesterday_afternoon,
                   history::SOURCE_BROWSED);
  history->AddPage(url1a, yesterday_afternoon, history::SOURCE_BROWSED);
  history->AddPage(url1b, yesterday_week, history::SOURCE_BROWSED);
  history->AddPage(origin2.GetURL(), yesterday_afternoon,
                   history::SOURCE_BROWSED);
  history->AddPage(origin3.GetURL(), yesterday_week, history::SOURCE_BROWSED);
  history->AddPage(url3a, yesterday_afternoon, history::SOURCE_BROWSED);

  // Check that the scores are valid at the beginning.
  ExpectScores(origin1, 7.0 / 11.0,
               MediaEngagementScore::GetScoreMinVisits() + 2, 14, TimeNotSet());
  EXPECT_EQ(14.0 / 22.0, GetActualScore(origin1));
  ExpectScores(origin2, 0.05, 2, 1, TimeNotSet());
  EXPECT_EQ(1 / 20.0, GetActualScore(origin2));
  ExpectScores(origin3, 0.05, 2, 1, TimeNotSet());
  EXPECT_EQ(1 / 20.0, GetActualScore(origin3));
  ExpectScores(origin4, 0.5, MediaEngagementScore::GetScoreMinVisits(), 10,
               TimeNotSet());
  EXPECT_EQ(0.5, GetActualScore(origin4));

  {
    MediaEngagementChangeWaiter waiter(profile());

    base::CancelableTaskTracker task_tracker;
    // Expire origin1, url1a, origin2, and url3a's most recent visit.
    history->ExpireHistoryBetween(
        std::set<GURL>(), history::kNoAppIdFilter, yesterday, today,
        /*user_initiated*/ true, base::DoNothing(), &task_tracker);
    waiter.Wait();

    // origin1 should have a score that is not zero and is the same as the old
    // score (sometimes it may not match exactly due to rounding). origin2
    // should have a score that is zero but it's visits and playbacks should
    // have decreased. origin3 should have had a decrease in the number of
    // visits. origin4 should have the old score.
    ExpectScores(origin1, 0.6, MediaEngagementScore::GetScoreMinVisits(), 12,
                 TimeNotSet());
    EXPECT_EQ(12.0 / 20.0, GetActualScore(origin1));
    ExpectScores(origin2, 0.0, 1, 0, TimeNotSet());
    EXPECT_EQ(0, GetActualScore(origin2));
    ExpectScores(origin3, 0.0, 1, 0, TimeNotSet());
    ExpectScores(origin4, 0.5, MediaEngagementScore::GetScoreMinVisits(), 10,
                 TimeNotSet());
  }

  {
    MediaEngagementChangeWaiter waiter(profile());

    // Expire url1b.
    std::vector<history::ExpireHistoryArgs> expire_list;
    history::ExpireHistoryArgs args;
    args.urls.insert(url1b);
    args.SetTimeRangeForOneDay(yesterday_week);
    expire_list.push_back(args);

    base::CancelableTaskTracker task_tracker;
    history->ExpireHistory(expire_list, base::DoNothing(), &task_tracker);
    waiter.Wait();

    // origin1's score should have changed but the rest should remain the same.
    ExpectScores(origin1, 0.55, MediaEngagementScore::GetScoreMinVisits() - 1,
                 11, TimeNotSet());
    ExpectScores(origin2, 0.0, 1, 0, TimeNotSet());
    ExpectScores(origin3, 0.0, 1, 0, TimeNotSet());
    ExpectScores(origin4, 0.5, MediaEngagementScore::GetScoreMinVisits(), 10,
                 TimeNotSet());
  }

  {
    MediaEngagementChangeWaiter waiter(profile());

    // Expire origin3.
    std::vector<history::ExpireHistoryArgs> expire_list;
    history::ExpireHistoryArgs args;
    args.urls.insert(origin3.GetURL());
    args.SetTimeRangeForOneDay(yesterday_week);
    expire_list.push_back(args);

    base::CancelableTaskTracker task_tracker;
    history->ExpireHistory(expire_list, base::DoNothing(), &task_tracker);
    waiter.Wait();

    // origin3's score should be removed but the rest should remain the same.
    std::map<url::Origin, double> scores = GetScoreMapForTesting();
    EXPECT_TRUE(scores.find(origin3) == scores.end());
    ExpectScores(origin1, 0.55, MediaEngagementScore::GetScoreMinVisits() - 1,
                 11, TimeNotSet());
    ExpectScores(origin2, 0.0, 1, 0, TimeNotSet());
    ExpectScores(origin3, 0.0, 0, 0, TimeNotSet());
    ExpectScores(origin4, 0.5, MediaEngagementScore::GetScoreMinVisits(), 10,
                 TimeNotSet());
  }
}

// The test is flaky: crbug.com/1042417.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#define MAYBE_CleanUpDatabaseWhenHistoryIsExpired \
  DISABLED_CleanUpDatabaseWhenHistoryIsExpired
#else
#define MAYBE_CleanUpDatabaseWhenHistoryIsExpired \
  CleanUpDatabaseWhenHistoryIsExpired
#endif
TEST_P(MediaEngagementServiceTest, MAYBE_CleanUpDatabaseWhenHistoryIsExpired) {
  // |origin1| will have history that is before the expiry threshold and should
  // not be deleted. |origin2| will have history either side of the threshold
  // and should also not be deleted. |origin3| will have history before the
  // threshold and should be deleted.
  url::Origin origin1 = url::Origin::Create(GURL("https://www.google.com"));
  url::Origin origin2 = url::Origin::Create(GURL("https://drive.google.com"));
  url::Origin origin3 = url::Origin::Create(GURL("https://deleted.com"));

  // Populate test MEI data.
  SetScores(origin1, 20, 20);
  SetScores(origin2, 30, 30);
  SetScores(origin3, 40, 40);

  base::Time today = base::Time::Now();
  base::Time before_threshold = today - kHistoryExpirationThreshold;
  SetNow(today);

  // Populate test history records.
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile(), ServiceAccessType::IMPLICIT_ACCESS);

  history->AddPage(origin1.GetURL(), today, history::SOURCE_BROWSED);
  history->AddPage(origin2.GetURL(), today, history::SOURCE_BROWSED);
  history->AddPage(origin2.GetURL(), before_threshold, history::SOURCE_BROWSED);
  history->AddPage(origin3.GetURL(), before_threshold, history::SOURCE_BROWSED);

  // Expire history older than |threshold|.
  MediaEngagementChangeWaiter waiter(profile());
  RestartHistoryService(mock_time_task_runner_);

  // From this point profile() is using a new HistoryService that runs on
  // mock time.

  // Now, fast forward time to ensure that the expiration job is completed. This
  // will start by triggering the backend initialization. 30 seconds is the
  // value of kExpirationDelaySec.
  mock_time_task_runner_->FastForwardBy(base::Seconds(30));
  waiter.Wait();

  // Check the scores for the test origins.
  ExpectScores(origin1, 1.0, 20, 20, TimeNotSet());
  ExpectScores(origin2, 1.0, 30, 30, TimeNotSet());
  ExpectScores(origin3, 0, 0, 0, TimeNotSet());
}

TEST_P(MediaEngagementServiceTest, CleanUpDatabaseWhenHistoryIsDeleted) {
  url::Origin origin1 = url::Origin::Create(GURL("https://www.google.com/"));
  url::Origin origin2 = url::Origin::Create(GURL("https://drive.google.com/"));
  url::Origin origin3 = url::Origin::Create(GURL("https://deleted.com/"));
  url::Origin origin4 = url::Origin::Create(GURL("https://notdeleted.com"));

  GURL url1a = GURL("https://www.google.com/search?q=asdf");
  GURL url1b = GURL("https://www.google.com/maps/search?q=asdf");
  GURL url3a = GURL("https://deleted.com/test");

  // origin1 will have a score that is high enough to not return zero
  // and we will ensure it has the same score. origin2 will have a score
  // that is zero and will remain zero. origin3 will have a score
  // and will be cleared. origin4 will have a normal score.
  SetScores(origin1, MediaEngagementScore::GetScoreMinVisits() + 2, 14);
  SetScores(origin2, 2, 1);
  SetScores(origin3, 2, 1);
  SetScores(origin4, MediaEngagementScore::GetScoreMinVisits(), 10);

  base::Time today = GetReferenceTime();
  base::Time yesterday_afternoon =
      GetReferenceTime() - base::Days(1) + base::Hours(4);
  base::Time yesterday_week = GetReferenceTime() - base::Days(8);
  SetNow(today);

  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile(), ServiceAccessType::IMPLICIT_ACCESS);

  history->AddPage(origin1.GetURL(), yesterday_afternoon,
                   history::SOURCE_BROWSED);
  history->AddPage(url1a, yesterday_afternoon, history::SOURCE_BROWSED);
  history->AddPage(url1b, yesterday_week, history::SOURCE_BROWSED);
  history->AddPage(origin2.GetURL(), yesterday_afternoon,
                   history::SOURCE_BROWSED);
  history->AddPage(origin3.GetURL(), yesterday_week, history::SOURCE_BROWSED);
  history->AddPage(url3a, yesterday_afternoon, history::SOURCE_BROWSED);

  // Check that the scores are valid at the beginning.
  ExpectScores(origin1, 7.0 / 11.0,
               MediaEngagementScore::GetScoreMinVisits() + 2, 14, TimeNotSet());
  EXPECT_EQ(14.0 / 22.0, GetActualScore(origin1));
  ExpectScores(origin2, 0.05, 2, 1, TimeNotSet());
  EXPECT_EQ(1 / 20.0, GetActualScore(origin2));
  ExpectScores(origin3, 0.05, 2, 1, TimeNotSet());
  EXPECT_EQ(1 / 20.0, GetActualScore(origin3));
  ExpectScores(origin4, 0.5, MediaEngagementScore::GetScoreMinVisits(), 10,
               TimeNotSet());
  EXPECT_EQ(0.5, GetActualScore(origin4));

  {
    base::RunLoop run_loop;
    base::CancelableTaskTracker task_tracker;
    // Clear all history.
    history->ExpireHistoryBetween(
        std::set<GURL>(), history::kNoAppIdFilter, base::Time(), base::Time(),
        /*user_initiated*/ true, run_loop.QuitClosure(), &task_tracker);
    run_loop.Run();

    // origin1 should have a score that is not zero and is the same as the old
    // score (sometimes it may not match exactly due to rounding). origin2
    // should have a score that is zero but it's visits and playbacks should
    // have decreased. origin3 should have had a decrease in the number of
    // visits. origin4 should have the old score.
    ExpectScores(origin1, 0.0, 0, 0, TimeNotSet());
    EXPECT_EQ(0, GetActualScore(origin1));
    ExpectScores(origin2, 0.0, 0, 0, TimeNotSet());
    EXPECT_EQ(0, GetActualScore(origin2));
    ExpectScores(origin3, 0.0, 0, 0, TimeNotSet());
    ExpectScores(origin4, 0.0, 0, 0, TimeNotSet());
  }
}

TEST_P(MediaEngagementServiceTest, HistoryExpirationIsNoOp) {
  url::Origin origin1 = url::Origin::Create(GURL("https://www.google.com/"));
  url::Origin origin2 = url::Origin::Create(GURL("https://drive.google.com/"));
  url::Origin origin3 = url::Origin::Create(GURL("https://deleted.com/"));
  url::Origin origin4 = url::Origin::Create(GURL("https://notdeleted.com"));

  GURL url1a = GURL("https://www.google.com/search?q=asdf");
  GURL url1b = GURL("https://www.google.com/maps/search?q=asdf");
  GURL url3a = GURL("https://deleted.com/test");

  SetScores(origin1, MediaEngagementScore::GetScoreMinVisits() + 2, 14);
  SetScores(origin2, 2, 1);
  SetScores(origin3, 2, 1);
  SetScores(origin4, MediaEngagementScore::GetScoreMinVisits(), 10);

  ExpectScores(origin1, 7.0 / 11.0,
               MediaEngagementScore::GetScoreMinVisits() + 2, 14, TimeNotSet());
  EXPECT_EQ(14.0 / 22.0, GetActualScore(origin1));
  ExpectScores(origin2, 0.05, 2, 1, TimeNotSet());
  EXPECT_EQ(1 / 20.0, GetActualScore(origin2));
  ExpectScores(origin3, 0.05, 2, 1, TimeNotSet());
  EXPECT_EQ(1 / 20.0, GetActualScore(origin3));
  ExpectScores(origin4, 0.5, MediaEngagementScore::GetScoreMinVisits(), 10,
               TimeNotSet());
  EXPECT_EQ(0.5, GetActualScore(origin4));

  {
    history::HistoryService* history = HistoryServiceFactory::GetForProfile(
        profile(), ServiceAccessType::IMPLICIT_ACCESS);

    service()->OnHistoryDeletions(
        history, history::DeletionInfo(history::DeletionTimeRange::Invalid(),
                                       true, history::URLRows(),
                                       std::set<GURL>(), std::nullopt));

    // Same as above, nothing should have changed.
    ExpectScores(origin1, 7.0 / 11.0,
                 MediaEngagementScore::GetScoreMinVisits() + 2, 14,
                 TimeNotSet());
    EXPECT_EQ(14.0 / 22.0, GetActualScore(origin1));
    ExpectScores(origin2, 0.05, 2, 1, TimeNotSet());
    EXPECT_EQ(1 / 20.0, GetActualScore(origin2));
    ExpectScores(origin3, 0.05, 2, 1, TimeNotSet());
    EXPECT_EQ(1 / 20.0, GetActualScore(origin3));
    ExpectScores(origin4, 0.5, MediaEngagementScore::GetScoreMinVisits(), 10,
                 TimeNotSet());
    EXPECT_EQ(0.5, GetActualScore(origin4));
  }
}

TEST_P(MediaEngagementServiceTest,
       CleanupDataOnSiteDataCleanup_OutsideBoundary) {
  url::Origin origin = url::Origin::Create(GURL("https://www.google.com"));

  base::Time today = GetReferenceTime();
  SetNow(today);

  SetScores(origin, 1, 1);
  SetLastMediaPlaybackTime(origin, today);

  ClearDataBetweenTime(today - base::Days(2), today - base::Days(1));
  ExpectScores(origin, 0.05, 1, 1, today);
}

TEST_P(MediaEngagementServiceTest,
       CleanupDataOnSiteDataCleanup_WithinBoundary) {
  url::Origin origin1 = url::Origin::Create(GURL("https://www.google.com"));
  url::Origin origin2 = url::Origin::Create(GURL("https://www.google.co.uk"));

  base::Time today = GetReferenceTime();
  base::Time yesterday = today - base::Days(1);
  base::Time two_days_ago = today - base::Days(2);
  SetNow(today);

  SetScores(origin1, 1, 1);
  SetScores(origin2, 1, 1);
  SetLastMediaPlaybackTime(origin1, yesterday);
  SetLastMediaPlaybackTime(origin2, two_days_ago);

  ClearDataBetweenTime(two_days_ago, yesterday);
  ExpectScores(origin1, 0, 0, 0, TimeNotSet());
  ExpectScores(origin2, 0, 0, 0, TimeNotSet());
}

TEST_P(MediaEngagementServiceTest, CleanupDataOnSiteDataCleanup_NoTimeSet) {
  url::Origin origin = url::Origin::Create(GURL("https://www.google.com"));

  base::Time today = GetReferenceTime();

  SetNow(GetReferenceTime());
  SetScores(origin, 1, 0);

  ClearDataBetweenTime(today - base::Days(2), today - base::Days(1));
  ExpectScores(origin, 0.0, 1, 0, TimeNotSet());
}

TEST_P(MediaEngagementServiceTest, CleanupDataOnSiteDataCleanup_All) {
  url::Origin origin1 = url::Origin::Create(GURL("https://www.google.com"));
  url::Origin origin2 = url::Origin::Create(GURL("https://www.google.co.uk"));

  base::Time today = GetReferenceTime();
  base::Time yesterday = today - base::Days(1);
  base::Time two_days_ago = today - base::Days(2);
  SetNow(today);

  SetScores(origin1, 1, 1);
  SetScores(origin2, 1, 1);
  SetLastMediaPlaybackTime(origin1, yesterday);
  SetLastMediaPlaybackTime(origin2, two_days_ago);

  ClearDataBetweenTime(base::Time(), base::Time::Max());
  ExpectScores(origin1, 0, 0, 0, TimeNotSet());
  ExpectScores(origin2, 0, 0, 0, TimeNotSet());
}

TEST_P(MediaEngagementServiceTest, HasHighEngagement) {
  url::Origin origin1 = url::Origin::Create(GURL("https://www.google.com"));
  url::Origin origin2 = url::Origin::Create(GURL("https://www.google.co.uk"));
  url::Origin origin3 = url::Origin::Create(GURL("https://www.example.com"));

  SetScores(origin1, 20, 15);
  SetScores(origin2, 20, 4);

  EXPECT_TRUE(HasHighEngagement(origin1));
  EXPECT_FALSE(HasHighEngagement(origin2));
  EXPECT_FALSE(HasHighEngagement(origin3));
}

TEST_P(MediaEngagementServiceTest, SchemaVersion_Changed) {
  url::Origin origin = url::Origin::Create(GURL("https://www.google.com"));
  SetScores(origin, 1, 2);

  SetSchemaVersion(0);
  std::unique_ptr<MediaEngagementService> new_service =
      base::WrapUnique<MediaEngagementService>(
          StartNewMediaEngagementService());

  ExpectScores(new_service.get(), origin, 0.0, 0, 0, TimeNotSet());
  new_service->Shutdown();
}

TEST_P(MediaEngagementServiceTest, SchemaVersion_Same) {
  url::Origin origin = url::Origin::Create(GURL("https://www.google.com"));
  SetScores(origin, 1, 2);

  std::unique_ptr<MediaEngagementService> new_service =
      base::WrapUnique<MediaEngagementService>(
          StartNewMediaEngagementService());

  ExpectScores(new_service.get(), origin, 0.1, 1, 2, TimeNotSet());
  new_service->Shutdown();
}

INSTANTIATE_TEST_SUITE_P(All, MediaEngagementServiceTest, ::testing::Bool());

class MediaEngagementServiceEnabledTest
    : public ChromeRenderViewHostTestHarness {};

TEST_F(MediaEngagementServiceEnabledTest, IsEnabled) {
#if BUILDFLAG(IS_ANDROID)
  // Make sure these flags are disabled on Android
  EXPECT_FALSE(base::FeatureList::IsEnabled(
      media::kMediaEngagementBypassAutoplayPolicies));
  EXPECT_FALSE(
      base::FeatureList::IsEnabled(media::kPreloadMediaEngagementData));
#else
  EXPECT_TRUE(base::FeatureList::IsEnabled(
      media::kMediaEngagementBypassAutoplayPolicies));
  EXPECT_TRUE(base::FeatureList::IsEnabled(media::kPreloadMediaEngagementData));
#endif
}
