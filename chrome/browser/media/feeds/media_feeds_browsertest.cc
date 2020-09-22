// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/media/feeds/media_feeds_contents_observer.h"
#include "chrome/browser/media/feeds/media_feeds_service.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom-forward.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom-shared.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom.h"
#include "chrome/browser/media/history/media_history_feeds_table.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "chrome/browser/media/history/media_history_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/schema_org/schema_org_entity_names.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/frame_load_waiter.h"
#include "content/public/test/test_utils.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/origin.h"

namespace media_feeds {

namespace {

const char kMediaFeedsTestURL[] = "/test";

const char kMediaFeedsAltTestURL[] = "/alt";

const char kMediaFeedsMinTestURL[] = "/min";

constexpr base::FilePath::CharType kMediaFeedsTestDir[] =
    FILE_PATH_LITERAL("chrome/test/data/media/feeds");

const char kMediaFeedsTestHTML[] =
    "  <!DOCTYPE html>"
    "  <head>%s</head>";

const char kMediaFeedsTestHeadHTML[] =
    "<link rel=media-feed type=\"application/ld+json\" "
    "href=\"/media-feed.json\"/>";

const char kMediaFeedsMinTestHeadHTML[] =
    "<link rel=media-feed href=\"/media-feed-min.json\"/>";

struct TestData {
  std::string head_html;
  bool discovered;
  bool https = true;
};

}  // namespace

class MediaFeedsBrowserTest : public InProcessBrowserTest {
 public:
  MediaFeedsBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~MediaFeedsBrowserTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(media::kMediaFeeds);

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    // The HTTPS server serves the test page using HTTPS.
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &MediaFeedsBrowserTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(https_server_.Start());

    // The embedded test server will serve the test page using HTTP.
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &MediaFeedsBrowserTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());

    InProcessBrowserTest::SetUpOnMainThread();
  }

  std::vector<media_feeds::mojom::MediaFeedPtr> GetDiscoveredFeeds() {
    base::RunLoop run_loop;
    std::vector<media_feeds::mojom::MediaFeedPtr> out;

    GetMediaHistoryService()->GetMediaFeeds(
        media_history::MediaHistoryKeyedService::GetMediaFeedsRequest(),
        base::BindLambdaForTesting(
            [&](std::vector<media_feeds::mojom::MediaFeedPtr> feeds) {
              out = std::move(feeds);
              run_loop.Quit();
            }));

    run_loop.Run();
    return out;
  }

  void WaitForDB() {
    base::RunLoop run_loop;
    GetMediaHistoryService()->PostTaskToDBForTest(run_loop.QuitClosure());
    run_loop.Run();
  }

  std::set<GURL> GetDiscoveredFeedURLs() {
    base::RunLoop run_loop;
    std::set<GURL> out;

    GetMediaHistoryService()->GetURLsInTableForTest(
        media_history::MediaHistoryFeedsTable::kTableName,
        base::BindLambdaForTesting([&](std::set<GURL> urls) {
          out = urls;
          run_loop.Quit();
        }));

    run_loop.Run();
    return out;
  }

  void DiscoverFeed(const std::string& url) {
    EXPECT_TRUE(GetDiscoveredFeedURLs().empty());

    MediaFeedsContentsObserver* contents_observer =
        static_cast<MediaFeedsContentsObserver*>(
            MediaFeedsContentsObserver::FromWebContents(GetWebContents()));

    GURL test_url(GetServer()->GetURL(url));

    // The contents observer will call this closure when it has checked for a
    // media feed.
    base::RunLoop run_loop;

    contents_observer->SetClosureForTest(
        base::BindLambdaForTesting([&]() { run_loop.Quit(); }));

    ui_test_utils::NavigateToURL(browser(), test_url);

    run_loop.Run();

    // Wait until the session has finished saving.
    WaitForDB();
  }

  std::vector<media_feeds::mojom::MediaFeedItemPtr> GetItemsForMediaFeedSync(
      int64_t feed_id) {
    base::RunLoop run_loop;
    std::vector<media_feeds::mojom::MediaFeedItemPtr> out;

    GetMediaHistoryService()->GetMediaFeedItems(
        media_history::MediaHistoryKeyedService::GetMediaFeedItemsRequest::
            CreateItemsForDebug(feed_id),
        base::BindLambdaForTesting(
            [&](std::vector<media_feeds::mojom::MediaFeedItemPtr> items) {
              out = std::move(items);
              run_loop.Quit();
            }));

    run_loop.Run();
    return out;
  }

  std::vector<media_feeds::mojom::MediaFeedPtr> GetMediaFeedsSync(
      const media_history::MediaHistoryKeyedService::GetMediaFeedsRequest&
          request =
              media_history::MediaHistoryKeyedService::GetMediaFeedsRequest()) {
    base::RunLoop run_loop;
    std::vector<media_feeds::mojom::MediaFeedPtr> out;

    GetMediaHistoryService()->GetMediaFeeds(
        request, base::BindLambdaForTesting(
                     [&](std::vector<media_feeds::mojom::MediaFeedPtr> rows) {
                       out = std::move(rows);
                       run_loop.Quit();
                     }));

    run_loop.Run();
    return out;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  media_history::MediaHistoryKeyedService* GetMediaHistoryService() {
    return media_history::MediaHistoryKeyedServiceFactory::GetForProfile(
        browser()->profile());
  }

  media_feeds::MediaFeedsService* GetMediaFeedsService() {
    return media_feeds::MediaFeedsService::Get(browser()->profile());
  }

  virtual net::EmbeddedTestServer* GetServer() { return &https_server_; }

 private:
  virtual std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url == kMediaFeedsTestURL) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_content(
          base::StringPrintf(kMediaFeedsTestHTML, kMediaFeedsTestHeadHTML));
      return response;
    } else if (request.relative_url == kMediaFeedsMinTestURL) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_content(
          base::StringPrintf(kMediaFeedsTestHTML, kMediaFeedsMinTestHeadHTML));
      return response;
    } else if (request.relative_url == kMediaFeedsAltTestURL) {
      return std::make_unique<net::test_server::BasicHttpResponse>();
    } else if (base::EndsWith(request.relative_url, "json",
                              base::CompareCase::SENSITIVE)) {
      if (full_test_data_.empty())
        LoadFullTestData(request.relative_url.substr(1));
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_content(full_test_data_);
      return response;
    }
    return nullptr;
  }

  void LoadFullTestData(const std::string& file_name) {
    base::FilePath file;
    ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &file));
    file = file.Append(kMediaFeedsTestDir).AppendASCII(file_name.c_str());

    std::string test_data;
    base::ReadFileToString(file, &test_data);
    ASSERT_TRUE(test_data.size());

    full_test_data_ = test_data.c_str();
  }

  net::EmbeddedTestServer https_server_;

  base::test::ScopedFeatureList scoped_feature_list_;

  std::string full_test_data_;
};

IN_PROC_BROWSER_TEST_F(MediaFeedsBrowserTest, DiscoverAndFetch) {
  DiscoverFeed(kMediaFeedsTestURL);

  // Check we discovered the feed.
  std::set<GURL> expected_urls = {GetServer()->GetURL("/media-feed.json")};
  EXPECT_EQ(expected_urls, GetDiscoveredFeedURLs());

  std::vector<media_feeds::mojom::MediaFeedPtr> discovered_feeds =
      GetDiscoveredFeeds();
  EXPECT_EQ(1u, discovered_feeds.size());

  base::RunLoop run_loop;
  GetMediaFeedsService()->FetchMediaFeed(
      discovered_feeds[0]->id,
      base::BindLambdaForTesting(
          [&](const std::string& ignored) { run_loop.Quit(); }));
  run_loop.Run();
  WaitForDB();

  auto feeds = GetMediaFeedsSync();

  auto logo1 = mojom::MediaImage::New();
  logo1->size = ::gfx::Size(1113, 245);
  logo1->src = GURL(
      "https://beccahughes.github.io/media/media-feeds/"
      "chromium_logo_white.png");
  logo1->content_attributes = {
      mojom::ContentAttribute::kForDarkBackground,
      mojom::ContentAttribute::kHasTitle,
      mojom::ContentAttribute::kHasTransparentBackground};
  auto logo2 = mojom::MediaImage::New();
  logo2->size = ::gfx::Size(600, 315);
  logo2->src =
      GURL("https://beccahughes.github.io/media/media-feeds/chromium_card.png");
  logo2->content_attributes = {mojom::ContentAttribute::kForLightBackground,
                               mojom::ContentAttribute::kHasTitle,
                               mojom::ContentAttribute::kCentered};
  std::set<url::Origin> origins = {
      url::Origin::Create(GURL("https://www.github.com"))};
  auto user_id = mojom::UserIdentifier::New();
  user_id->name = "Becca Hughes";
  user_id->email = "beccahughes@chromium.org";
  user_id->image = mojom::MediaImage::New();
  user_id->image->size = ::gfx::Size(32, 32);
  user_id->image->src = GURL(
      "https://www.chromium.org/_/rsrc/1438811752264/chromium-projects/"
      "logo_chrome_color_1x_web_32dp.png");

  // First, check the feed metadata.
  ASSERT_EQ(1u, feeds.size());
  EXPECT_EQ("Chromium Developers", feeds[0]->display_name);
  ASSERT_EQ(2u, feeds[0]->logos.size());
  EXPECT_EQ(logo1, feeds[0]->logos[0]);
  EXPECT_EQ(logo2, feeds[0]->logos[1]);
  EXPECT_EQ(user_id, feeds[0]->user_identifier);
  EXPECT_EQ("TEST", feeds[0]->cookie_name_filter);

  auto items = GetItemsForMediaFeedSync(discovered_feeds[0]->id);

  EXPECT_EQ(7u, items.size());

  // Check each feed item and all fields one-by-one.
  {
    mojom::MediaFeedItemPtr expected_item = mojom::MediaFeedItem::New();
    expected_item->id = 1;

    const std::string name = "Anatomy of a Web Media Experience 😊";
    ASSERT_TRUE(
        base::UTF8ToUTF16(name.c_str(), name.size(), &expected_item->name));

    expected_item->type = mojom::MediaFeedItemType::kVideo;
    expected_item->author = mojom::Author::New();
    expected_item->author->name = "Google Chrome Developers";
    expected_item->author->url =
        GURL("https://www.youtube.com/user/ChromeDevelopers");
    ASSERT_TRUE(
        base::Time::FromString("2019-05-09", &expected_item->date_published));
    expected_item->duration =
        base::TimeDelta::FromMinutes(34) + base::TimeDelta::FromSeconds(41);
    expected_item->genre.push_back("Factual");
    expected_item->interaction_counters = {
        {mojom::InteractionCounterType::kWatch, 7252},
        {mojom::InteractionCounterType::kLike, 94},
        {mojom::InteractionCounterType::kDislike, 4}};
    expected_item->is_family_friendly =
        media_feeds::mojom::IsFamilyFriendly::kUnknown;
    expected_item->action = mojom::Action::New();
    expected_item->action->url =
        GURL("https://www.youtube.com/watch?v=lXm6jOQLe1Y");
    auto image = mojom::MediaImage::New();
    image->size = ::gfx::Size(336, 188);
    image->src =
        GURL("https://beccahughes.github.io/media/media-feeds/video1.webp");
    image->content_attributes = {mojom::ContentAttribute::kHasTitle,
                                 mojom::ContentAttribute::kIconic,
                                 mojom::ContentAttribute::kPoster};
    expected_item->images.push_back(std::move(image));

    auto actual = std::find_if(items.begin(), items.end(), [&](auto& item) {
      return item->name == expected_item->name;
    });

    ASSERT_TRUE(actual != items.end());
    EXPECT_EQ(expected_item, *actual);
  }

  {
    mojom::MediaFeedItemPtr expected_item = mojom::MediaFeedItem::New();
    expected_item->id = 2;
    expected_item->name = base::ASCIIToUTF16(
        "Building Modern Web Media Experiences: Picture-in-Picture and AV1");
    expected_item->type = mojom::MediaFeedItemType::kVideo;
    expected_item->author = mojom::Author::New();
    expected_item->author->name = "Google Chrome Developers";
    expected_item->author->url =
        GURL("https://www.youtube.com/user/ChromeDevelopers");
    ASSERT_TRUE(
        base::Time::FromString("2018-11-12", &expected_item->date_published));
    expected_item->duration =
        base::TimeDelta::FromMinutes(21) + base::TimeDelta::FromSeconds(24);
    expected_item->genre.push_back("Factual");
    auto identifier = mojom::Identifier::New();
    identifier->type = mojom::Identifier::Type::kPartnerId;
    identifier->value = "456789876";
    expected_item->identifiers.push_back(std::move(identifier));
    expected_item->interaction_counters = {
        {mojom::InteractionCounterType::kWatch, 7252},
        {mojom::InteractionCounterType::kLike, 94},
        {mojom::InteractionCounterType::kDislike, 4}};
    expected_item->is_family_friendly =
        media_feeds::mojom::IsFamilyFriendly::kYes;
    expected_item->action_status = mojom::MediaFeedItemActionStatus::kActive;
    expected_item->action = mojom::Action::New();
    expected_item->action->url = GURL("https://youtu.be/iTC3mfe0DwE?t=10");
    expected_item->action->start_time = base::TimeDelta::FromSeconds(10);
    auto image = mojom::MediaImage::New();
    image->size = ::gfx::Size(336, 188);
    image->src =
        GURL("https://beccahughes.github.io/media/media-feeds/video2.webp");
    image->content_attributes = {mojom::ContentAttribute::kHasTitle,
                                 mojom::ContentAttribute::kIconic,
                                 mojom::ContentAttribute::kPoster};
    expected_item->images.push_back(std::move(image));
    auto image2 = mojom::MediaImage::New();
    image2->size = ::gfx::Size(1884, 982);
    image2->src = GURL(
        "https://beccahughes.github.io/media/media-feeds/video2_current.png");
    image2->content_attributes = {mojom::ContentAttribute::kSceneStill};
    expected_item->images.push_back(std::move(image2));

    auto actual = std::find_if(items.begin(), items.end(), [&](auto& item) {
      return item->name == expected_item->name;
    });

    ASSERT_TRUE(actual != items.end());
    EXPECT_EQ(expected_item, *actual);
  }

  {
    mojom::MediaFeedItemPtr expected_item = mojom::MediaFeedItem::New();
    expected_item->id = 3;
    expected_item->name = base::ASCIIToUTF16("Chrome Releases");
    expected_item->type = mojom::MediaFeedItemType::kTVSeries;
    ASSERT_TRUE(
        base::Time::FromString("2019-11-10", &expected_item->date_published));
    expected_item->genre.push_back("Factual");
    expected_item->is_family_friendly =
        media_feeds::mojom::IsFamilyFriendly::kYes;
    expected_item->action_status = mojom::MediaFeedItemActionStatus::kActive;
    expected_item->action = mojom::Action::New();
    expected_item->action->url =
        GURL("https://www.youtube.com/watch?v=L0OB0_bO5I0?t=254");
    expected_item->action->start_time =
        base::TimeDelta::FromMinutes(4) + base::TimeDelta::FromSeconds(14);

    expected_item->tv_episode = mojom::TVEpisode::New();
    expected_item->tv_episode->name = "New in Chrome 79";
    expected_item->tv_episode->episode_number = 79;
    expected_item->tv_episode->season_number = 1;
    expected_item->tv_episode->duration =
        base::TimeDelta::FromMinutes(4) + base::TimeDelta::FromSeconds(16);
    auto episode_image = mojom::MediaImage::New();
    episode_image->size = ::gfx::Size(336, 188);
    episode_image->src =
        GURL("https://beccahughes.github.io/media/media-feeds/chrome79.webp");
    episode_image->content_attributes = {mojom::ContentAttribute::kHasTitle,
                                         mojom::ContentAttribute::kIconic,
                                         mojom::ContentAttribute::kPoster};
    expected_item->tv_episode->images.push_back(std::move(episode_image));
    auto episode_image2 = mojom::MediaImage::New();
    episode_image2->size = ::gfx::Size(1874, 970);
    episode_image2->src = GURL(
        "https://beccahughes.github.io/media/media-feeds/chrome79_current.png");
    episode_image2->content_attributes = {mojom::ContentAttribute::kSceneStill};
    expected_item->tv_episode->images.push_back(std::move(episode_image2));

    expected_item->play_next_candidate = mojom::PlayNextCandidate::New();
    expected_item->play_next_candidate->name = "New in Chrome 80";
    expected_item->play_next_candidate->episode_number = 80;
    expected_item->play_next_candidate->season_number = 1;
    expected_item->play_next_candidate->duration =
        base::TimeDelta::FromMinutes(6) + base::TimeDelta::FromSeconds(1);
    expected_item->play_next_candidate->action = mojom::Action::New();
    expected_item->play_next_candidate->action->url =
        GURL("https://www.youtube.com/watch?v=lM0qZpxu0Fg");
    auto play_next_image = mojom::MediaImage::New();
    play_next_image->size = ::gfx::Size(336, 188);
    play_next_image->src =
        GURL("https://beccahughes.github.io/media/media-feeds/chrome80.webp");
    play_next_image->content_attributes = {mojom::ContentAttribute::kHasTitle,
                                           mojom::ContentAttribute::kIconic,
                                           mojom::ContentAttribute::kPoster};
    expected_item->play_next_candidate->images.push_back(
        std::move(play_next_image));

    auto image = mojom::MediaImage::New();
    image->size = ::gfx::Size(336, 188);
    image->src =
        GURL("https://beccahughes.github.io/media/media-feeds/chromerel.webp");
    expected_item->images.push_back(std::move(image));

    auto actual = std::find_if(items.begin(), items.end(), [&](auto& item) {
      return item->name == expected_item->name;
    });

    ASSERT_TRUE(actual != items.end());
    EXPECT_EQ(expected_item, *actual);
  }

  {
    mojom::MediaFeedItemPtr expected_item = mojom::MediaFeedItem::New();
    expected_item->id = 4;
    expected_item->name = base::ASCIIToUTF16("Chrome University");
    expected_item->type = mojom::MediaFeedItemType::kTVSeries;
    ASSERT_TRUE(
        base::Time::FromString("2020-01-01", &expected_item->date_published));
    expected_item->genre.push_back("Factual");
    expected_item->is_family_friendly =
        media_feeds::mojom::IsFamilyFriendly::kYes;
    expected_item->action_status = mojom::MediaFeedItemActionStatus::kCompleted;
    expected_item->action = mojom::Action::New();
    expected_item->action->url =
        GURL("https://www.youtube.com/watch?v=kNzoswFIU9M");

    expected_item->tv_episode = mojom::TVEpisode::New();
    expected_item->tv_episode->name = "Anatomy of the Browser";
    expected_item->tv_episode->episode_number = 10;
    expected_item->tv_episode->season_number = 1;
    expected_item->tv_episode->duration =
        base::TimeDelta::FromMinutes(15) + base::TimeDelta::FromSeconds(33);
    auto episode_image = mojom::MediaImage::New();
    episode_image->size = ::gfx::Size(336, 188);
    episode_image->src =
        GURL("https://beccahughes.github.io/media/media-feeds/chromeu1.webp");
    episode_image->content_attributes = {mojom::ContentAttribute::kHasTitle,
                                         mojom::ContentAttribute::kIconic,
                                         mojom::ContentAttribute::kPoster};
    expected_item->tv_episode->images.push_back(std::move(episode_image));

    expected_item->play_next_candidate = mojom::PlayNextCandidate::New();
    expected_item->play_next_candidate->name = "History of the Web";
    expected_item->play_next_candidate->episode_number = 1;
    expected_item->play_next_candidate->season_number = 2;
    expected_item->play_next_candidate->duration =
        base::TimeDelta::FromMinutes(10) + base::TimeDelta::FromSeconds(15);
    expected_item->play_next_candidate->action = mojom::Action::New();
    expected_item->play_next_candidate->action->url =
        GURL("https://www.youtube.com/watch?v=PzzNuCk-e0Y");
    auto play_next_image = mojom::MediaImage::New();
    play_next_image->size = ::gfx::Size(336, 188);
    play_next_image->src =
        GURL("https://beccahughes.github.io/media/media-feeds/chromeu2.webp");
    play_next_image->content_attributes = {mojom::ContentAttribute::kHasTitle,
                                           mojom::ContentAttribute::kIconic,
                                           mojom::ContentAttribute::kPoster};
    expected_item->play_next_candidate->images.push_back(
        std::move(play_next_image));

    auto image = mojom::MediaImage::New();
    image->size = ::gfx::Size(336, 188);
    image->src =
        GURL("https://beccahughes.github.io/media/media-feeds/chromeu.webp");
    image->content_attributes = {mojom::ContentAttribute::kHasTitle,
                                 mojom::ContentAttribute::kIconic,
                                 mojom::ContentAttribute::kPoster};
    expected_item->images.push_back(std::move(image));

    auto actual = std::find_if(items.begin(), items.end(), [&](auto& item) {
      return item->name == expected_item->name;
    });

    ASSERT_TRUE(actual != items.end());
    EXPECT_EQ(expected_item, *actual);
  }

  {
    mojom::MediaFeedItemPtr expected_item = mojom::MediaFeedItem::New();
    expected_item->id = 5;
    expected_item->name = base::ASCIIToUTF16("JAM stack");
    expected_item->type = mojom::MediaFeedItemType::kTVSeries;
    ASSERT_TRUE(
        base::Time::FromString("2020-01-22", &expected_item->date_published));
    expected_item->genre.push_back("Factual");
    expected_item->is_family_friendly =
        media_feeds::mojom::IsFamilyFriendly::kYes;
    expected_item->duration =
        base::TimeDelta::FromMinutes(9) + base::TimeDelta::FromSeconds(55);
    expected_item->action = mojom::Action::New();
    expected_item->action->url =
        GURL("https://www.youtube.com/watch?v=QXsWaA3HTHA");

    auto image = mojom::MediaImage::New();
    image->size = ::gfx::Size(360, 480);
    image->src =
        GURL("https://beccahughes.github.io/media/media-feeds/jam.webp");
    image->content_attributes = {mojom::ContentAttribute::kHasTitle,
                                 mojom::ContentAttribute::kIconic,
                                 mojom::ContentAttribute::kPoster};
    expected_item->images.push_back(std::move(image));

    auto actual = std::find_if(items.begin(), items.end(), [&](auto& item) {
      return item->name == expected_item->name;
    });

    ASSERT_TRUE(actual != items.end());
    EXPECT_EQ(expected_item, *actual);
  }

  {
    mojom::MediaFeedItemPtr expected_item = mojom::MediaFeedItem::New();
    expected_item->id = 6;
    expected_item->name = base::ASCIIToUTF16("Ask Chrome");
    expected_item->type = mojom::MediaFeedItemType::kVideo;
    expected_item->author = mojom::Author::New();
    expected_item->author->name = "Google Chrome Developers";
    expected_item->author->url =
        GURL("https://www.youtube.com/user/ChromeDevelopers");
    ASSERT_TRUE(
        base::Time::FromString("2020-01-27", &expected_item->date_published));
    expected_item->duration = base::TimeDelta::FromMinutes(1);
    expected_item->genre.push_back("Factual");
    expected_item->is_family_friendly =
        media_feeds::mojom::IsFamilyFriendly::kYes;
    expected_item->action = mojom::Action::New();
    expected_item->action->url =
        GURL("https://www.youtube.com/watch?v=zJQNQmE6_UI");
    auto image = mojom::MediaImage::New();
    image->size = ::gfx::Size(336, 188);
    image->src =
        GURL("https://beccahughes.github.io/media/media-feeds/askchrome.webp");
    image->content_attributes = {mojom::ContentAttribute::kHasTitle,
                                 mojom::ContentAttribute::kIconic,
                                 mojom::ContentAttribute::kPoster};
    expected_item->images.push_back(std::move(image));
    expected_item->live = mojom::LiveDetails::New();
    ASSERT_TRUE(base::Time::FromString("2020-03-20T00:00:00+0000",
                                       &expected_item->live->start_time));
    base::Time end_time;
    ASSERT_TRUE(base::Time::FromString("2020-12-31T23:59:00+0000", &end_time));
    expected_item->live->end_time = end_time;

    auto actual = std::find_if(items.begin(), items.end(), [&](auto& item) {
      return item->name == expected_item->name;
    });

    ASSERT_TRUE(actual != items.end());
    EXPECT_EQ(expected_item, *actual);
  }

  {
    mojom::MediaFeedItemPtr expected_item = mojom::MediaFeedItem::New();
    expected_item->id = 7;
    expected_item->name = base::ASCIIToUTF16("Big Buck Bunny");
    expected_item->type = mojom::MediaFeedItemType::kMovie;
    ASSERT_TRUE(
        base::Time::FromString("2008-01-01", &expected_item->date_published));
    auto content_rating = mojom::ContentRating::New();
    content_rating->agency = "MPAA";
    content_rating->value = "G";
    expected_item->content_ratings.push_back(std::move(content_rating));
    expected_item->duration = base::TimeDelta::FromMinutes(12);
    expected_item->genre.push_back("Comedy");
    expected_item->is_family_friendly =
        media_feeds::mojom::IsFamilyFriendly::kNo;
    expected_item->action = mojom::Action::New();
    expected_item->action->url = GURL(
        "https://mounirlamouri.github.io/sandbox/media/dynamic-controls.html");
    auto image = mojom::MediaImage::New();
    image->size = ::gfx::Size(1392, 749);
    image->src = GURL(
        "https://beccahughes.github.io/media/media-feeds/big_buck_bunny.jpg");
    image->content_attributes = {mojom::ContentAttribute::kHasTitle,
                                 mojom::ContentAttribute::kIconic,
                                 mojom::ContentAttribute::kPoster};
    expected_item->images.push_back(std::move(image));
    auto image2 = mojom::MediaImage::New();
    image2->size = ::gfx::Size(1600, 900);
    image2->src = GURL(
        "https://beccahughes.github.io/media/media-feeds/"
        "big_buck_bunny_bg.jpg");
    image2->content_attributes = {mojom::ContentAttribute::kIconic,
                                  mojom::ContentAttribute::kBackground};
    expected_item->images.push_back(std::move(image2));

    auto actual = std::find_if(items.begin(), items.end(), [&](auto& item) {
      return item->name == expected_item->name;
    });

    ASSERT_TRUE(actual != items.end());
    EXPECT_EQ(expected_item, *actual);
  }
}

// Media feeds should successfully fetch and convert a minimal example feed
// with all fields correctly populated.
IN_PROC_BROWSER_TEST_F(MediaFeedsBrowserTest, DiscoverAndFetchMinimal) {
  DiscoverFeed(kMediaFeedsMinTestURL);

  // Check we discovered the feed.
  std::set<GURL> expected_urls = {GetServer()->GetURL("/media-feed-min.json")};
  EXPECT_EQ(expected_urls, GetDiscoveredFeedURLs());

  std::vector<media_feeds::mojom::MediaFeedPtr> discovered_feeds =
      GetDiscoveredFeeds();
  EXPECT_EQ(1u, discovered_feeds.size());

  base::RunLoop run_loop;
  GetMediaFeedsService()->FetchMediaFeed(
      discovered_feeds[0]->id,
      base::BindLambdaForTesting(
          [&](const std::string& ignored) { run_loop.Quit(); }));
  run_loop.Run();
  WaitForDB();

  auto feeds = GetMediaFeedsSync();

  auto logo1 = mojom::MediaImage::New();
  logo1->size = ::gfx::Size(1113, 245);
  logo1->src = GURL(
      "https://beccahughes.github.io/media/media-feeds/"
      "chromium_logo_white.png");
  logo1->content_attributes = {
      mojom::ContentAttribute::kForDarkBackground,
      mojom::ContentAttribute::kHasTitle,
      mojom::ContentAttribute::kHasTransparentBackground};
  auto logo2 = mojom::MediaImage::New();
  logo2->size = ::gfx::Size(600, 315);
  logo2->src =
      GURL("https://beccahughes.github.io/media/media-feeds/chromium_card.png");
  logo2->content_attributes = {mojom::ContentAttribute::kForLightBackground,
                               mojom::ContentAttribute::kHasTitle,
                               mojom::ContentAttribute::kCentered};

  // First, check the feed metadata.
  ASSERT_EQ(1u, feeds.size());
  EXPECT_EQ("Chromium Developers", feeds[0]->display_name);
  ASSERT_EQ(2u, feeds[0]->logos.size());
  EXPECT_EQ(logo1, feeds[0]->logos[0]);
  EXPECT_EQ(logo2, feeds[0]->logos[1]);
  EXPECT_TRUE(feeds[0]->cookie_name_filter.empty());

  auto items = GetItemsForMediaFeedSync(discovered_feeds[0]->id);

  EXPECT_EQ(3u, items.size());

  // Check that we fetched, validated, and converted all the items, and that all
  // the correct fields are set. Don't assume or require any specific ordering.
  {
    mojom::MediaFeedItemPtr expected_item = mojom::MediaFeedItem::New();
    expected_item->id = 1;
    expected_item->name =
        base::ASCIIToUTF16("Anatomy of a Web Media Experience");
    expected_item->type = mojom::MediaFeedItemType::kVideo;
    expected_item->author = mojom::Author::New();
    expected_item->author->name = "Google Chrome Developers";
    expected_item->author->url =
        GURL("https://www.youtube.com/user/ChromeDevelopers");
    ASSERT_TRUE(
        base::Time::FromString("2019-05-09", &expected_item->date_published));
    expected_item->duration =
        base::TimeDelta::FromMinutes(34) + base::TimeDelta::FromSeconds(41);
    expected_item->is_family_friendly =
        media_feeds::mojom::IsFamilyFriendly::kYes;
    expected_item->action = mojom::Action::New();
    expected_item->action->url =
        GURL("https://www.youtube.com/watch?v=lXm6jOQLe1Y");
    auto image = mojom::MediaImage::New();
    image->size = ::gfx::Size(336, 188);
    image->src =
        GURL("https://beccahughes.github.io/media/media-feeds/video1.webp");
    expected_item->images.push_back(std::move(image));

    auto actual = std::find_if(items.begin(), items.end(), [](auto& item) {
      return base::UTF16ToASCII(item->name) ==
             "Anatomy of a Web Media Experience";
    });

    ASSERT_TRUE(actual != items.end());
    EXPECT_EQ(expected_item, *actual);
  }

  {
    mojom::MediaFeedItemPtr expected_item = mojom::MediaFeedItem::New();
    expected_item->id = 2;
    expected_item->name = base::ASCIIToUTF16("Chrome Releases");
    expected_item->type = mojom::MediaFeedItemType::kTVSeries;
    ASSERT_TRUE(
        base::Time::FromString("2019-11-10", &expected_item->date_published));
    expected_item->is_family_friendly =
        media_feeds::mojom::IsFamilyFriendly::kYes;

    expected_item->tv_episode = mojom::TVEpisode::New();
    expected_item->tv_episode->duration =
        base::TimeDelta::FromMinutes(4) + base::TimeDelta::FromSeconds(16);
    expected_item->tv_episode->episode_number = 79;
    expected_item->tv_episode->season_number = 1;
    expected_item->tv_episode->name = "New in Chrome 79";
    auto episode_image = mojom::MediaImage::New();
    episode_image->size = ::gfx::Size(1874, 970);
    episode_image->src = GURL(
        "https://beccahughes.github.io/media/media-feeds/chrome79_current.png");
    expected_item->tv_episode->images.push_back(std::move(episode_image));

    expected_item->action_status = mojom::MediaFeedItemActionStatus::kActive;
    expected_item->action = mojom::Action::New();
    expected_item->action->url =
        GURL("https://www.youtube.com/watch?v=L0OB0_bO5I0?t=254");
    expected_item->action->start_time =
        base::TimeDelta::FromMinutes(4) + base::TimeDelta::FromSeconds(14);
    auto image = mojom::MediaImage::New();
    image->size = ::gfx::Size(336, 188);
    image->src =
        GURL("https://beccahughes.github.io/media/media-feeds/chromerel.webp");
    expected_item->images.push_back(std::move(image));

    auto actual = std::find_if(items.begin(), items.end(), [](auto& item) {
      return base::UTF16ToASCII(item->name) == "Chrome Releases";
    });

    ASSERT_TRUE(actual != items.end());
    EXPECT_EQ(expected_item, *actual);
  }

  {
    mojom::MediaFeedItemPtr expected_item = mojom::MediaFeedItem::New();
    expected_item->id = 3;
    expected_item->name = base::ASCIIToUTF16("Big Buck Bunny");
    expected_item->type = mojom::MediaFeedItemType::kMovie;
    ASSERT_TRUE(
        base::Time::FromString("2008-01-01", &expected_item->date_published));
    expected_item->is_family_friendly =
        media_feeds::mojom::IsFamilyFriendly::kNo;
    expected_item->duration = base::TimeDelta::FromMinutes(12);
    expected_item->action = mojom::Action::New();
    expected_item->action->url = GURL(
        "https://mounirlamouri.github.io/sandbox/media/dynamic-controls.html");
    auto image = mojom::MediaImage::New();
    image->size = ::gfx::Size(1392, 749);
    image->src = GURL(
        "https://beccahughes.github.io/media/media-feeds/big_buck_bunny.jpg");
    expected_item->images.push_back(std::move(image));

    auto actual = std::find_if(items.begin(), items.end(), [](auto& item) {
      return base::UTF16ToASCII(item->name) == "Big Buck Bunny";
    });

    ASSERT_TRUE(actual != items.end());
    EXPECT_EQ(expected_item, *actual);
  }
}

IN_PROC_BROWSER_TEST_F(MediaFeedsBrowserTest, ResetMediaFeed_OnNavigation) {
  DiscoverFeed(kMediaFeedsTestURL);

  {
    auto feeds = GetDiscoveredFeeds();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);

    // Fetch the feed.
    base::RunLoop run_loop;
    GetMediaFeedsService()->FetchMediaFeed(
        feeds[0]->id,
        base::BindLambdaForTesting(
            [&](const std::string& ignored) { run_loop.Quit(); }));
    run_loop.Run();
    WaitForDB();
  }

  // Navigate on the same origin and make sure we do not reset.
  ui_test_utils::NavigateToURL(browser(),
                               GetServer()->GetURL(kMediaFeedsAltTestURL));
  WaitForDB();

  {
    auto feeds = GetDiscoveredFeeds();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }

  // Navigate to a different origin and make sure we reset.
  ui_test_utils::NavigateToURL(
      browser(), GetServer()->GetURL("www.example.com", kMediaFeedsAltTestURL));
  WaitForDB();

  {
    auto feeds = GetDiscoveredFeeds();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kVisit, feeds[0]->reset_reason);
  }
}

IN_PROC_BROWSER_TEST_F(MediaFeedsBrowserTest,
                       ResetMediaFeed_OnNavigation_NeverFetched) {
  DiscoverFeed(kMediaFeedsTestURL);

  ui_test_utils::NavigateToURL(
      browser(), GetServer()->GetURL("www.example.com", kMediaFeedsAltTestURL));
  WaitForDB();

  {
    auto feeds = GetDiscoveredFeeds();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }
}

IN_PROC_BROWSER_TEST_F(MediaFeedsBrowserTest,
                       ResetMediaFeed_OnNavigation_WrongOrigin) {
  DiscoverFeed(kMediaFeedsTestURL);

  ui_test_utils::NavigateToURL(
      browser(), GetServer()->GetURL("www.example.com", kMediaFeedsAltTestURL));
  WaitForDB();

  {
    auto feeds = GetDiscoveredFeeds();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);

    // Fetch the feed.
    base::RunLoop run_loop;
    GetMediaFeedsService()->FetchMediaFeed(
        feeds[0]->id,
        base::BindLambdaForTesting(
            [&](const std::string& ignored) { run_loop.Quit(); }));
    run_loop.Run();
    WaitForDB();
  }

  // The navigation is not on an origin associated with the feed so we should
  // never reset it.
  ui_test_utils::NavigateToURL(
      browser(),
      GetServer()->GetURL("www.example2.com", kMediaFeedsAltTestURL));
  WaitForDB();

  {
    auto feeds = GetDiscoveredFeeds();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }
}

// Flaky on lacros: crbug.com/1124983
#if BUILDFLAG(IS_LACROS)
#define MAYBE_ResetMediaFeed_WebContentsDestroyed \
  DISABLED_ResetMediaFeed_WebContentsDestroyed
#else
#define MAYBE_ResetMediaFeed_WebContentsDestroyed \
  ResetMediaFeed_WebContentsDestroyed
#endif
IN_PROC_BROWSER_TEST_F(MediaFeedsBrowserTest,
                       MAYBE_ResetMediaFeed_WebContentsDestroyed) {
  DiscoverFeed(kMediaFeedsTestURL);

  {
    auto feeds = GetDiscoveredFeeds();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);

    // Fetch the feed.
    base::RunLoop run_loop;
    GetMediaFeedsService()->FetchMediaFeed(
        feeds[0]->id,
        base::BindLambdaForTesting(
            [&](const std::string& ignored) { run_loop.Quit(); }));
    run_loop.Run();
    WaitForDB();
  }

  // If we destroy the web contents then we should reset the feed.
  browser()->tab_strip_model()->CloseAllTabs();
  WaitForDB();

  {
    auto feeds = GetDiscoveredFeeds();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kVisit, feeds[0]->reset_reason);
  }
}

// Parameterized test to check that media feed discovery works with different
// header HTMLs.
class MediaFeedsDiscoveryBrowserTest
    : public MediaFeedsBrowserTest,
      public ::testing::WithParamInterface<TestData> {
 public:
  net::EmbeddedTestServer* GetServer() override {
    return GetParam().https ? MediaFeedsBrowserTest::GetServer()
                            : embedded_test_server();
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override {
    if (request.relative_url == kMediaFeedsTestURL) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_content(base::StringPrintf(kMediaFeedsTestHTML,
                                               GetParam().head_html.c_str()));
      return response;
    }
    return nullptr;
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    MediaFeedsDiscoveryBrowserTest,
    ::testing::Values(
        TestData{"<link rel=media-feed type=\"application/ld+json\" "
                 "href=\"/test\"/>",
                 true},
        TestData{"<link rel=media-feed href=\"/test\"/>", true},
        TestData{"<link rel=media-feed type=\"application/ld+json\" "
                 "href=\"/test\"/>",
                 false, false},
        TestData{"", false},
        TestData{"<link rel=media-feed type=\"application/ld+json\" "
                 "href=\"/test\"/><link rel=media-feed "
                 "type=\"application/ld+json\" href=\"/test2\"/>",
                 true},
        TestData{"<link rel=media-feed type=\"application/ld+json\" "
                 "href=\"https://www.example.com/test\"/>",
                 false},
        TestData{
            "<link rel=media-feed type=\"application/ld+json\" href=\"\"/>",
            false},
        TestData{"<link rel=feed type=\"application/ld+json\" "
                 "href=\"/test\"/>",
                 false},
        TestData{
            "<link rel=other type=\"application/ld+json\" href=\"/test\"/>",
            false}));

IN_PROC_BROWSER_TEST_P(MediaFeedsDiscoveryBrowserTest, Discover) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  DiscoverFeed(kMediaFeedsTestURL);

  // Check we discovered the feed.
  std::set<GURL> expected_urls;

  if (GetParam().discovered)
    expected_urls.insert(GetServer()->GetURL("/test"));
  EXPECT_EQ(expected_urls, GetDiscoveredFeedURLs());

  // Check that we did/didn't record this to UKM.
  using Entry = ukm::builders::Media_Feed_Discover;
  auto entries = ukm_recorder.GetEntriesByName(Entry::kEntryName);

  if (GetParam().discovered) {
    EXPECT_EQ(1u, entries.size());
    ukm_recorder.ExpectEntrySourceHasUrl(
        entries[0], GetServer()->GetURL(kMediaFeedsTestURL));
    ukm_recorder.ExpectEntryMetric(entries[0], Entry::kHasMediaFeedName, 1);
  } else {
    EXPECT_TRUE(entries.empty());
  }
}

}  // namespace media_feeds
