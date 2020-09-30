// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_host.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/safe_browsing/browser_feature_extractor.h"
#include "chrome/browser/safe_browsing/client_side_detection_service.h"
#include "chrome/browser/safe_browsing/client_side_model_loader.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom-shared.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/db/database_manager.h"
#include "components/safe_browsing/core/db/test_database_manager.h"
#include "components/safe_browsing/core/features.h"
#include "components/safe_browsing/core/proto/csd.pb.h"
#include "components/security_interstitials/content/unsafe_resource_util.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "ipc/ipc_test_sink.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::RenderFrameHostTester;
using content::WebContents;
using ::testing::_;
using ::testing::DeleteArg;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::IsNull;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using ::testing::StrictMock;

namespace {

const bool kFalse = false;
const bool kTrue = true;

std::unique_ptr<content::NavigationSimulator> NavigateAndKeepLoading(
    content::WebContents* web_contents,
    const GURL& url) {
  auto navigation =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents);
  navigation->SetKeepLoading(true);
  navigation->Commit();
  return navigation;
}

}  // namespace

namespace safe_browsing {
namespace {
// This matcher verifies that the client computed verdict
// (ClientPhishingRequest) which is passed to SendClientReportPhishingRequest
// has the expected fields set.  Note: we can't simply compare the protocol
// buffer strings because the BrowserFeatureExtractor might add features to the
// verdict object before calling SendClientReportPhishingRequest.
MATCHER_P(PartiallyEqualVerdict, other, "") {
  return (other.url() == arg->url() &&
          other.client_score() == arg->client_score() &&
          other.is_phishing() == arg->is_phishing());
}

// Test that the callback is nullptr when the verdict is not phishing.
MATCHER(CallbackIsNull, "") {
  return arg.is_null();
}

class MockModelLoader : public ModelLoader {
 public:
  MockModelLoader() : ModelLoader(base::Closure(), nullptr, false) {}
  ~MockModelLoader() override = default;

  MOCK_METHOD1(ScheduleFetch, void(int64_t));
  MOCK_METHOD0(CancelFetcher, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockModelLoader);
};

class MockClientSideDetectionService : public ClientSideDetectionService {
 public:
  MockClientSideDetectionService() : ClientSideDetectionService(nullptr) {}
  ~MockClientSideDetectionService() override {}

  MOCK_METHOD4(SendClientReportPhishingRequest,
               void(std::unique_ptr<ClientPhishingRequest>,
                    bool,
                    bool,
                    const ClientReportPhishingRequestCallback&));
  MOCK_CONST_METHOD1(IsPrivateIPAddress, bool(const std::string&));
  MOCK_METHOD2(GetValidCachedResult, bool(const GURL&, bool*));
  MOCK_METHOD1(IsInCache, bool(const GURL&));
  MOCK_METHOD0(OverPhishingReportLimit, bool());
  MOCK_METHOD0(GetModelStr, std::string());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockClientSideDetectionService);
};

class MockSafeBrowsingUIManager : public SafeBrowsingUIManager {
 public:
  explicit MockSafeBrowsingUIManager(SafeBrowsingService* service)
      : SafeBrowsingUIManager(service) { }

  MOCK_METHOD1(DisplayBlockingPage, void(const UnsafeResource& resource));

  // Helper function which calls OnBlockingPageComplete for this client
  // object.
  void InvokeOnBlockingPageComplete(
    const security_interstitials::UnsafeResource::UrlCheckCallback& callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    // Note: this will delete the client object in the case of the CsdClient
    // implementation.
    if (!callback.is_null())
      callback.Run(false /*proceed*/, true /*showed_interstitial*/);
  }

 protected:
  ~MockSafeBrowsingUIManager() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSafeBrowsingUIManager);
};

class MockSafeBrowsingDatabaseManager : public TestSafeBrowsingDatabaseManager {
 public:
  MockSafeBrowsingDatabaseManager() {}

  MOCK_METHOD2(CheckCsdWhitelistUrl,
               AsyncMatch(const GURL&, SafeBrowsingDatabaseManager::Client*));

 protected:
  ~MockSafeBrowsingDatabaseManager() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSafeBrowsingDatabaseManager);
};

class MockBrowserFeatureExtractor : public BrowserFeatureExtractor {
 public:
  explicit MockBrowserFeatureExtractor(WebContents* tab)
      : BrowserFeatureExtractor(tab) {}
  ~MockBrowserFeatureExtractor() override {}

  MOCK_METHOD3(ExtractFeatures,
               void(const BrowseInfo*,
                    std::unique_ptr<ClientPhishingRequest>,
                    BrowserFeatureExtractor::DoneCallback));
};

}  // namespace

class FakePhishingDetector : public mojom::PhishingDetector {
 public:
  FakePhishingDetector() = default;

  ~FakePhishingDetector() override = default;

  void BindReceiver(mojo::ScopedMessagePipeHandle handle) {
    receivers_.Add(this, mojo::PendingReceiver<mojom::PhishingDetector>(
                             std::move(handle)));
  }

  // mojom::PhishingDetector
  void SetPhishingModel(const std::string& model) override { model_ = model; }

  // mojom::PhishingDetector
  void StartPhishingDetection(
      const GURL& url,
      StartPhishingDetectionCallback callback) override {
    url_ = url;
    phishing_detection_started_ = true;

    // The callback must be run before destruction, so send a minimal
    // ClientPhishingRequest.
    ClientPhishingRequest request;
    request.set_client_score(0.8);
    std::move(callback).Run(mojom::PhishingDetectorResult::SUCCESS,
                            request.SerializeAsString());

    return;
  }

  void CheckMessage(const GURL* url) {
    if (!url) {
      EXPECT_FALSE(phishing_detection_started_);
    } else {
      ASSERT_TRUE(phishing_detection_started_);
      EXPECT_EQ(*url, url_);
    }
  }

  void CheckModel(const std::string& model) { EXPECT_EQ(model, model_); }

  void Reset() {
    phishing_detection_started_ = false;
    url_ = GURL();
    model_ = "";
  }

 private:
  mojo::ReceiverSet<mojom::PhishingDetector> receivers_;
  bool phishing_detection_started_ = false;
  GURL url_;
  std::string model_ = "";

  DISALLOW_COPY_AND_ASSIGN(FakePhishingDetector);
};

class ClientSideDetectionHostTestBase : public ChromeRenderViewHostTestHarness {
 public:
  typedef security_interstitials::UnsafeResource UnsafeResource;

  explicit ClientSideDetectionHostTestBase(bool is_incognito)
      : is_incognito_(is_incognito) {}

  void InitTestApi() {
    service_manager::InterfaceProvider* remote_interfaces =
        web_contents()->GetMainFrame()->GetRemoteInterfaces();

    service_manager::InterfaceProvider::TestApi test_api(remote_interfaces);

    test_api.SetBinderForName(
        mojom::PhishingDetector::Name_,
        base::BindRepeating(&FakePhishingDetector::BindReceiver,
                            base::Unretained(&fake_phishing_detector_)));
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    if (is_incognito_) {
      auto incognito_web_contents =
          content::WebContentsTester::CreateTestWebContents(
              profile()->GetPrimaryOTRProfile(), nullptr);
      SetContents(std::move(incognito_web_contents));
    }

    InitTestApi();

    // Inject service classes.
    csd_service_.reset(new StrictMock<MockClientSideDetectionService>());
    database_manager_ = new StrictMock<MockSafeBrowsingDatabaseManager>();
    ui_manager_ = new StrictMock<MockSafeBrowsingUIManager>(
        // TODO(crbug/925153): Port consumers of the SafeBrowsingService to
        // use the interface in components/safe_browsing, and remove this cast.
        static_cast<safe_browsing::SafeBrowsingService*>(
            SafeBrowsingService::CreateSafeBrowsingService()));

    csd_host_ = ClientSideDetectionHost::Create(web_contents());
    csd_host_->set_client_side_detection_service(csd_service_.get());
    csd_host_->set_ui_manager(ui_manager_.get());
    csd_host_->set_database_manager(database_manager_.get());
    csd_host_->set_tick_clock_for_testing(&clock_);

    // We need to create this here since we don't call DidStopLanding in
    // this test.
    csd_host_->browse_info_.reset(new BrowseInfo);
  }

  void TearDown() override {
    // Delete the host object on the UI thread and release the
    // SafeBrowsingService.
    content::GetUIThreadTaskRunner({})->DeleteSoon(FROM_HERE,
                                                   csd_host_.release());
    database_manager_.reset();
    ui_manager_.reset();
    base::RunLoop().RunUntilIdle();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  void PhishingDetectionDone(const std::string& verdict_str) {
    csd_host_->PhishingDetectionDone(mojom::PhishingDetectorResult::SUCCESS,
                                     verdict_str);
  }

  void PhishingDetectionError(mojom::PhishingDetectorResult error) {
    csd_host_->PhishingDetectionDone(error, "");
  }

  void ExpectPreClassificationChecks(const GURL& url,
                                     const bool* is_private,
                                     const bool* match_csd_whitelist,
                                     const bool* get_valid_cached_result,
                                     const bool* is_in_cache,
                                     const bool* over_phishing_report_limit) {
    if (is_private) {
      EXPECT_CALL(*csd_service_, IsPrivateIPAddress(_))
          .WillOnce(Return(*is_private));
    }
    if (match_csd_whitelist) {
      EXPECT_CALL(*database_manager_.get(), CheckCsdWhitelistUrl(url, _))
          .WillOnce(Return(*match_csd_whitelist ? AsyncMatch::MATCH
                                                : AsyncMatch::NO_MATCH));
    }
    if (get_valid_cached_result) {
      EXPECT_CALL(*csd_service_, GetValidCachedResult(url, NotNull()))
          .WillOnce(
              DoAll(SetArgPointee<1>(true), Return(*get_valid_cached_result)));
    }
    if (is_in_cache) {
      EXPECT_CALL(*csd_service_, IsInCache(url)).WillOnce(Return(*is_in_cache));
    }
    if (over_phishing_report_limit) {
      EXPECT_CALL(*csd_service_, OverPhishingReportLimit())
          .WillOnce(Return(*over_phishing_report_limit));
    }
  }

  void WaitAndCheckPreClassificationChecks() {
    // Wait for CheckCsdWhitelist and CheckCache() to be called if at all.
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
    EXPECT_TRUE(Mock::VerifyAndClear(ui_manager_.get()));
    EXPECT_TRUE(Mock::VerifyAndClear(database_manager_.get()));
  }

  void SetFeatureExtractor(BrowserFeatureExtractor* extractor) {
    csd_host_->feature_extractor_.reset(extractor);
  }

  void SetRedirectChain(const std::vector<GURL>& redirect_chain) {
    csd_host_->browse_info_->url_redirects = redirect_chain;
  }

  void TestUnsafeResourceCopied(const UnsafeResource& resource) {
    ASSERT_TRUE(csd_host_->unsafe_resource_.get());
    // Test that the resource from OnSafeBrowsingHit notification was copied
    // into the CSDH.
    EXPECT_EQ(resource.url, csd_host_->unsafe_resource_->url);
    EXPECT_EQ(resource.original_url, csd_host_->unsafe_resource_->original_url);
    EXPECT_EQ(resource.is_subresource,
              csd_host_->unsafe_resource_->is_subresource);
    EXPECT_EQ(resource.threat_type, csd_host_->unsafe_resource_->threat_type);
    EXPECT_TRUE(csd_host_->unsafe_resource_->callback.is_null());
    EXPECT_EQ(resource.web_contents_getter.Run(),
              csd_host_->unsafe_resource_->web_contents_getter.Run());
  }

  void SetUnsafeSubResourceForCurrent(bool expect_unsafe_resource) {
    UnsafeResource resource;
    resource.url = GURL("http://www.malware.com/");
    resource.original_url = web_contents()->GetURL();
    resource.is_subresource = true;
    resource.threat_type = SB_THREAT_TYPE_URL_MALWARE;
    resource.callback = base::DoNothing();
    resource.callback_thread = content::GetIOThreadTaskRunner({});
    resource.web_contents_getter = security_interstitials::GetWebContentsGetter(
        web_contents()->GetMainFrame()->GetProcess()->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID());
    csd_host_->OnSafeBrowsingHit(resource);
    resource.callback.Reset();
    ASSERT_EQ(expect_unsafe_resource, csd_host_->DidShowSBInterstitial());
    if (expect_unsafe_resource)
      TestUnsafeResourceCopied(resource);
  }

  void NavigateWithSBHitAndCommit(const GURL& url) {
    // Create a pending navigation.
    controller().LoadURL(
        url, content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());

    ASSERT_TRUE(pending_main_rfh());

    // Simulate a safebrowsing hit before navigation completes.
    UnsafeResource resource;
    resource.url = url;
    resource.original_url = url;
    resource.is_subresource = false;
    resource.threat_type = SB_THREAT_TYPE_URL_MALWARE;
    resource.callback = base::DoNothing();
    resource.callback_thread = content::GetIOThreadTaskRunner({});
    resource.web_contents_getter = security_interstitials::GetWebContentsGetter(
        pending_rvh()->GetProcess()->GetID(),
        pending_main_rfh()->GetRoutingID());
    csd_host_->OnSafeBrowsingHit(resource);
    resource.callback.Reset();

    // LoadURL created a navigation entry, now simulate the RenderView sending
    // a notification that it actually navigated.
    content::WebContentsTester::For(web_contents())->CommitPendingNavigation();

    ASSERT_TRUE(csd_host_->DidShowSBInterstitial());
    TestUnsafeResourceCopied(resource);
  }

  void NavigateWithoutSBHitAndCommit(const GURL& safe_url) {
    controller().LoadURL(
        safe_url, content::Referrer(), ui::PAGE_TRANSITION_LINK,
        std::string());

    ASSERT_TRUE(pending_main_rfh());

    content::WebContentsTester::For(web_contents())->CommitPendingNavigation();
    ASSERT_FALSE(csd_host_->DidShowSBInterstitial());
  }

  void AdvanceTimeTickClock(base::TimeDelta delta) { clock_.Advance(delta); }

 protected:
  std::unique_ptr<ClientSideDetectionHost> csd_host_;
  std::unique_ptr<StrictMock<MockClientSideDetectionService>> csd_service_;
  scoped_refptr<StrictMock<MockSafeBrowsingUIManager> > ui_manager_;
  scoped_refptr<StrictMock<MockSafeBrowsingDatabaseManager> > database_manager_;
  FakePhishingDetector fake_phishing_detector_;
  base::SimpleTestTickClock clock_;
  const bool is_incognito_;
};

class ClientSideDetectionHostTest : public ClientSideDetectionHostTestBase {
 public:
  ClientSideDetectionHostTest()
      : ClientSideDetectionHostTestBase(false /*is_incognito*/) {}
};

class ClientSideDetectionHostIncognitoTest
    : public ClientSideDetectionHostTestBase {
 public:
  ClientSideDetectionHostIncognitoTest()
      : ClientSideDetectionHostTestBase(true /*is_incognito*/) {}
};

TEST_F(ClientSideDetectionHostTest, PhishingDetectionDoneInvalidVerdict) {
  // Case 0: renderer sends an invalid verdict string that we're unable to
  // parse.
  MockBrowserFeatureExtractor* mock_extractor =
      new StrictMock<MockBrowserFeatureExtractor>(web_contents());
  SetFeatureExtractor(mock_extractor);  // The host class takes ownership.
  EXPECT_CALL(*mock_extractor, ExtractFeatures(_, _, _)).Times(0);
  PhishingDetectionDone("Invalid Protocol Buffer");
  EXPECT_TRUE(Mock::VerifyAndClear(mock_extractor));
}

TEST_F(ClientSideDetectionHostTest, PhishingDetectionDoneNotPhishing) {
  // Case 1: client thinks the page is phishing.  The server does not agree.
  // No interstitial is shown.
  MockBrowserFeatureExtractor* mock_extractor =
      new StrictMock<MockBrowserFeatureExtractor>(web_contents());
  SetFeatureExtractor(mock_extractor);  // The host class takes ownership.

  ClientSideDetectionService::ClientReportPhishingRequestCallback cb;
  ClientPhishingRequest verdict;
  verdict.set_url("http://phishingurl.com/");
  verdict.set_client_score(1.0f);
  verdict.set_is_phishing(true);

  EXPECT_CALL(*mock_extractor, ExtractFeatures(_, _, _))
      .WillOnce(Invoke([&](const BrowseInfo* into,
                           std::unique_ptr<ClientPhishingRequest> request,
                           BrowserFeatureExtractor::DoneCallback callback) {
        request->CopyFrom(verdict);
        std::move(callback).Run(true, std::move(request));
      }));

  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(
                                 PartiallyEqualVerdict(verdict), _, _, _))
      .WillOnce(SaveArg<3>(&cb));
  PhishingDetectionDone(verdict.SerializeAsString());
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  ASSERT_FALSE(cb.is_null());

  // Make sure DisplayBlockingPage is not going to be called.
  EXPECT_CALL(*ui_manager_.get(), DisplayBlockingPage(_)).Times(0);
  cb.Run(GURL(verdict.url()), false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Mock::VerifyAndClear(ui_manager_.get()));
}

TEST_F(ClientSideDetectionHostTest, PhishingDetectionDoneDisabled) {
  // Case 2: client thinks the page is phishing and so does the server but
  // showing the interstitial is disabled => no interstitial is shown.
  MockBrowserFeatureExtractor* mock_extractor =
      new StrictMock<MockBrowserFeatureExtractor>(web_contents());
  SetFeatureExtractor(mock_extractor);  // The host class takes ownership.

  ClientSideDetectionService::ClientReportPhishingRequestCallback cb;
  ClientPhishingRequest verdict;
  verdict.set_url("http://phishingurl.com/");
  verdict.set_client_score(1.0f);
  verdict.set_is_phishing(true);

  EXPECT_CALL(*mock_extractor, ExtractFeatures(_, _, _))
      .WillOnce(Invoke([&](const BrowseInfo* info,
                           std::unique_ptr<ClientPhishingRequest> request,
                           BrowserFeatureExtractor::DoneCallback callback) {
        request->CopyFrom(verdict);
        std::move(callback).Run(true, std::move(request));
      }));
  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(
                                 PartiallyEqualVerdict(verdict), _, _, _))
      .WillOnce(SaveArg<3>(&cb));
  PhishingDetectionDone(verdict.SerializeAsString());
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  ASSERT_FALSE(cb.is_null());

  // Make sure DisplayBlockingPage is not going to be called.
  EXPECT_CALL(*ui_manager_.get(), DisplayBlockingPage(_)).Times(0);
  cb.Run(GURL(verdict.url()), false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Mock::VerifyAndClear(ui_manager_.get()));
}

TEST_F(ClientSideDetectionHostTest, PhishingDetectionDoneShowInterstitial) {
  // Case 3: client thinks the page is phishing and so does the server.
  // We show an interstitial.
  MockBrowserFeatureExtractor* mock_extractor =
      new StrictMock<MockBrowserFeatureExtractor>(web_contents());
  SetFeatureExtractor(mock_extractor);  // The host class takes ownership.

  ClientSideDetectionService::ClientReportPhishingRequestCallback cb;
  GURL phishing_url("http://phishingurl.com/");
  ClientPhishingRequest verdict;
  verdict.set_url(phishing_url.spec());
  verdict.set_client_score(1.0f);
  verdict.set_is_phishing(true);

  EXPECT_CALL(*mock_extractor, ExtractFeatures(_, _, _))
      .WillOnce(Invoke([&](const BrowseInfo* info,
                           std::unique_ptr<ClientPhishingRequest> request,
                           BrowserFeatureExtractor::DoneCallback callback) {
        request->CopyFrom(verdict);
        std::move(callback).Run(true, std::move(request));
      }));
  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(
                                 PartiallyEqualVerdict(verdict), _, _, _))
      .WillOnce(SaveArg<3>(&cb));
  PhishingDetectionDone(verdict.SerializeAsString());
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  ASSERT_FALSE(cb.is_null());

  UnsafeResource resource;
  EXPECT_CALL(*ui_manager_.get(), DisplayBlockingPage(_))
      .WillOnce(SaveArg<0>(&resource));
  cb.Run(phishing_url, true);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Mock::VerifyAndClear(ui_manager_.get()));
  EXPECT_EQ(phishing_url, resource.url);
  EXPECT_EQ(phishing_url, resource.original_url);
  EXPECT_FALSE(resource.is_subresource);
  EXPECT_EQ(SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING, resource.threat_type);
  EXPECT_EQ(ThreatSource::CLIENT_SIDE_DETECTION, resource.threat_source);
  EXPECT_EQ(web_contents(), resource.web_contents_getter.Run());

  // Make sure the client object will be deleted.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MockSafeBrowsingUIManager::InvokeOnBlockingPageComplete,
                     ui_manager_, resource.callback));
}

TEST_F(ClientSideDetectionHostTest, PhishingDetectionDoneMultiplePings) {
  // Case 4 & 5: client thinks a page is phishing then navigates to
  // another page which is also considered phishing by the client
  // before the server responds with a verdict.  After a while the
  // server responds for both requests with a phishing verdict.  Only
  // a single interstitial is shown for the second URL.
  MockBrowserFeatureExtractor* mock_extractor =
      new StrictMock<MockBrowserFeatureExtractor>(web_contents());
  SetFeatureExtractor(mock_extractor);  // The host class takes ownership.

  ClientSideDetectionService::ClientReportPhishingRequestCallback cb;
  GURL phishing_url("http://phishingurl.com/");
  ClientPhishingRequest verdict;
  verdict.set_url(phishing_url.spec());
  verdict.set_client_score(1.0f);
  verdict.set_is_phishing(true);

  EXPECT_CALL(*mock_extractor, ExtractFeatures(_, _, _))
      .WillOnce(Invoke([&](const BrowseInfo* info,
                           std::unique_ptr<ClientPhishingRequest> request,
                           BrowserFeatureExtractor::DoneCallback callback) {
        request->CopyFrom(verdict);
        std::move(callback).Run(true, std::move(request));
      }));
  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(
                                 PartiallyEqualVerdict(verdict), _, _, _))
      .WillOnce(SaveArg<3>(&cb));
  PhishingDetectionDone(verdict.SerializeAsString());
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  ASSERT_FALSE(cb.is_null());

  // Set this back to a normal browser feature extractor since we're using
  // NavigateAndCommit() and it's easier to use the real thing than setting up
  // mock expectations.
  SetFeatureExtractor(new BrowserFeatureExtractor(web_contents()));
  GURL other_phishing_url("http://other_phishing_url.com/bla");
  ExpectPreClassificationChecks(other_phishing_url, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse);
  EXPECT_CALL(*csd_service_, GetModelStr()).WillRepeatedly(Return("model_str"));
  // We navigate away.  The callback cb should be revoked.
  NavigateAndCommit(other_phishing_url);
  // Wait for the pre-classification checks to finish for other_phishing_url.
  WaitAndCheckPreClassificationChecks();

  ClientSideDetectionService::ClientReportPhishingRequestCallback cb_other;
  verdict.set_url(other_phishing_url.spec());
  verdict.set_client_score(0.8f);
  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(
                                 PartiallyEqualVerdict(verdict), _, _, _))
      .WillOnce(SaveArg<3>(&cb_other));
  std::vector<GURL> redirect_chain;
  redirect_chain.push_back(other_phishing_url);
  SetRedirectChain(redirect_chain);
  PhishingDetectionDone(verdict.SerializeAsString());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  ASSERT_FALSE(cb_other.is_null());

  // We expect that the interstitial is shown for the second phishing URL and
  // not for the first phishing URL.
  UnsafeResource resource;
  EXPECT_CALL(*ui_manager_.get(), DisplayBlockingPage(_))
      .WillOnce(SaveArg<0>(&resource));

  cb.Run(phishing_url, true);  // Should have no effect.
  cb_other.Run(other_phishing_url, true);  // Should show interstitial.

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Mock::VerifyAndClear(ui_manager_.get()));
  EXPECT_EQ(other_phishing_url, resource.url);
  EXPECT_EQ(other_phishing_url, resource.original_url);
  EXPECT_FALSE(resource.is_subresource);
  EXPECT_EQ(SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING, resource.threat_type);
  EXPECT_EQ(ThreatSource::CLIENT_SIDE_DETECTION, resource.threat_source);
  EXPECT_EQ(web_contents(), resource.web_contents_getter.Run());

  // Make sure the client object will be deleted.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MockSafeBrowsingUIManager::InvokeOnBlockingPageComplete,
                     ui_manager_, resource.callback));
}

TEST_F(ClientSideDetectionHostTest, PhishingDetectionDoneVerdictNotPhishing) {
  // Case 6: renderer sends a verdict string that isn't phishing.
  MockBrowserFeatureExtractor* mock_extractor =
      new StrictMock<MockBrowserFeatureExtractor>(web_contents());
  SetFeatureExtractor(mock_extractor);  // The host class takes ownership.

  ClientPhishingRequest verdict;
  verdict.set_url("http://not-phishing.com/");
  verdict.set_client_score(0.1f);
  verdict.set_is_phishing(false);

  EXPECT_CALL(*mock_extractor, ExtractFeatures(_, _, _)).Times(0);
  PhishingDetectionDone(verdict.SerializeAsString());
  EXPECT_TRUE(Mock::VerifyAndClear(mock_extractor));
}

TEST_F(ClientSideDetectionHostTest,
       PhishingDetectionDoneVerdictNotPhishingButSBMatchSubResource) {
  // Case 7: renderer sends a verdict string that isn't phishing but the URL
  // of a subresource was on the regular phishing or malware lists.
  GURL url("http://not-phishing.com/");
  ClientPhishingRequest verdict;
  verdict.set_url(url.spec());
  verdict.set_client_score(0.1f);
  verdict.set_is_phishing(false);

  // First we have to navigate to the URL to set the unique page ID.
  EXPECT_CALL(*csd_service_, GetModelStr()).WillRepeatedly(Return("model_str"));
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();
  SetUnsafeSubResourceForCurrent(true /* expect_unsafe_resource */);

  std::vector<GURL> redirect_chain;
  redirect_chain.push_back(url);
  SetRedirectChain(redirect_chain);
  PhishingDetectionDone(verdict.SerializeAsString());
}

TEST_F(ClientSideDetectionHostTest,
       PhishingDetectionDoneVerdictNotPhishingButSBMatchOnNewRVH) {
  // When navigating to a different host (thus creating a pending RVH) which
  // matches regular malware list, and after navigation the renderer sends a
  // verdict string that isn't phishing, we should still send the report.

  // Do an initial navigation to a safe host.
  EXPECT_CALL(*csd_service_, GetModelStr()).WillRepeatedly(Return("model_str"));
  GURL start_url("http://safe.example.com/");
  ExpectPreClassificationChecks(start_url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse);
  NavigateAndCommit(start_url);
  WaitAndCheckPreClassificationChecks();

  // Now navigate to a different host which will have a malware hit before the
  // navigation commits.
  GURL url("http://malware-but-not-phishing.com/");
  ClientPhishingRequest verdict;
  verdict.set_url(url.spec());
  verdict.set_client_score(0.1f);
  verdict.set_is_phishing(false);

  EXPECT_CALL(*csd_service_, GetModelStr()).WillRepeatedly(Return("model_str"));
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse);
  NavigateWithSBHitAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  std::vector<GURL> redirect_chain;
  redirect_chain.push_back(url);
  SetRedirectChain(redirect_chain);
  PhishingDetectionDone(verdict.SerializeAsString());
}

TEST_F(
    ClientSideDetectionHostTest,
    PhishingDetectionDoneVerdictNotPhishingButSBMatchOnSubresourceWhileNavPending) {
  // When a malware hit happens on a committed page while a slow pending load is
  // in progress, the csd report should be sent for the committed page.

  // Do an initial navigation to a safe host.
  EXPECT_CALL(*csd_service_, GetModelStr()).WillRepeatedly(Return("model_str"));
  GURL start_url("http://safe.example.com/");
  ExpectPreClassificationChecks(start_url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse);
  NavigateAndCommit(start_url);
  WaitAndCheckPreClassificationChecks();

  // Now navigate to a different host which does not have a SB hit.
  GURL url("http://not-malware-not-phishing-but-malware-subresource.com/");
  ClientPhishingRequest verdict;
  verdict.set_url(url.spec());
  verdict.set_client_score(0.1f);
  verdict.set_is_phishing(false);

  EXPECT_CALL(*csd_service_, GetModelStr()).WillRepeatedly(Return("model_str"));
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse);
  NavigateWithoutSBHitAndCommit(url);

  // Simulate a subresource malware hit on committed page.
  SetUnsafeSubResourceForCurrent(true /* expect_unsafe_resource */);

  // Create a pending navigation, but don't commit it.
  GURL pending_url("http://slow.example.com/");
  auto pending_navigation =
      content::NavigationSimulator::CreateBrowserInitiated(pending_url,
                                                           web_contents());
  pending_navigation->Start();

  WaitAndCheckPreClassificationChecks();

  std::vector<GURL> redirect_chain;
  redirect_chain.push_back(url);
  SetRedirectChain(redirect_chain);
  PhishingDetectionDone(verdict.SerializeAsString());
}

TEST_F(ClientSideDetectionHostTest, SafeBrowsingHitOnFreshTab) {
  // A fresh WebContents should not have any NavigationEntries yet. (See
  // https://crbug.com/524208.)
  EXPECT_EQ(nullptr, controller().GetLastCommittedEntry());
  EXPECT_EQ(nullptr, controller().GetPendingEntry());

  // Simulate a subresource malware hit (this could happen if the WebContents
  // was created with window.open, and had content injected into it).
  SetUnsafeSubResourceForCurrent(false /* expect_unsafe_resource */);

  // If the test didn't crash, we're good. (Nothing else to test, since there
  // was no DidNavigateMainFrame, CSD won't do anything with this hit.)
}

// This test doesn't work because it makes assumption about how
// the message loop is run, and those assumptions are wrong when properly
// simulating a navigation with browser-side navigations.
// TODO(clamy): Fix the test and re-enable. See crbug.com/753357.
TEST_F(ClientSideDetectionHostTest,
       DISABLED_NavigationCancelsShouldClassifyUrl) {
  // Test that canceling pending should classify requests works as expected.
  GURL first_url("http://first.phishy.url.com");
  GURL second_url("http://second.url.com/");
  // The first few checks are done synchronously so check that they have been
  // done for the first URL, while the second URL has all the checks done.  We
  // need to manually set up the IsPrivateIPAddress mock since if the same mock
  // expectation is specified twice, gmock will only use the last instance of
  // it, meaning the first will never be matched.
  EXPECT_CALL(*csd_service_, GetModelStr()).WillRepeatedly(Return("model_str"));
  EXPECT_CALL(*csd_service_, IsPrivateIPAddress(_))
      .WillOnce(Return(false))
      .WillOnce(Return(false));
  ExpectPreClassificationChecks(first_url, nullptr, &kFalse, nullptr, nullptr,
                                nullptr);
  ExpectPreClassificationChecks(second_url, nullptr, &kFalse, &kFalse, &kFalse,
                                &kFalse);

  NavigateAndCommit(first_url);
  // Don't flush the message loop, as we want to navigate to a different
  // url before the final pre-classification checks are run.
  NavigateAndCommit(second_url);
  WaitAndCheckPreClassificationChecks();
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckPass) {
  // Navigate the tab to a page.  We should see a StartPhishingDetection IPC.
  EXPECT_CALL(*csd_service_, GetModelStr()).WillRepeatedly(Return("model_str"));
  GURL url("http://host.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse);
  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(&url);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckMatchWhitelist) {
  EXPECT_CALL(*csd_service_, GetModelStr()).WillRepeatedly(Return("model_str"));
  GURL url("http://host.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kTrue, nullptr, nullptr,
                                nullptr);
  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckSameDocumentNavigation) {
  EXPECT_CALL(*csd_service_, GetModelStr()).WillRepeatedly(Return("model_str"));
  GURL url("http://host.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse);
  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(&url);
  fake_phishing_detector_.Reset();

  // Now try an same-document navigation.  This should not trigger an IPC.
  EXPECT_CALL(*csd_service_, IsPrivateIPAddress(_)).Times(0);
  GURL inpage("http://host.com/#foo");
  ExpectPreClassificationChecks(inpage, nullptr, nullptr, nullptr, nullptr,
                                nullptr);
  NavigateAndKeepLoading(web_contents(), inpage);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(nullptr);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckXHTML) {
  // Check that XHTML is supported, in addition to the default HTML type.
  EXPECT_CALL(*csd_service_, GetModelStr()).WillRepeatedly(Return("model_str"));
  GURL url("http://host.com/xhtml");
  auto navigation =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  navigation->SetContentsMimeType("application/xhtml+xml");
  navigation->SetKeepLoading(true);
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse);
  navigation->Commit();
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(&url);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckTwoNavigations) {
  // Navigate to two hosts, which should cause two IPCs.
  EXPECT_CALL(*csd_service_, GetModelStr()).WillRepeatedly(Return("model_str"));
  GURL url1("http://host1.com/");
  ExpectPreClassificationChecks(url1, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse);
  NavigateAndKeepLoading(web_contents(), url1);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(&url1);

  EXPECT_CALL(*csd_service_, GetModelStr()).WillRepeatedly(Return("model_str"));
  GURL url2("http://host2.com/");
  ExpectPreClassificationChecks(url2, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse);
  NavigateAndKeepLoading(web_contents(), url2);
  // Re-override the binder for PhishingDetector because navigation causes
  // a new web InterfaceProvider to be created
  InitTestApi();
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(&url2);
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckPrivateIpAddress) {
  // If IsPrivateIPAddress returns true, no IPC should be triggered.
  EXPECT_CALL(*csd_service_, GetModelStr()).WillRepeatedly(Return("model_str"));
  GURL url("http://host3.com/");
  ExpectPreClassificationChecks(url, &kTrue, nullptr, nullptr, nullptr,
                                nullptr);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(nullptr);
}

TEST_F(ClientSideDetectionHostIncognitoTest,
       TestPreClassificationCheckIncognito) {
  // If the tab is incognito there should be no IPC.  Also, we shouldn't
  // even check the csd-whitelist.
  EXPECT_CALL(*csd_service_, GetModelStr()).WillRepeatedly(Return("model_str"));
  GURL url("http://host4.com/");
  ExpectPreClassificationChecks(url, &kFalse, nullptr, nullptr, nullptr,
                                nullptr);

  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(nullptr);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckInvalidCache) {
  // If item is in the cache but it isn't valid, we will classify regardless
  // of whether we are over the reporting limit.
  EXPECT_CALL(*csd_service_, GetModelStr()).WillRepeatedly(Return("model_str"));
  GURL url("http://host6.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kTrue,
                                nullptr);

  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(&url);
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckOverPhishingReportingLimit) {
  // If the url isn't in the cache and we are over the reporting limit, we
  // don't do classification.
  EXPECT_CALL(*csd_service_, GetModelStr()).WillRepeatedly(Return("model_str"));
  GURL url("http://host7.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kTrue);
  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(nullptr);
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckOverBothReportingLimits) {
  EXPECT_CALL(*csd_service_, GetModelStr()).WillRepeatedly(Return("model_str"));
  GURL url("http://host.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kTrue);
  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(nullptr);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckHttpsUrl) {
  EXPECT_CALL(*csd_service_, GetModelStr()).WillRepeatedly(Return("model_str"));
  GURL url("https://host.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse);
  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(&url);
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckNoneHttpOrHttpsUrl) {
  EXPECT_CALL(*csd_service_, GetModelStr()).WillRepeatedly(Return("model_str"));
  GURL url("file://host.com/");
  ExpectPreClassificationChecks(url, &kFalse, nullptr, nullptr, nullptr,
                                nullptr);
  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(nullptr);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckValidCached) {
  // If result is cached, we will try and display the blocking page directly
  // with no start classification message.
  EXPECT_CALL(*csd_service_, GetModelStr()).WillRepeatedly(Return("model_str"));
  GURL url("http://host8.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kTrue, &kFalse,
                                &kFalse);

  UnsafeResource resource;
  EXPECT_CALL(*ui_manager_.get(), DisplayBlockingPage(_))
      .WillOnce(SaveArg<0>(&resource));

  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();
  EXPECT_EQ(url, resource.url);
  EXPECT_EQ(url, resource.original_url);

  fake_phishing_detector_.CheckMessage(nullptr);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationWhitelistedByPolicy) {
  // Configures enterprise whitelist.
  ListPrefUpdate update(profile()->GetPrefs(),
                        prefs::kSafeBrowsingWhitelistDomains);
  update->AppendString("example.com");

  EXPECT_CALL(*csd_service_, GetModelStr()).WillRepeatedly(Return("model_str"));
  GURL url("http://example.com/");
  ExpectPreClassificationChecks(url, &kFalse, nullptr, nullptr, nullptr,
                                nullptr);

  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(nullptr);
}

TEST_F(ClientSideDetectionHostTest, RecordsPhishingDetectorResults) {
  MockBrowserFeatureExtractor* mock_extractor =
      new StrictMock<MockBrowserFeatureExtractor>(web_contents());
  SetFeatureExtractor(mock_extractor);  // The host class takes ownership.

  {
    ClientPhishingRequest verdict;
    verdict.set_url("http://not-phishing.com/");
    verdict.set_client_score(0.1f);
    verdict.set_is_phishing(false);

    base::HistogramTester histogram_tester;

    EXPECT_CALL(*mock_extractor, ExtractFeatures(_, _, _)).Times(0);
    PhishingDetectionDone(verdict.SerializeAsString());
    EXPECT_TRUE(Mock::VerifyAndClear(mock_extractor));

    histogram_tester.ExpectUniqueSample(
        "SBClientPhishing.PhishingDetectorResult",
        mojom::PhishingDetectorResult::SUCCESS, 1);
  }

  {
    base::HistogramTester histogram_tester;

    EXPECT_CALL(*mock_extractor, ExtractFeatures(_, _, _)).Times(0);
    PhishingDetectionError(mojom::PhishingDetectorResult::CLASSIFIER_NOT_READY);
    EXPECT_TRUE(Mock::VerifyAndClear(mock_extractor));

    histogram_tester.ExpectUniqueSample(
        "SBClientPhishing.PhishingDetectorResult",
        mojom::PhishingDetectorResult::CLASSIFIER_NOT_READY, 1);
  }

  {
    base::HistogramTester histogram_tester;

    EXPECT_CALL(*mock_extractor, ExtractFeatures(_, _, _)).Times(0);
    PhishingDetectionError(
        mojom::PhishingDetectorResult::FORWARD_BACK_TRANSITION);
    EXPECT_TRUE(Mock::VerifyAndClear(mock_extractor));

    histogram_tester.ExpectUniqueSample(
        "SBClientPhishing.PhishingDetectorResult",
        mojom::PhishingDetectorResult::FORWARD_BACK_TRANSITION, 1);
  }
}

TEST_F(ClientSideDetectionHostTest, RecordsPhishingDetectionDuration) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PhishingDetectionDuration", 0);

  EXPECT_CALL(*csd_service_, GetModelStr()).WillRepeatedly(Return("model_str"));
  GURL start_url("http://safe.example.com/");
  ExpectPreClassificationChecks(start_url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse);
  NavigateAndCommit(start_url);
  WaitAndCheckPreClassificationChecks();
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PhishingDetectionDuration", 1);

  GURL url("http://phishing.example.com/");
  ClientPhishingRequest verdict;
  verdict.set_url(url.spec());
  verdict.set_client_score(0.1f);
  verdict.set_is_phishing(false);

  EXPECT_CALL(*csd_service_, GetModelStr()).WillRepeatedly(Return("model_str"));
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();
  const base::TimeDelta duration = base::TimeDelta::FromMilliseconds(10);
  AdvanceTimeTickClock(duration);

  std::vector<GURL> redirect_chain;
  redirect_chain.push_back(url);
  SetRedirectChain(redirect_chain);
  PhishingDetectionDone(verdict.SerializeAsString());

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PhishingDetectionDuration", 3);
  EXPECT_LE(duration.InMilliseconds(),
            histogram_tester
                .GetAllSamples("SBClientPhishing.PhishingDetectionDuration")
                .front()
                .min);
}

TEST_F(ClientSideDetectionHostTest, TestSendModelToRenderFrame) {
  StrictMock<MockModelLoader> loader;
  loader.SetModelStrForTesting("standard");
  EXPECT_CALL(*csd_service_, GetModelStr()).WillRepeatedly(Return("standard"));
  csd_host_->SendModelToRenderFrame();
  base::RunLoop().RunUntilIdle();
  fake_phishing_detector_.CheckModel("standard");
  fake_phishing_detector_.Reset();
}

TEST_F(ClientSideDetectionHostTest, ClearsScreenshotData) {
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled,
                                    false);

  MockBrowserFeatureExtractor* mock_extractor =
      new StrictMock<MockBrowserFeatureExtractor>(web_contents());
  SetFeatureExtractor(mock_extractor);  // The host class takes ownership.

  ClientPhishingRequest verdict;
  verdict.set_url("http://phishingurl.com/");
  verdict.set_client_score(1.0f);
  verdict.set_is_phishing(true);
  verdict.set_screenshot_digest("screenshot_digest");
  verdict.set_screenshot_phash("screenshot_phash");
  verdict.set_phash_dimension_size(48);

  ClientPhishingRequest request;

  EXPECT_CALL(*mock_extractor, ExtractFeatures(_, _, _))
      .WillOnce(Invoke([&](const BrowseInfo* into,
                           std::unique_ptr<ClientPhishingRequest> request,
                           BrowserFeatureExtractor::DoneCallback callback) {
        std::move(callback).Run(true, std::move(request));
      }));
  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(_, _, _, _))
      .WillOnce(testing::SaveArgPointee<0>(&request));
  PhishingDetectionDone(verdict.SerializeAsString());
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  EXPECT_FALSE(request.has_phash_dimension_size());
  EXPECT_FALSE(request.has_screenshot_phash());
  EXPECT_FALSE(request.has_screenshot_digest());
}

TEST_F(ClientSideDetectionHostTest, AllowsScreenshotDataForSBER) {
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled,
                                    true);

  MockBrowserFeatureExtractor* mock_extractor =
      new StrictMock<MockBrowserFeatureExtractor>(web_contents());
  SetFeatureExtractor(mock_extractor);  // The host class takes ownership.

  ClientPhishingRequest verdict;
  verdict.set_url("http://phishingurl.com/");
  verdict.set_client_score(1.0f);
  verdict.set_is_phishing(true);
  verdict.set_screenshot_digest("screenshot_digest");
  verdict.set_screenshot_phash("screenshot_phash");
  verdict.set_phash_dimension_size(48);

  ClientPhishingRequest request;

  EXPECT_CALL(*mock_extractor, ExtractFeatures(_, _, _))
      .WillOnce(Invoke([&](const BrowseInfo* into,
                           std::unique_ptr<ClientPhishingRequest> request,
                           BrowserFeatureExtractor::DoneCallback callback) {
        std::move(callback).Run(true, std::move(request));
      }));
  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(_, _, _, _))
      .WillOnce(testing::SaveArgPointee<0>(&request));
  PhishingDetectionDone(verdict.SerializeAsString());
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  EXPECT_TRUE(request.has_phash_dimension_size());
  EXPECT_TRUE(request.has_screenshot_phash());
  EXPECT_TRUE(request.has_screenshot_digest());
}

}  // namespace safe_browsing
