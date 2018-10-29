// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/browser_feature_extractor.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/browser_features.h"
#include "chrome/browser/safe_browsing/client_side_detection_host.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_service.h"
#include "components/safe_browsing/db/database_manager.h"
#include "components/safe_browsing/db/test_database_manager.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::WebContentsTester;

using testing::DoAll;
using testing::Return;
using testing::StrictMock;

namespace safe_browsing {

namespace {

class MockSafeBrowsingDatabaseManager : public TestSafeBrowsingDatabaseManager {
 public:
  MockSafeBrowsingDatabaseManager() {}

  MOCK_METHOD1(MatchMalwareIP, bool(const std::string& ip_address));

 protected:
  ~MockSafeBrowsingDatabaseManager() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSafeBrowsingDatabaseManager);
};

class MockClientSideDetectionHost : public ClientSideDetectionHost {
 public:
  MockClientSideDetectionHost(
      content::WebContents* tab,
      SafeBrowsingDatabaseManager* database_manager)
      : ClientSideDetectionHost(tab) {
    set_safe_browsing_managers(NULL, database_manager);
  }

  ~MockClientSideDetectionHost() override {}

  MOCK_METHOD1(IsBadIpAddress, bool(const std::string&));
};
}  // namespace

class BrowserFeatureExtractorTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ASSERT_TRUE(profile()->CreateHistoryService(
        true /* delete_file */, false /* no_db */));

    db_manager_ = new StrictMock<MockSafeBrowsingDatabaseManager>();
    host_.reset(new StrictMock<MockClientSideDetectionHost>(
        web_contents(), db_manager_.get()));
    extractor_.reset(
        new BrowserFeatureExtractor(web_contents(), host_.get()));
    num_pending_ = 0;
    browse_info_.reset(new BrowseInfo);
  }

  void TearDown() override {
    extractor_.reset();
    host_.reset();
    db_manager_ = NULL;
    ChromeRenderViewHostTestHarness::TearDown();
    ASSERT_EQ(0, num_pending_);
  }

  history::HistoryService* history_service() {
    return HistoryServiceFactory::GetForProfile(
        profile(), ServiceAccessType::EXPLICIT_ACCESS);
  }

  void SetRedirectChain(const std::vector<GURL>& redirect_chain,
                        bool new_host) {
    browse_info_->url_redirects = redirect_chain;
    if (new_host) {
      browse_info_->host_redirects = redirect_chain;
    }
  }

  // Wrapper around NavigateAndCommit that also sets the redirect chain to
  // a sane value.
  void SimpleNavigateAndCommit(const GURL& url) {
    std::vector<GURL> redirect_chain;
    redirect_chain.push_back(url);
    SetRedirectChain(redirect_chain, true);
    NavigateAndCommit(url, GURL(), ui::PAGE_TRANSITION_LINK);
  }

  // This is similar to NavigateAndCommit that is in WebContentsTester, but
  // allows us to specify the referrer and page_transition_type.
  void NavigateAndCommit(const GURL& url,
                         const GURL& referrer,
                         ui::PageTransition type) {
    auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
        url, web_contents());
    navigation->SetReferrer(
        content::Referrer(referrer, network::mojom::ReferrerPolicy::kDefault));
    navigation->SetTransition(type);
    navigation->Commit();
  }

  bool ExtractFeatures(ClientPhishingRequest* request) {
    StartExtractFeatures(request);
    base::RunLoop().Run();
    EXPECT_EQ(1U, success_.count(request));
    return success_.count(request) ? success_[request] : false;
  }

  void StartExtractFeatures(ClientPhishingRequest* request) {
    success_.erase(request);
    ++num_pending_;
    extractor_->ExtractFeatures(
        browse_info_.get(),
        request,
        base::Bind(&BrowserFeatureExtractorTest::ExtractFeaturesDone,
                   base::Unretained(this)));
  }

  void GetFeatureMap(const ClientPhishingRequest& request,
                     std::map<std::string, double>* features) {
    for (int i = 0; i < request.non_model_feature_map_size(); ++i) {
      const ClientPhishingRequest::Feature& feature =
          request.non_model_feature_map(i);
      EXPECT_EQ(0U, features->count(feature.name()));
      (*features)[feature.name()] = feature.value();
    }
  }

  void ExtractMalwareFeatures(ClientMalwareRequest* request) {
    // Feature extraction takes ownership of the request object
    // and passes it along to the done callback in the end.
    StartExtractMalwareFeatures(request);
    ASSERT_TRUE(base::MessageLoopForUI::IsCurrent());
    base::RunLoop().Run();
    EXPECT_EQ(1U, success_.count(request));
    EXPECT_TRUE(success_[request]);
  }

  void StartExtractMalwareFeatures(ClientMalwareRequest* request) {
    success_.erase(request);
    ++num_pending_;
    // We temporarily give up ownership of request to ExtractMalwareFeatures
    // but we'll regain ownership of it in ExtractMalwareFeaturesDone.
    extractor_->ExtractMalwareFeatures(
        browse_info_.get(),
        request,
        base::Bind(&BrowserFeatureExtractorTest::ExtractMalwareFeaturesDone,
                   base::Unretained(this)));
  }

  void GetMalwareUrls(
      const ClientMalwareRequest& request,
      std::map<std::string, std::set<std::string> >* urls) {
    for (int i = 0; i < request.bad_ip_url_info_size(); ++i) {
      const ClientMalwareRequest::UrlInfo& urlinfo =
          request.bad_ip_url_info(i);
      (*urls)[urlinfo.ip()].insert(urlinfo.url());
    }
  }

  int num_pending_;  // Number of pending feature extractions.
  std::unique_ptr<BrowserFeatureExtractor> extractor_;
  std::map<void*, bool> success_;
  std::unique_ptr<BrowseInfo> browse_info_;
  std::unique_ptr<StrictMock<MockClientSideDetectionHost>> host_;
  scoped_refptr<StrictMock<MockSafeBrowsingDatabaseManager> > db_manager_;

 private:
  void ExtractFeaturesDone(bool success,
                           std::unique_ptr<ClientPhishingRequest> request) {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    ASSERT_EQ(0U, success_.count(request.get()));
    // The pointer doesn't really belong to us.  It belongs to
    // the test case which passed it to ExtractFeatures above.
    success_[request.release()] = success;
    if (--num_pending_ == 0) {
      base::RunLoop::QuitCurrentWhenIdleDeprecated();
    }
  }

  void ExtractMalwareFeaturesDone(
      bool success,
      std::unique_ptr<ClientMalwareRequest> request) {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    ASSERT_EQ(0U, success_.count(request.get()));
    // The pointer doesn't really belong to us.  It belongs to
    // the test case which passed it to ExtractMalwareFeatures above.
    success_[request.release()] = success;
    if (--num_pending_ == 0) {
      base::RunLoop::QuitCurrentWhenIdleDeprecated();
    }
  }
};

TEST_F(BrowserFeatureExtractorTest, UrlNotInHistory) {
  ClientPhishingRequest request;
  SimpleNavigateAndCommit(GURL("http://www.google.com"));
  request.set_url("http://www.google.com/");
  request.set_client_score(0.5);
  EXPECT_FALSE(ExtractFeatures(&request));
}

TEST_F(BrowserFeatureExtractorTest, RequestNotInitialized) {
  ClientPhishingRequest request;
  request.set_url("http://www.google.com/");
  // Request is missing the score value.
  SimpleNavigateAndCommit(GURL("http://www.google.com"));
  EXPECT_FALSE(ExtractFeatures(&request));
}

TEST_F(BrowserFeatureExtractorTest, UrlInHistory) {
  history_service()->AddPage(GURL("http://www.foo.com/bar.html"),
                             base::Time::Now(),
                             history::SOURCE_BROWSED);
  history_service()->AddPage(GURL("https://www.foo.com/gaa.html"),
                             base::Time::Now(),
                             history::SOURCE_BROWSED);  // same host HTTPS.
  history_service()->AddPage(GURL("http://www.foo.com/gaa.html"),
                             base::Time::Now(),
                             history::SOURCE_BROWSED);  // same host HTTP.
  history_service()->AddPage(GURL("http://bar.foo.com/gaa.html"),
                             base::Time::Now(),
                             history::SOURCE_BROWSED);  // different host.
  history_service()->AddPage(GURL("http://www.foo.com/bar.html?a=b"),
                             base::Time::Now() - base::TimeDelta::FromHours(23),
                             NULL, 0, GURL(), history::RedirectList(),
                             ui::PAGE_TRANSITION_LINK,
                             history::SOURCE_BROWSED, false);
  history_service()->AddPage(GURL("http://www.foo.com/bar.html"),
                             base::Time::Now() - base::TimeDelta::FromHours(25),
                             NULL, 0, GURL(), history::RedirectList(),
                             ui::PAGE_TRANSITION_TYPED,
                             history::SOURCE_BROWSED, false);
  history_service()->AddPage(GURL("https://www.foo.com/goo.html"),
                             base::Time::Now() - base::TimeDelta::FromDays(5),
                             NULL, 0, GURL(), history::RedirectList(),
                             ui::PAGE_TRANSITION_TYPED,
                             history::SOURCE_BROWSED, false);

  SimpleNavigateAndCommit(GURL("http://www.foo.com/bar.html"));

  ClientPhishingRequest request;
  request.set_url("http://www.foo.com/bar.html");
  request.set_client_score(0.5);
  EXPECT_TRUE(ExtractFeatures(&request));
  std::map<std::string, double> features;
  GetFeatureMap(request, &features);

  EXPECT_EQ(12U, features.size());
  EXPECT_DOUBLE_EQ(2.0, features[kUrlHistoryVisitCount]);
  EXPECT_DOUBLE_EQ(1.0, features[kUrlHistoryVisitCountMoreThan24hAgo]);
  EXPECT_DOUBLE_EQ(1.0, features[kUrlHistoryTypedCount]);
  EXPECT_DOUBLE_EQ(1.0, features[kUrlHistoryLinkCount]);
  EXPECT_DOUBLE_EQ(4.0, features[kHttpHostVisitCount]);
  EXPECT_DOUBLE_EQ(2.0, features[kHttpsHostVisitCount]);
  EXPECT_DOUBLE_EQ(1.0, features[kFirstHttpHostVisitMoreThan24hAgo]);
  EXPECT_DOUBLE_EQ(1.0, features[kFirstHttpsHostVisitMoreThan24hAgo]);

  request.Clear();
  request.set_url("https://www.foo.com/gaa.html");
  request.set_client_score(0.5);
  EXPECT_TRUE(ExtractFeatures(&request));
  features.clear();
  GetFeatureMap(request, &features);

  EXPECT_EQ(8U, features.size());
  EXPECT_DOUBLE_EQ(1.0, features[kUrlHistoryVisitCount]);
  EXPECT_DOUBLE_EQ(0.0, features[kUrlHistoryVisitCountMoreThan24hAgo]);
  EXPECT_DOUBLE_EQ(0.0, features[kUrlHistoryTypedCount]);
  EXPECT_DOUBLE_EQ(1.0, features[kUrlHistoryLinkCount]);
  EXPECT_DOUBLE_EQ(4.0, features[kHttpHostVisitCount]);
  EXPECT_DOUBLE_EQ(2.0, features[kHttpsHostVisitCount]);
  EXPECT_DOUBLE_EQ(1.0, features[kFirstHttpHostVisitMoreThan24hAgo]);
  EXPECT_DOUBLE_EQ(1.0, features[kFirstHttpsHostVisitMoreThan24hAgo]);

  request.Clear();
  request.set_url("http://bar.foo.com/gaa.html");
  request.set_client_score(0.5);
  EXPECT_TRUE(ExtractFeatures(&request));
  features.clear();
  GetFeatureMap(request, &features);
  // We have less features because we didn't Navigate to this page, so we don't
  // have Referrer, IsFirstNavigation, HasSSLReferrer, etc.
  EXPECT_EQ(7U, features.size());
  EXPECT_DOUBLE_EQ(1.0, features[kUrlHistoryVisitCount]);
  EXPECT_DOUBLE_EQ(0.0, features[kUrlHistoryVisitCountMoreThan24hAgo]);
  EXPECT_DOUBLE_EQ(0.0, features[kUrlHistoryTypedCount]);
  EXPECT_DOUBLE_EQ(1.0, features[kUrlHistoryLinkCount]);
  EXPECT_DOUBLE_EQ(1.0, features[kHttpHostVisitCount]);
  EXPECT_DOUBLE_EQ(0.0, features[kHttpsHostVisitCount]);
  EXPECT_DOUBLE_EQ(0.0, features[kFirstHttpHostVisitMoreThan24hAgo]);
  EXPECT_FALSE(features.count(kFirstHttpsHostVisitMoreThan24hAgo));
}

TEST_F(BrowserFeatureExtractorTest, MultipleRequestsAtOnce) {
  history_service()->AddPage(GURL("http://www.foo.com/bar.html"),
                             base::Time::Now(),
                             history::SOURCE_BROWSED);
  SimpleNavigateAndCommit(GURL("http:/www.foo.com/bar.html"));
  ClientPhishingRequest request;
  request.set_url("http://www.foo.com/bar.html");
  request.set_client_score(0.5);
  StartExtractFeatures(&request);

  SimpleNavigateAndCommit(GURL("http://www.foo.com/goo.html"));
  ClientPhishingRequest request2;
  request2.set_url("http://www.foo.com/goo.html");
  request2.set_client_score(1.0);
  StartExtractFeatures(&request2);

  base::RunLoop().Run();
  EXPECT_TRUE(success_[&request]);
  // Success is false because the second URL is not in the history and we are
  // not able to distinguish between a missing URL in the history and an error.
  EXPECT_FALSE(success_[&request2]);
}

TEST_F(BrowserFeatureExtractorTest, BrowseFeatures) {
  history_service()->AddPage(GURL("http://www.foo.com/"),
                             base::Time::Now(),
                             history::SOURCE_BROWSED);
  history_service()->AddPage(GURL("http://www.foo.com/page.html"),
                             base::Time::Now(),
                             history::SOURCE_BROWSED);
  history_service()->AddPage(GURL("http://www.bar.com/"),
                             base::Time::Now(),
                             history::SOURCE_BROWSED);
  history_service()->AddPage(GURL("http://www.bar.com/other_page.html"),
                             base::Time::Now(),
                             history::SOURCE_BROWSED);
  history_service()->AddPage(GURL("http://www.baz.com/"),
                             base::Time::Now(),
                             history::SOURCE_BROWSED);

  ClientPhishingRequest request;
  request.set_url("http://www.foo.com/");
  request.set_client_score(0.5);
  std::vector<GURL> redirect_chain;
  redirect_chain.push_back(GURL("http://somerandomwebsite.com/"));
  redirect_chain.push_back(GURL("http://www.foo.com/"));
  SetRedirectChain(redirect_chain, true);
  browse_info_->http_status_code = 200;
  NavigateAndCommit(GURL("http://www.foo.com/"),
                    GURL("http://google.com/"),
                    ui::PageTransitionFromInt(
                        ui::PAGE_TRANSITION_AUTO_BOOKMARK |
                        ui::PAGE_TRANSITION_FORWARD_BACK));

  EXPECT_TRUE(ExtractFeatures(&request));
  std::map<std::string, double> features;
  GetFeatureMap(request, &features);

  EXPECT_EQ(
      1.0,
      features[base::StringPrintf("%s=%s", kReferrer, "http://google.com/")]);
  EXPECT_EQ(1.0, features[base::StringPrintf("%s[0]=%s", kRedirect,
                                             "http://somerandomwebsite.com/")]);
  // We shouldn't have a feature for the last redirect in the chain, since it
  // should always be the URL that we navigated to.
  EXPECT_EQ(
      0.0,
      features[base::StringPrintf("%s[1]=%s", kRedirect, "http://foo.com/")]);
  EXPECT_EQ(0.0, features[kHasSSLReferrer]);
  EXPECT_EQ(2.0, features[kPageTransitionType]);
  EXPECT_EQ(1.0, features[kIsFirstNavigation]);
  EXPECT_EQ(200.0, features[kHttpStatusCode]);

  request.Clear();
  request.set_url("http://www.foo.com/page.html");
  request.set_client_score(0.5);
  redirect_chain.clear();
  redirect_chain.push_back(GURL("http://www.foo.com/redirect"));
  redirect_chain.push_back(GURL("http://www.foo.com/second_redirect"));
  redirect_chain.push_back(GURL("http://www.foo.com/page.html"));
  SetRedirectChain(redirect_chain, false);
  browse_info_->http_status_code = 404;
  NavigateAndCommit(GURL("http://www.foo.com/page.html"),
                    GURL("http://www.foo.com"),
                    ui::PageTransitionFromInt(
                        ui::PAGE_TRANSITION_TYPED |
                        ui::PAGE_TRANSITION_CHAIN_START |
                        ui::PAGE_TRANSITION_CLIENT_REDIRECT));

  EXPECT_TRUE(ExtractFeatures(&request));
  features.clear();
  GetFeatureMap(request, &features);

  EXPECT_EQ(
      1,
      features[base::StringPrintf("%s=%s", kReferrer, "http://www.foo.com/")]);
  EXPECT_EQ(1.0, features[base::StringPrintf("%s[0]=%s", kRedirect,
                                             "http://www.foo.com/redirect")]);
  EXPECT_EQ(1.0,
            features[base::StringPrintf("%s[1]=%s", kRedirect,
                                        "http://www.foo.com/second_redirect")]);
  EXPECT_EQ(0.0, features[kHasSSLReferrer]);
  EXPECT_EQ(1.0, features[kPageTransitionType]);
  EXPECT_EQ(0.0, features[kIsFirstNavigation]);
  EXPECT_EQ(1.0, features[base::StringPrintf("%s%s=%s", kHostPrefix, kReferrer,
                                             "http://google.com/")]);
  EXPECT_EQ(1.0,
            features[base::StringPrintf("%s%s[0]=%s", kHostPrefix, kRedirect,
                                        "http://somerandomwebsite.com/")]);
  EXPECT_EQ(
      2.0,
      features[base::StringPrintf("%s%s", kHostPrefix, kPageTransitionType)]);
  EXPECT_EQ(
      1.0,
      features[base::StringPrintf("%s%s", kHostPrefix, kIsFirstNavigation)]);
  EXPECT_EQ(404.0, features[kHttpStatusCode]);

  request.Clear();
  request.set_url("http://www.bar.com/");
  request.set_client_score(0.5);
  redirect_chain.clear();
  redirect_chain.push_back(GURL("http://www.foo.com/page.html"));
  redirect_chain.push_back(GURL("http://www.bar.com/"));
  SetRedirectChain(redirect_chain, true);
  NavigateAndCommit(GURL("http://www.bar.com/"),
                    GURL("http://www.foo.com/page.html"),
                    ui::PageTransitionFromInt(
                        ui::PAGE_TRANSITION_LINK |
                        ui::PAGE_TRANSITION_CHAIN_END |
                        ui::PAGE_TRANSITION_CLIENT_REDIRECT));

  EXPECT_TRUE(ExtractFeatures(&request));
  features.clear();
  GetFeatureMap(request, &features);

  EXPECT_EQ(1.0, features[base::StringPrintf("%s=%s", kReferrer,
                                             "http://www.foo.com/page.html")]);
  EXPECT_EQ(1.0, features[base::StringPrintf("%s[0]=%s", kRedirect,
                                             "http://www.foo.com/page.html")]);
  EXPECT_EQ(0.0, features[kHasSSLReferrer]);
  EXPECT_EQ(0.0, features[kPageTransitionType]);
  EXPECT_EQ(0.0, features[kIsFirstNavigation]);

  // Should not have host features.
  EXPECT_EQ(0U, features.count(base::StringPrintf("%s%s", kHostPrefix,
                                                  kPageTransitionType)));
  EXPECT_EQ(0U, features.count(base::StringPrintf("%s%s", kHostPrefix,
                                                  kIsFirstNavigation)));

  request.Clear();
  request.set_url("http://www.bar.com/other_page.html");
  request.set_client_score(0.5);
  redirect_chain.clear();
  redirect_chain.push_back(GURL("http://www.bar.com/other_page.html"));
  SetRedirectChain(redirect_chain, false);
  NavigateAndCommit(GURL("http://www.bar.com/other_page.html"),
                    GURL("http://www.bar.com/"),
                    ui::PAGE_TRANSITION_LINK);

  EXPECT_TRUE(ExtractFeatures(&request));
  features.clear();
  GetFeatureMap(request, &features);

  EXPECT_EQ(
      1.0,
      features[base::StringPrintf("%s=%s", kReferrer, "http://www.bar.com/")]);
  EXPECT_EQ(0.0, features[kHasSSLReferrer]);
  EXPECT_EQ(0.0, features[kPageTransitionType]);
  EXPECT_EQ(0.0, features[kIsFirstNavigation]);
  EXPECT_EQ(1.0, features[base::StringPrintf("%s%s=%s", kHostPrefix, kReferrer,
                                             "http://www.foo.com/page.html")]);
  EXPECT_EQ(1.0,
            features[base::StringPrintf("%s%s[0]=%s", kHostPrefix, kRedirect,
                                        "http://www.foo.com/page.html")]);
  EXPECT_EQ(
      0.0,
      features[base::StringPrintf("%s%s", kHostPrefix, kPageTransitionType)]);
  EXPECT_EQ(
      0.0,
      features[base::StringPrintf("%s%s", kHostPrefix, kIsFirstNavigation)]);
  request.Clear();
  request.set_url("http://www.baz.com/");
  request.set_client_score(0.5);
  redirect_chain.clear();
  redirect_chain.push_back(GURL("https://bankofamerica.com"));
  redirect_chain.push_back(GURL("http://www.baz.com/"));
  SetRedirectChain(redirect_chain, true);
  NavigateAndCommit(GURL("http://www.baz.com"),
                    GURL("https://bankofamerica.com"),
                    ui::PAGE_TRANSITION_GENERATED);

  EXPECT_TRUE(ExtractFeatures(&request));
  features.clear();
  GetFeatureMap(request, &features);

  EXPECT_EQ(1.0, features[base::StringPrintf("%s[0]=%s", kRedirect,
                                             kSecureRedirectValue)]);
  EXPECT_EQ(1.0, features[kHasSSLReferrer]);
  EXPECT_EQ(5.0, features[kPageTransitionType]);
  // Should not have redirect or host features.
  EXPECT_EQ(0U, features.count(base::StringPrintf("%s%s", kHostPrefix,
                                                  kPageTransitionType)));
  EXPECT_EQ(0U, features.count(base::StringPrintf("%s%s", kHostPrefix,
                                                  kIsFirstNavigation)));
  EXPECT_EQ(5.0, features[kPageTransitionType]);
}

TEST_F(BrowserFeatureExtractorTest, SafeBrowsingFeatures) {
  SimpleNavigateAndCommit(GURL("http://www.foo.com/malware.html"));
  ClientPhishingRequest request;
  request.set_url("http://www.foo.com/malware.html");
  request.set_client_score(0.5);

  browse_info_->unsafe_resource.reset(
      new security_interstitials::UnsafeResource);
  browse_info_->unsafe_resource->url = GURL("http://www.malware.com/");
  browse_info_->unsafe_resource->original_url = GURL("http://www.good.com/");
  browse_info_->unsafe_resource->is_subresource = true;
  browse_info_->unsafe_resource->threat_type = SB_THREAT_TYPE_URL_MALWARE;

  ExtractFeatures(&request);
  std::map<std::string, double> features;
  GetFeatureMap(request, &features);
  EXPECT_TRUE(features.count(base::StringPrintf(
      "%s%s", kSafeBrowsingMaliciousUrl, "http://www.malware.com/")));
  EXPECT_TRUE(features.count(base::StringPrintf(
      "%s%s", kSafeBrowsingOriginalUrl, "http://www.good.com/")));
  EXPECT_DOUBLE_EQ(1.0, features[kSafeBrowsingIsSubresource]);
  EXPECT_DOUBLE_EQ(SB_THREAT_TYPE_URL_MALWARE,
                   features[kSafeBrowsingThreatType]);
}

TEST_F(BrowserFeatureExtractorTest, MalwareFeatures) {
  ClientMalwareRequest request;
  request.set_url("http://www.foo.com/");

  std::vector<IPUrlInfo> bad_urls;
  bad_urls.push_back(
      IPUrlInfo("http://bad.com", "GET", "", content::RESOURCE_TYPE_SCRIPT));
  bad_urls.push_back(
      IPUrlInfo("http://evil.com", "GET", "", content::RESOURCE_TYPE_SCRIPT));
  browse_info_->ips.insert(std::make_pair("193.5.163.8", bad_urls));
  browse_info_->ips.insert(std::make_pair("92.92.92.92", bad_urls));
  std::vector<IPUrlInfo> good_urls;
  good_urls.push_back(
      IPUrlInfo("http://ok.com", "GET", "", content::RESOURCE_TYPE_SCRIPT));
  browse_info_->ips.insert(std::make_pair("23.94.78.1", good_urls));
  EXPECT_CALL(*db_manager_, MatchMalwareIP("193.5.163.8"))
      .WillOnce(Return(true));
  EXPECT_CALL(*db_manager_, MatchMalwareIP("92.92.92.92"))
      .WillOnce(Return(true));
  EXPECT_CALL(*db_manager_, MatchMalwareIP("23.94.78.1"))
      .WillOnce(Return(false));

  ExtractMalwareFeatures(&request);
  EXPECT_EQ(4, request.bad_ip_url_info_size());
  std::map<std::string, std::set<std::string> > result_urls;
  GetMalwareUrls(request, &result_urls);

  EXPECT_EQ(2U, result_urls.size());
  EXPECT_TRUE(result_urls.count("193.5.163.8"));
  std::set<std::string> urls = result_urls["193.5.163.8"];
  EXPECT_EQ(2U, urls.size());
  EXPECT_TRUE(urls.find("http://bad.com") != urls.end());
  EXPECT_TRUE(urls.find("http://evil.com") != urls.end());
  EXPECT_TRUE(result_urls.count("92.92.92.92"));
  urls = result_urls["92.92.92.92"];
  EXPECT_EQ(2U, urls.size());
  EXPECT_TRUE(urls.find("http://bad.com") != urls.end());
  EXPECT_TRUE(urls.find("http://evil.com") != urls.end());
}

TEST_F(BrowserFeatureExtractorTest, MalwareFeatures_ExceedLimit) {
  ClientMalwareRequest request;
  request.set_url("http://www.foo.com/");

  std::vector<IPUrlInfo> bad_urls;
  bad_urls.push_back(
      IPUrlInfo("http://bad.com", "GET", "", content::RESOURCE_TYPE_SCRIPT));
  std::vector<std::string> ips;
  for (int i = 0; i < 7; ++i) {  // Add 7 ips
    std::string ip = base::StringPrintf("%d.%d.%d.%d", i, i, i, i);
    ips.push_back(ip);
    browse_info_->ips.insert(std::make_pair(ip, bad_urls));

    // First ip is good but all the others are bad.
    EXPECT_CALL(*db_manager_, MatchMalwareIP(ip)).WillOnce(Return(i > 0));
  }

  ExtractMalwareFeatures(&request);
  // The number of IP matched url we store is capped at 5 IPs per request.
  EXPECT_EQ(5, request.bad_ip_url_info_size());
}

}  // namespace safe_browsing
