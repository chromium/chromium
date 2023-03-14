// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/safe_browsing/chrome_client_side_detection_host_delegate.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/safe_browsing/chrome_safe_browsing_blocking_page_factory.h"
#include "chrome/browser/safe_browsing/chrome_ui_manager_delegate.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/content/browser/client_side_detection_service.h"
#include "components/safe_browsing/content/browser/client_side_phishing_model.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom-shared.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"
#include "components/safe_browsing/core/browser/sync/sync_utils.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/unsafe_resource_util.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "ipc/ipc_test_sink.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
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
using ::testing::ReturnRef;
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

class MockSafeBrowsingTokenFetcher : public SafeBrowsingTokenFetcher {
 public:
  MockSafeBrowsingTokenFetcher() = default;

  MockSafeBrowsingTokenFetcher(const MockSafeBrowsingTokenFetcher&) = delete;
  MockSafeBrowsingTokenFetcher& operator=(const MockSafeBrowsingTokenFetcher&) =
      delete;

  ~MockSafeBrowsingTokenFetcher() override = default;

  MOCK_METHOD1(Start, void(Callback));
  MOCK_METHOD1(OnInvalidAccessToken, void(const std::string&));
};

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

class MockClientSideDetectionService : public ClientSideDetectionService {
 public:
  MockClientSideDetectionService()
      : ClientSideDetectionService(nullptr, nullptr, nullptr) {}

  MockClientSideDetectionService(const MockClientSideDetectionService&) =
      delete;
  MockClientSideDetectionService& operator=(
      const MockClientSideDetectionService&) = delete;

  ~MockClientSideDetectionService() override {}

  MOCK_METHOD3(SendClientReportPhishingRequest,
               void(std::unique_ptr<ClientPhishingRequest>,
                    ClientReportPhishingRequestCallback,
                    const std::string&));
  MOCK_CONST_METHOD1(IsPrivateIPAddress, bool(const net::IPAddress&));
  MOCK_CONST_METHOD1(IsLocalResource, bool(const net::IPAddress&));
  MOCK_METHOD2(GetValidCachedResult, bool(const GURL&, bool*));
  MOCK_METHOD1(IsInCache, bool(const GURL&));
  MOCK_METHOD0(OverPhishingReportLimit, bool());
  MOCK_METHOD0(GetModelStr, std::string&());
  MOCK_METHOD0(GetModelSharedMemoryRegion, base::ReadOnlySharedMemoryRegion());
  MOCK_METHOD0(GetModelType, CSDModelType());
};

class MockSafeBrowsingUIManager : public SafeBrowsingUIManager {
 public:
  MockSafeBrowsingUIManager()
      : SafeBrowsingUIManager(
            std::make_unique<ChromeSafeBrowsingUIManagerDelegate>(),
            std::make_unique<ChromeSafeBrowsingBlockingPageFactory>(),
            GURL(chrome::kChromeUINewTabURL)) {}

  MockSafeBrowsingUIManager(const MockSafeBrowsingUIManager&) = delete;
  MockSafeBrowsingUIManager& operator=(const MockSafeBrowsingUIManager&) =
      delete;

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
};

class MockSafeBrowsingDatabaseManager : public TestSafeBrowsingDatabaseManager {
 public:
  MockSafeBrowsingDatabaseManager()
      : safe_browsing::TestSafeBrowsingDatabaseManager(
            content::GetUIThreadTaskRunner({}),
            content::GetIOThreadTaskRunner({})) {}

  MockSafeBrowsingDatabaseManager(const MockSafeBrowsingDatabaseManager&) =
      delete;
  MockSafeBrowsingDatabaseManager& operator=(
      const MockSafeBrowsingDatabaseManager&) = delete;

  MOCK_METHOD2(CheckCsdAllowlistUrl,
               AsyncMatch(const GURL&, SafeBrowsingDatabaseManager::Client*));

 protected:
  ~MockSafeBrowsingDatabaseManager() override {}
};

}  // namespace

class FakePhishingDetector : public mojom::PhishingDetector {
 public:
  FakePhishingDetector() = default;

  FakePhishingDetector(const FakePhishingDetector&) = delete;
  FakePhishingDetector& operator=(const FakePhishingDetector&) = delete;

  ~FakePhishingDetector() override = default;

  void BindReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receivers_.Add(this,
                   mojo::PendingAssociatedReceiver<mojom::PhishingDetector>(
                       std::move(handle)));
  }

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

  void Reset() {
    phishing_detection_started_ = false;
    url_ = GURL();
  }

 private:
  mojo::AssociatedReceiverSet<mojom::PhishingDetector> receivers_;
  bool phishing_detection_started_ = false;
  GURL url_;
};

class ClientSideDetectionHostTestBase : public ChromeRenderViewHostTestHarness {
 public:
  typedef security_interstitials::UnsafeResource UnsafeResource;

  class WebContentsObserver : public content::WebContentsObserver {
   public:
    WebContentsObserver(ClientSideDetectionHostTestBase* harness,
                        content::WebContents* contents)
        : content::WebContentsObserver(contents), harness_(harness) {}

    void RenderFrameCreated(
        content::RenderFrameHost* render_frame_host) override {
      harness_->InitTestApi(render_frame_host);
    }

   private:
    // The raw pointer is safe because `harness_` owns this.
    raw_ptr<ClientSideDetectionHostTestBase> harness_;
  };

  explicit ClientSideDetectionHostTestBase(bool is_incognito)
      : is_incognito_(is_incognito), model_str_("model_str") {}

  void InitTestApi(content::RenderFrameHost* rfh) {
    rfh->GetRemoteAssociatedInterfaces()->OverrideBinderForTesting(
        mojom::PhishingDetector::Name_,
        base::BindRepeating(&FakePhishingDetector::BindReceiver,
                            base::Unretained(&fake_phishing_detector_)));
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    observer_ = std::make_unique<WebContentsObserver>(this, web_contents());

    if (is_incognito_) {
      auto incognito_web_contents =
          content::WebContentsTester::CreateTestWebContents(
              profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
              nullptr);
      SetContents(std::move(incognito_web_contents));
    }

    // Initiate the connection to a (pretend) renderer process.
    NavigateAndCommit(GURL("about:blank"));

    InitTestApi(web_contents()->GetPrimaryMainFrame());

    // Inject service classes.
    csd_service_ = std::make_unique<MockClientSideDetectionService>();
    database_manager_ = new StrictMock<MockSafeBrowsingDatabaseManager>();
    ui_manager_ = new StrictMock<MockSafeBrowsingUIManager>();

    identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                   signin::ConsentLevel::kSync);

    csd_host_ =
        ChromeClientSideDetectionHostDelegate::CreateHost(web_contents());
    csd_host_->set_client_side_detection_service(csd_service_->GetWeakPtr());
    csd_host_->set_ui_manager(ui_manager_.get());
    csd_host_->set_database_manager(database_manager_.get());
    csd_host_->set_tick_clock_for_testing(&clock_);
    csd_host_->set_is_off_the_record_for_testing(is_incognito_);
    csd_host_->set_account_signed_in_for_testing(
        base::BindRepeating(&safe_browsing::SyncUtils::IsPrimaryAccountSignedIn,
                            identity_test_env_.identity_manager()));
    auto token_fetcher =
        std::make_unique<StrictMock<MockSafeBrowsingTokenFetcher>>();
    raw_token_fetcher_ = token_fetcher.get();
    csd_host_->set_token_fetcher_for_testing(std::move(token_fetcher));

    testing::DefaultValue<CSDModelType>::Set(CSDModelType::kProtobuf);
  }

  void TearDown() override {
    // Delete the host object on the UI thread and release the
    // SafeBrowsingService.
    content::GetUIThreadTaskRunner({})->DeleteSoon(FROM_HERE,
                                                   csd_host_.release());

    // RenderProcessHostCreationObserver expects to be torn down on UI.
    content::GetUIThreadTaskRunner({})->DeleteSoon(FROM_HERE,
                                                   csd_service_.release());
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
                                     const bool* match_csd_allowlist,
                                     const bool* get_valid_cached_result,
                                     const bool* is_in_cache,
                                     const bool* over_phishing_report_limit,
                                     const bool* is_local) {
    if (is_private) {
      EXPECT_CALL(*csd_service_, IsPrivateIPAddress(_))
          .WillOnce(Return(*is_private));
    }
    if (match_csd_allowlist) {
      EXPECT_CALL(*database_manager_.get(), CheckCsdAllowlistUrl(url, _))
          .WillOnce(Return(*match_csd_allowlist ? AsyncMatch::MATCH
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
    if (is_local) {
      EXPECT_CALL(*csd_service_, IsLocalResource(_))
          .WillOnce(Return(*is_local));
    }
  }

  void WaitAndCheckPreClassificationChecks() {
    // Wait for CheckCsdAllowlist and CheckCache() to be called if at all.
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
    EXPECT_TRUE(Mock::VerifyAndClear(ui_manager_.get()));
    EXPECT_TRUE(Mock::VerifyAndClear(database_manager_.get()));
  }

  void NavigateAndCommit(const GURL& safe_url) {
    controller().LoadURL(
        safe_url, content::Referrer(), ui::PAGE_TRANSITION_LINK,
        std::string());

    content::WebContentsTester::For(web_contents())->CommitPendingNavigation();
  }

  void AdvanceTimeTickClock(base::TimeDelta delta) { clock_.Advance(delta); }

  void SetFeatures(
      const std::vector<base::test::FeatureRef>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features) {
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 protected:
  std::unique_ptr<ClientSideDetectionHost> csd_host_;
  std::unique_ptr<MockClientSideDetectionService> csd_service_;
  scoped_refptr<StrictMock<MockSafeBrowsingUIManager> > ui_manager_;
  scoped_refptr<StrictMock<MockSafeBrowsingDatabaseManager>> database_manager_;
  FakePhishingDetector fake_phishing_detector_;
  raw_ptr<StrictMock<MockSafeBrowsingTokenFetcher>> raw_token_fetcher_ =
      nullptr;
  base::SimpleTestTickClock clock_;
  const bool is_incognito_;
  signin::IdentityTestEnvironment identity_test_env_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<WebContentsObserver> observer_;
  std::string model_str_;
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

class ClientSideDetectionHostPolicyTest
    : public ClientSideDetectionHostTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  ClientSideDetectionHostPolicyTest()
      : ClientSideDetectionHostTestBase(false /*is_incognito*/) {}

  // Verify that client side phishing protection follows policy value.
  void TestPreClassificationCheckCsdPhishingPolicy() {
    // Configures enterprise policy
    profile()->GetPrefs()->SetBoolean(
        prefs::kSafeBrowsingCsdPhishingProtectionAllowedByPolicy, GetParam());

    EXPECT_CALL(*csd_service_, GetModelStr())
        .WillRepeatedly(ReturnRef(model_str_));
    GURL url("http://host.com/");
    if (GetParam()) {
      ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                    &kFalse, &kFalse);
    } else {
      ExpectPreClassificationChecks(url, &kFalse, nullptr, nullptr, nullptr,
                                    nullptr, &kFalse);
    }
    NavigateAndKeepLoading(web_contents(), url);
    WaitAndCheckPreClassificationChecks();
    if (GetParam()) {
      fake_phishing_detector_.CheckMessage(&url);
    } else {
      fake_phishing_detector_.CheckMessage(nullptr);
    }
  }
};

TEST_F(ClientSideDetectionHostTest, PhishingDetectionDoneInvalidVerdict) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // Case 0: renderer sends an invalid verdict string that we're unable to
  // parse.
  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(_, _, _)).Times(0);
  PhishingDetectionDone("Invalid Protocol Buffer");
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
}

TEST_F(ClientSideDetectionHostTest, PhishingDetectionDoneNotPhishing) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // Case 1: client thinks the page is phishing.  The server does not agree.
  // No interstitial is shown.
  ClientSideDetectionService::ClientReportPhishingRequestCallback cb;
  ClientPhishingRequest verdict;
  verdict.set_url("http://phishingurl.com/");
  verdict.set_client_score(1.0f);
  verdict.set_is_phishing(true);

  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(
                                 PartiallyEqualVerdict(verdict), _, _))
      .WillOnce(MoveArg<1>(&cb));
  PhishingDetectionDone(verdict.SerializeAsString());
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  ASSERT_FALSE(cb.is_null());

  // Make sure DisplayBlockingPage is not going to be called.
  EXPECT_CALL(*ui_manager_.get(), DisplayBlockingPage(_)).Times(0);
  std::move(cb).Run(GURL(verdict.url()), false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Mock::VerifyAndClear(ui_manager_.get()));
}

TEST_F(ClientSideDetectionHostTest, PhishingDetectionDoneDisabled) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // Case 2: client thinks the page is phishing and so does the server but
  // showing the interstitial is disabled => no interstitial is shown.
  ClientSideDetectionService::ClientReportPhishingRequestCallback cb;
  ClientPhishingRequest verdict;
  verdict.set_url("http://phishingurl.com/");
  verdict.set_client_score(1.0f);
  verdict.set_is_phishing(true);

  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(
                                 PartiallyEqualVerdict(verdict), _, _))
      .WillOnce(MoveArg<1>(&cb));
  PhishingDetectionDone(verdict.SerializeAsString());
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  ASSERT_FALSE(cb.is_null());

  // Make sure DisplayBlockingPage is not going to be called.
  EXPECT_CALL(*ui_manager_.get(), DisplayBlockingPage(_)).Times(0);
  std::move(cb).Run(GURL(verdict.url()), false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Mock::VerifyAndClear(ui_manager_.get()));
}

TEST_F(ClientSideDetectionHostTest, PhishingDetectionDoneShowInterstitial) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // Case 3: client thinks the page is phishing and so does the server.
  // We show an interstitial.
  ClientSideDetectionService::ClientReportPhishingRequestCallback cb;
  GURL phishing_url("http://phishingurl.com/");
  ClientPhishingRequest verdict;
  verdict.set_url(phishing_url.spec());
  verdict.set_client_score(1.0f);
  verdict.set_is_phishing(true);

  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(
                                 PartiallyEqualVerdict(verdict), _, _))
      .WillOnce(MoveArg<1>(&cb));
  PhishingDetectionDone(verdict.SerializeAsString());
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  ASSERT_FALSE(cb.is_null());

  UnsafeResource resource;
  EXPECT_CALL(*ui_manager_.get(), DisplayBlockingPage(_))
      .WillOnce(SaveArg<0>(&resource));
  std::move(cb).Run(phishing_url, true);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Mock::VerifyAndClear(ui_manager_.get()));
  EXPECT_EQ(phishing_url, resource.url);
  EXPECT_EQ(phishing_url, resource.original_url);
  EXPECT_FALSE(resource.is_subresource);
  EXPECT_EQ(SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING, resource.threat_type);
  EXPECT_EQ(ThreatSource::CLIENT_SIDE_DETECTION, resource.threat_source);
  EXPECT_EQ(web_contents(),
            security_interstitials::GetWebContentsForResource(resource));

  // Make sure the client object will be deleted.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MockSafeBrowsingUIManager::InvokeOnBlockingPageComplete,
                     ui_manager_, resource.callback));
}

TEST_F(ClientSideDetectionHostTest, PhishingDetectionDoneMultiplePings) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // Case 4 & 5: client thinks a page is phishing then navigates to
  // another page which is also considered phishing by the client
  // before the server responds with a verdict.  After a while the
  // server responds for both requests with a phishing verdict.  Only
  // a single interstitial is shown for the second URL.
  ClientSideDetectionService::ClientReportPhishingRequestCallback cb;
  GURL phishing_url("http://phishingurl.com/");
  ClientPhishingRequest verdict;
  verdict.set_url(phishing_url.spec());
  verdict.set_client_score(1.0f);
  verdict.set_is_phishing(true);

  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(
                                 PartiallyEqualVerdict(verdict), _, _))
      .WillOnce(MoveArg<1>(&cb));
  PhishingDetectionDone(verdict.SerializeAsString());
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  ASSERT_FALSE(cb.is_null());

  GURL other_phishing_url("http://other_phishing_url.com/bla");
  ExpectPreClassificationChecks(other_phishing_url, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse, &kFalse);
  EXPECT_CALL(*csd_service_, GetModelStr())
      .WillRepeatedly(ReturnRef(model_str_));
  // We navigate away.  The callback cb should be revoked.
  NavigateAndCommit(other_phishing_url);
  // Wait for the pre-classification checks to finish for other_phishing_url.
  WaitAndCheckPreClassificationChecks();

  ClientSideDetectionService::ClientReportPhishingRequestCallback cb_other;
  verdict.set_url(other_phishing_url.spec());
  verdict.set_client_score(0.8f);
  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(
                                 PartiallyEqualVerdict(verdict), _, _))
      .WillOnce(MoveArg<1>(&cb_other));
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

  std::move(cb).Run(phishing_url, true);  // Should have no effect.
  std::move(cb_other).Run(other_phishing_url,
                          true);  // Should show interstitial.

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Mock::VerifyAndClear(ui_manager_.get()));
  EXPECT_EQ(other_phishing_url, resource.url);
  EXPECT_EQ(other_phishing_url, resource.original_url);
  EXPECT_FALSE(resource.is_subresource);
  EXPECT_EQ(SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING, resource.threat_type);
  EXPECT_EQ(ThreatSource::CLIENT_SIDE_DETECTION, resource.threat_source);
  EXPECT_EQ(web_contents(),
            security_interstitials::GetWebContentsForResource(resource));

  // Make sure the client object will be deleted.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MockSafeBrowsingUIManager::InvokeOnBlockingPageComplete,
                     ui_manager_, resource.callback));
}

TEST_F(ClientSideDetectionHostTest, PhishingDetectionDoneVerdictNotPhishing) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // Case 6: renderer sends a verdict string that isn't phishing.
  ClientPhishingRequest verdict;
  verdict.set_url("http://not-phishing.com/");
  verdict.set_client_score(0.1f);
  verdict.set_is_phishing(false);

  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(_, _, _)).Times(0);
  PhishingDetectionDone(verdict.SerializeAsString());
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
}

TEST_F(ClientSideDetectionHostTest,
       PhishingDetectionDoneVerdictNotPhishingButSBMatchSubResource) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // Case 7: renderer sends a verdict string that isn't phishing but the URL
  // of a subresource was on the regular phishing or malware lists.
  GURL url("http://not-phishing.com/");
  ClientPhishingRequest verdict;
  verdict.set_url(url.spec());
  verdict.set_client_score(0.1f);
  verdict.set_is_phishing(false);

  // First we have to navigate to the URL to set the unique page ID.
  EXPECT_CALL(*csd_service_, GetModelStr())
      .WillRepeatedly(ReturnRef(model_str_));
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  PhishingDetectionDone(verdict.SerializeAsString());
}

TEST_F(ClientSideDetectionHostTest,
       PhishingDetectionDoneVerdictNotPhishingButSBMatchOnNewRVH) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // When navigating to a different host (thus creating a pending RVH) which
  // matches regular malware list, and after navigation the renderer sends a
  // verdict string that isn't phishing, we should still send the report.

  // Do an initial navigation to a safe host.
  EXPECT_CALL(*csd_service_, GetModelStr())
      .WillRepeatedly(ReturnRef(model_str_));
  GURL start_url("http://safe.example.com/");
  ExpectPreClassificationChecks(start_url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse);
  NavigateAndCommit(start_url);
  WaitAndCheckPreClassificationChecks();

  // Now navigate to a different host which will have a malware hit before the
  // navigation commits.
  GURL url("http://malware-but-not-phishing.com/");
  ClientPhishingRequest verdict;
  verdict.set_url(url.spec());
  verdict.set_client_score(0.1f);
  verdict.set_is_phishing(false);

  EXPECT_CALL(*csd_service_, GetModelStr())
      .WillRepeatedly(ReturnRef(model_str_));
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  PhishingDetectionDone(verdict.SerializeAsString());
}

TEST_F(
    ClientSideDetectionHostTest,
    PhishingDetectionDoneVerdictNotPhishingButSBMatchOnSubresourceWhileNavPending) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // When a malware hit happens on a committed page while a slow pending load is
  // in progress, the csd report should be sent for the committed page.

  // Do an initial navigation to a safe host.
  EXPECT_CALL(*csd_service_, GetModelStr())
      .WillRepeatedly(ReturnRef(model_str_));
  GURL start_url("http://safe.example.com/");
  ExpectPreClassificationChecks(start_url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse);
  NavigateAndCommit(start_url);
  WaitAndCheckPreClassificationChecks();

  // Now navigate to a different host which does not have a SB hit.
  GURL url("http://not-malware-not-phishing-but-malware-subresource.com/");
  ClientPhishingRequest verdict;
  verdict.set_url(url.spec());
  verdict.set_client_score(0.1f);
  verdict.set_is_phishing(false);

  EXPECT_CALL(*csd_service_, GetModelStr())
      .WillRepeatedly(ReturnRef(model_str_));
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse);
  NavigateAndCommit(url);

  // Create a pending navigation, but don't commit it.
  GURL pending_url("http://slow.example.com/");
  auto pending_navigation =
      content::NavigationSimulator::CreateBrowserInitiated(pending_url,
                                                           web_contents());
  pending_navigation->Start();

  WaitAndCheckPreClassificationChecks();

  PhishingDetectionDone(verdict.SerializeAsString());
}

TEST_F(ClientSideDetectionHostTest,
       PhishingDetectionDoneEnhancedProtectionShouldHaveToken) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  ClientPhishingRequest verdict;
  verdict.set_url("http://example.com/");
  verdict.set_client_score(1.0f);
  verdict.set_is_phishing(true);

  // Set up mock call to csd service.
  EXPECT_CALL(*csd_service_,
              SendClientReportPhishingRequest(PartiallyEqualVerdict(verdict), _,
                                              "fake_access_token"));

  // Set up mock call to token fetcher.
  SafeBrowsingTokenFetcher::Callback cb;
  EXPECT_CALL(*raw_token_fetcher_, Start(_)).WillOnce(MoveArg<0>(&cb));

  // Make the call.
  PhishingDetectionDone(verdict.SerializeAsString());

  // Wait for token fetcher to be called.
  EXPECT_TRUE(Mock::VerifyAndClear(raw_token_fetcher_));

  ASSERT_FALSE(cb.is_null());
  std::move(cb).Run("fake_access_token");
}

TEST_F(ClientSideDetectionHostTest,
       PhishingDetectionDoneCalledTwiceShouldSucceed) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  ClientPhishingRequest verdict;
  verdict.set_url("http://example.com/");
  verdict.set_client_score(1.0f);
  verdict.set_is_phishing(true);

  // Set up mock call to csd service.
  EXPECT_CALL(*csd_service_,
              SendClientReportPhishingRequest(PartiallyEqualVerdict(verdict), _,
                                              "fake_access_token_1"))
      .Times(1);

  // Set up mock call to csd service.
  EXPECT_CALL(*csd_service_,
              SendClientReportPhishingRequest(PartiallyEqualVerdict(verdict), _,
                                              "fake_access_token_2"))
      .Times(1);

  // Set up mock call to token fetcher.
  SafeBrowsingTokenFetcher::Callback cb;
  EXPECT_CALL(*raw_token_fetcher_, Start(_))
      .Times(1)
      .WillRepeatedly(MoveArg<0>(&cb));

  // Make the call.
  PhishingDetectionDone(verdict.SerializeAsString());

  // Wait for token fetcher to be called.
  EXPECT_TRUE(Mock::VerifyAndClear(raw_token_fetcher_));

  ASSERT_FALSE(cb.is_null());
  std::move(cb).Run("fake_access_token_1");

  // Make the call again.
  EXPECT_CALL(*raw_token_fetcher_, Start(_))
      .Times(1)
      .WillRepeatedly(MoveArg<0>(&cb));
  PhishingDetectionDone(verdict.SerializeAsString());
  EXPECT_TRUE(Mock::VerifyAndClear(raw_token_fetcher_));
  ASSERT_FALSE(cb.is_null());
  std::move(cb).Run("fake_access_token_2");
}

TEST_F(ClientSideDetectionHostIncognitoTest,
       PhishingDetectionDoneIncognitoShouldNotHaveToken) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  ClientPhishingRequest verdict;
  verdict.set_url("http://example.com/");
  verdict.set_client_score(1.0f);
  verdict.set_is_phishing(true);

  // Set up mock call to csd service.
  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(
                                 PartiallyEqualVerdict(verdict), _, ""));

  // Set up mock call to token fetcher.
  SafeBrowsingTokenFetcher::Callback cb;
  EXPECT_CALL(*raw_token_fetcher_, Start(_)).Times(0);

  // Make the call.
  PhishingDetectionDone(verdict.SerializeAsString());
}

TEST_F(ClientSideDetectionHostTest,
       PhishingDetectionDoneNoEnhancedProtectionShouldNotHaveToken) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  ClientPhishingRequest verdict;
  verdict.set_url("http://example.com/");
  verdict.set_client_score(1.0f);
  verdict.set_is_phishing(true);

  // Set up mock call to csd service.
  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(
                                 PartiallyEqualVerdict(verdict), _, ""));

  // Set up mock call to token fetcher.
  SafeBrowsingTokenFetcher::Callback cb;
  EXPECT_CALL(*raw_token_fetcher_, Start(_)).Times(0);

  // Make the call.
  PhishingDetectionDone(verdict.SerializeAsString());
}

// This test doesn't work because it makes assumption about how
// the message loop is run, and those assumptions are wrong when properly
// simulating a navigation with browser-side navigations.
// TODO(clamy): Fix the test and re-enable. See crbug.com/753357.
TEST_F(ClientSideDetectionHostTest,
       DISABLED_NavigationCancelsShouldClassifyUrl) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // Test that canceling pending should classify requests works as expected.
  GURL first_url("http://first.phishy.url.com");
  GURL second_url("http://second.url.com/");
  // The first few checks are done synchronously so check that they have been
  // done for the first URL, while the second URL has all the checks done.  We
  // need to manually set up the IsPrivateIPAddress mock since if the same mock
  // expectation is specified twice, gmock will only use the last instance of
  // it, meaning the first will never be matched.
  EXPECT_CALL(*csd_service_, GetModelStr())
      .WillRepeatedly(ReturnRef(model_str_));
  EXPECT_CALL(*csd_service_, IsPrivateIPAddress(_))
      .WillOnce(Return(false))
      .WillOnce(Return(false));
  ExpectPreClassificationChecks(first_url, nullptr, &kFalse, nullptr, nullptr,
                                nullptr, nullptr);
  ExpectPreClassificationChecks(second_url, nullptr, &kFalse, &kFalse, &kFalse,
                                &kFalse, nullptr);

  NavigateAndCommit(first_url);
  // Don't flush the message loop, as we want to navigate to a different
  // url before the final pre-classification checks are run.
  NavigateAndCommit(second_url);
  WaitAndCheckPreClassificationChecks();
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckPass) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // Navigate the tab to a page.  We should see a StartPhishingDetection IPC.
  EXPECT_CALL(*csd_service_, GetModelStr())
      .WillRepeatedly(ReturnRef(model_str_));
  GURL url("http://host.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse);
  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(&url);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckMatchAllowlist) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  EXPECT_CALL(*csd_service_, GetModelStr())
      .WillRepeatedly(ReturnRef(model_str_));
  GURL url("http://host.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kTrue, nullptr, nullptr, nullptr,
                                &kFalse);
  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckSameDocumentNavigation) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  EXPECT_CALL(*csd_service_, GetModelStr())
      .WillRepeatedly(ReturnRef(model_str_));
  GURL url("http://host.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse);
  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(&url);
  fake_phishing_detector_.Reset();

  // Now try an same-document navigation.  This should not trigger an IPC.
  EXPECT_CALL(*csd_service_, IsPrivateIPAddress(_)).Times(0);
  GURL inpage("http://host.com/#foo");
  ExpectPreClassificationChecks(inpage, nullptr, nullptr, nullptr, nullptr,
                                nullptr, nullptr);
  NavigateAndKeepLoading(web_contents(), inpage);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(nullptr);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckXHTML) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // Check that XHTML is supported, in addition to the default HTML type.
  EXPECT_CALL(*csd_service_, GetModelStr())
      .WillRepeatedly(ReturnRef(model_str_));
  GURL url("http://host.com/xhtml");
  auto navigation =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  navigation->SetContentsMimeType("application/xhtml+xml");
  navigation->SetKeepLoading(true);
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse);
  navigation->Commit();
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(&url);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckTwoNavigations) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // Navigate to two hosts, which should cause two IPCs.
  EXPECT_CALL(*csd_service_, GetModelStr())
      .WillRepeatedly(ReturnRef(model_str_));
  GURL url1("http://host1.com/");
  ExpectPreClassificationChecks(url1, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse);
  NavigateAndKeepLoading(web_contents(), url1);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(&url1);

  EXPECT_CALL(*csd_service_, GetModelStr())
      .WillRepeatedly(ReturnRef(model_str_));
  GURL url2("http://host2.com/");
  ExpectPreClassificationChecks(url2, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse);
  NavigateAndKeepLoading(web_contents(), url2);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(&url2);
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckPrivateIpAddress) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // If IsPrivateIPAddress returns true, no IPC should be triggered.
  EXPECT_CALL(*csd_service_, GetModelStr())
      .WillRepeatedly(ReturnRef(model_str_));
  GURL url("http://host3.com/");
  ExpectPreClassificationChecks(url, &kTrue, nullptr, nullptr, nullptr, nullptr,
                                nullptr);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(nullptr);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckLocalResource) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // If IsLocalResource returns true, no IPC should be triggered.
  EXPECT_CALL(*csd_service_, GetModelStr())
      .WillRepeatedly(ReturnRef(model_str_));
  GURL url("http://host3.com/");
  ExpectPreClassificationChecks(url, nullptr, nullptr, nullptr, nullptr,
                                nullptr, &kTrue);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(nullptr);
}

TEST_F(ClientSideDetectionHostIncognitoTest,
       TestPreClassificationCheckIncognito) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // If the tab is incognito there should be no IPC.  Also, we shouldn't
  // even check the csd-allowlist.
  EXPECT_CALL(*csd_service_, GetModelStr())
      .WillRepeatedly(ReturnRef(model_str_));
  GURL url("http://host4.com/");
  ExpectPreClassificationChecks(url, &kFalse, nullptr, nullptr, nullptr,
                                nullptr, &kFalse);

  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(nullptr);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckInvalidCache) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // If item is in the cache but it isn't valid, we will classify regardless
  // of whether we are over the reporting limit.
  EXPECT_CALL(*csd_service_, GetModelStr())
      .WillRepeatedly(ReturnRef(model_str_));
  GURL url("http://host6.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kTrue, nullptr,
                                &kFalse);

  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(&url);
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckOverPhishingReportingLimit) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // If the url isn't in the cache and we are over the reporting limit, we
  // don't do classification.
  EXPECT_CALL(*csd_service_, GetModelStr())
      .WillRepeatedly(ReturnRef(model_str_));
  GURL url("http://host7.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse, &kTrue,
                                &kFalse);
  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(nullptr);
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckOverBothReportingLimits) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  EXPECT_CALL(*csd_service_, GetModelStr())
      .WillRepeatedly(ReturnRef(model_str_));
  GURL url("http://host.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse, &kTrue,
                                &kFalse);
  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(nullptr);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckHttpsUrl) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  EXPECT_CALL(*csd_service_, GetModelStr())
      .WillRepeatedly(ReturnRef(model_str_));
  GURL url("https://host.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse);
  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(&url);
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckNoneHttpOrHttpsUrl) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  EXPECT_CALL(*csd_service_, GetModelStr())
      .WillRepeatedly(ReturnRef(model_str_));
  GURL url("file://host.com/");
  ExpectPreClassificationChecks(url, &kFalse, nullptr, nullptr, nullptr,
                                nullptr, &kFalse);
  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(nullptr);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckValidCached) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // If result is cached, we will try and display the blocking page directly
  // with no start classification message.
  EXPECT_CALL(*csd_service_, GetModelStr())
      .WillRepeatedly(ReturnRef(model_str_));
  GURL url("http://host8.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kTrue, &kFalse, &kFalse,
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

TEST_F(ClientSideDetectionHostTest, TestPreClassificationAllowlistedByPolicy) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // Configures enterprise allowlist.
  ScopedListPrefUpdate update(profile()->GetPrefs(),
                              prefs::kSafeBrowsingAllowlistDomains);
  update->Append("example.com");

  EXPECT_CALL(*csd_service_, GetModelStr())
      .WillRepeatedly(ReturnRef(model_str_));
  GURL url("http://example.com/");
  ExpectPreClassificationChecks(url, &kFalse, nullptr, nullptr, nullptr,
                                nullptr, &kFalse);

  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(nullptr);
}

INSTANTIATE_TEST_SUITE_P(, ClientSideDetectionHostPolicyTest, testing::Bool());

TEST_P(ClientSideDetectionHostPolicyTest,
       TestPreClassificationCheckPolicyEnabled) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  TestPreClassificationCheckCsdPhishingPolicy();
}

TEST_F(ClientSideDetectionHostTest, RecordsPhishingDetectorResults) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  {
    ClientPhishingRequest verdict;
    verdict.set_url("http://not-phishing.com/");
    verdict.set_client_score(0.1f);
    verdict.set_is_phishing(false);

    base::HistogramTester histogram_tester;

    EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(_, _, _))
        .Times(0);
    PhishingDetectionDone(verdict.SerializeAsString());
    EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));

    histogram_tester.ExpectUniqueSample(
        "SBClientPhishing.PhishingDetectorResult",
        mojom::PhishingDetectorResult::SUCCESS, 1);
  }

  {
    base::HistogramTester histogram_tester;

    EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(_, _, _))
        .Times(0);
    PhishingDetectionError(mojom::PhishingDetectorResult::CLASSIFIER_NOT_READY);
    EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));

    histogram_tester.ExpectUniqueSample(
        "SBClientPhishing.PhishingDetectorResult",
        mojom::PhishingDetectorResult::CLASSIFIER_NOT_READY, 1);
  }

  {
    base::HistogramTester histogram_tester;

    EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(_, _, _))
        .Times(0);
    PhishingDetectionError(
        mojom::PhishingDetectorResult::FORWARD_BACK_TRANSITION);
    EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));

    histogram_tester.ExpectUniqueSample(
        "SBClientPhishing.PhishingDetectorResult",
        mojom::PhishingDetectorResult::FORWARD_BACK_TRANSITION, 1);
  }
}

TEST_F(ClientSideDetectionHostTest, RecordsPhishingDetectionDuration) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PhishingDetectionDuration", 0);

  EXPECT_CALL(*csd_service_, GetModelStr())
      .WillRepeatedly(ReturnRef(model_str_));
  GURL start_url("http://safe.example.com/");
  ExpectPreClassificationChecks(start_url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse);
  NavigateAndCommit(start_url);
  WaitAndCheckPreClassificationChecks();
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PhishingDetectionDuration", 1);

  GURL url("http://phishing.example.com/");
  ClientPhishingRequest verdict;
  verdict.set_url(url.spec());
  verdict.set_client_score(0.1f);
  verdict.set_is_phishing(false);

  EXPECT_CALL(*csd_service_, GetModelStr())
      .WillRepeatedly(ReturnRef(model_str_));
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();
  const base::TimeDelta duration = base::Milliseconds(10);
  AdvanceTimeTickClock(duration);

  PhishingDetectionDone(verdict.SerializeAsString());

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PhishingDetectionDuration", 3);
  EXPECT_LE(duration.InMilliseconds(),
            histogram_tester
                .GetAllSamples("SBClientPhishing.PhishingDetectionDuration")
                .front()
                .min);
}

TEST_F(ClientSideDetectionHostTest, PopulatesPageLoadToken) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  GURL url("http://phishing.example.com/");
  ClientPhishingRequest verdict;
  verdict.set_client_score(1.0);
  verdict.set_is_phishing(true);

  EXPECT_CALL(*csd_service_, GetModelStr())
      .WillRepeatedly(ReturnRef(model_str_));
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  std::unique_ptr<ClientPhishingRequest> verdict_sent;
  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(_, _, _))
      .WillOnce(MoveArg<0>(&verdict_sent));
  PhishingDetectionDone(verdict.SerializeAsString());
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  ASSERT_EQ(1, verdict_sent->population().page_load_tokens_size());
}

class ClientSideDetectionHostDebugFeaturesTest
    : public ClientSideDetectionHostTest {
 public:
  ClientSideDetectionHostDebugFeaturesTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    command_line_.GetProcessCommandLine()->AppendSwitchPath(
        "--csd-debug-feature-directory", temp_dir_.GetPath());

    ClientSideDetectionHostTest::SetUp();
  }

 private:
  base::ScopedTempDir temp_dir_;
  base::test::ScopedCommandLine command_line_;
};

TEST_F(ClientSideDetectionHostDebugFeaturesTest,
       SkipsAllowlistWhenDumpingFeatures) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  EXPECT_CALL(*csd_service_, GetModelStr())
      .WillRepeatedly(ReturnRef(model_str_));
  GURL url("http://host.com/");
  ExpectPreClassificationChecks(url, &kFalse, nullptr, nullptr, nullptr,
                                nullptr, &kFalse);
  EXPECT_CALL(*database_manager_.get(), CheckCsdAllowlistUrl(url, _)).Times(0);
  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();
  fake_phishing_detector_.CheckMessage(&url);
}

TEST_F(ClientSideDetectionHostDebugFeaturesTest,
       SkipsCacheWhenDumpingFeatures) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  EXPECT_CALL(*csd_service_, GetModelStr())
      .WillRepeatedly(ReturnRef(model_str_));
  GURL url("http://host.com/");
  ExpectPreClassificationChecks(url, &kFalse, nullptr, nullptr, nullptr,
                                nullptr, &kFalse);
  EXPECT_CALL(*csd_service_, GetValidCachedResult(url, NotNull())).Times(0);
  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();
  fake_phishing_detector_.CheckMessage(&url);
}

TEST_F(ClientSideDetectionHostDebugFeaturesTest,
       SkipsReportLimitWhenDumpingFeatures) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  EXPECT_CALL(*csd_service_, GetModelStr())
      .WillRepeatedly(ReturnRef(model_str_));
  GURL url("http://host.com/");
  ExpectPreClassificationChecks(url, &kFalse, nullptr, nullptr, nullptr,
                                nullptr, &kFalse);
  EXPECT_CALL(*csd_service_, OverPhishingReportLimit()).Times(0);
  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();
  fake_phishing_detector_.CheckMessage(&url);
}

}  // namespace safe_browsing
