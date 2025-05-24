// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <tuple>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/safe_browsing/chrome_client_side_detection_host_delegate.h"
#include "chrome/browser/safe_browsing/chrome_safe_browsing_blocking_page_factory.h"
#include "chrome/browser/safe_browsing/chrome_ui_manager_delegate.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/verdict_cache_manager_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/content/browser/async_check_tracker.h"
#include "components/safe_browsing/content/browser/client_side_detection_feature_cache.h"
#include "components/safe_browsing/content/browser/client_side_detection_service.h"
#include "components/safe_browsing/content/browser/client_side_phishing_model.h"
#include "components/safe_browsing/content/browser/content_unsafe_resource_util.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/content/browser/url_checker_holder.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom-shared.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/hit_report.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/sync/sync_utils.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
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
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/http/http_status_code.h"
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

MATCHER_P(HasScamThreatSubtype, other, "") {
  return (other.threat_subtype == arg.threat_subtype);
}

// Test that the callback is nullptr when the verdict is not phishing.
MATCHER(CallbackIsNull, "") {
  return arg.is_null();
}

class MockClientSideDetectionService : public ClientSideDetectionService {
 public:
  MockClientSideDetectionService()
      : ClientSideDetectionService(nullptr, nullptr) {}

  MockClientSideDetectionService(const MockClientSideDetectionService&) =
      delete;
  MockClientSideDetectionService& operator=(
      const MockClientSideDetectionService&) = delete;

  ~MockClientSideDetectionService() override = default;

  MOCK_METHOD3(SendClientReportPhishingRequest,
               void(std::unique_ptr<ClientPhishingRequest>,
                    ClientReportPhishingRequestCallback,
                    const std::string&));
  MOCK_CONST_METHOD1(IsPrivateIPAddress, bool(const net::IPAddress&));
  MOCK_CONST_METHOD1(IsLocalResource, bool(const net::IPAddress&));
  MOCK_METHOD2(GetValidCachedResult, bool(const GURL&, bool*));
  MOCK_METHOD0(AtPhishingReportLimit, bool());
  MOCK_METHOD0(GetModelSharedMemoryRegion, base::ReadOnlySharedMemoryRegion());
  MOCK_METHOD0(GetModelType, CSDModelType());
  MOCK_METHOD0(IsModelAvailable, bool());
  MOCK_METHOD(
      void,
      InquireOnDeviceModel,
      (std::string,
       base::OnceCallback<void(
           std::optional<optimization_guide::proto::ScamDetectionResponse>)>));
  MOCK_METHOD0(LogOnDeviceModelEligibilityReason, void());
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
 protected:
  ~MockSafeBrowsingUIManager() override = default;
};

class MockSafeBrowsingDatabaseManager : public TestSafeBrowsingDatabaseManager {
 public:
  MockSafeBrowsingDatabaseManager()
      : safe_browsing::TestSafeBrowsingDatabaseManager(
            content::GetUIThreadTaskRunner({})) {}

  MockSafeBrowsingDatabaseManager(const MockSafeBrowsingDatabaseManager&) =
      delete;
  MockSafeBrowsingDatabaseManager& operator=(
      const MockSafeBrowsingDatabaseManager&) = delete;

  MOCK_METHOD2(CheckCsdAllowlistUrl,
               AsyncMatch(const GURL&, SafeBrowsingDatabaseManager::Client*));
  MOCK_CONST_METHOD1(CanCheckUrl, bool(const GURL&));

  // Calls the callback with the allowlist match result previously set by
  // |SetAllowlistLookupDetailsForUrl|. Returns std::nullopt. It crashes if
  // the allowlist match result is not set in advance for the |gurl|.
  void CheckUrlForHighConfidenceAllowlist(
      const GURL& gurl,
      CheckUrlForHighConfidenceAllowlistCallback callback) override {
    std::string url = gurl.spec();
    DCHECK(base::Contains(urls_allowlist_match_, url));

    ui_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            /*url_on_high_confidence_allowlist=*/urls_allowlist_match_[url],
            /*logging_details=*/std::nullopt));
  }

  void SetAllowlistLookupDetailsForUrl(const GURL& gurl, bool match) {
    std::string url = gurl.spec();
    urls_allowlist_match_[url] = match;
  }

 protected:
  ~MockSafeBrowsingDatabaseManager() override = default;

 private:
  base::flat_map<std::string, bool> urls_allowlist_match_;
};

class MockClientSideDetectionHostDelegate
    : public ChromeClientSideDetectionHostDelegate {
 public:
  explicit MockClientSideDetectionHostDelegate(
      content::WebContents* web_contents)
      : ChromeClientSideDetectionHostDelegate(web_contents) {}

  void GetInnerText(HostInnerTextCallback callback) override {
    std::move(callback).Run(inner_text_);
  }

  void ForceEmptyInnerText() { inner_text_ = ""; }

 private:
  std::string inner_text_ = "inner text";
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
                            mojo_base::ProtoWrapper(request));

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
      : is_incognito_(is_incognito) {}

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
    csd_service_ = std::make_unique<NiceMock<MockClientSideDetectionService>>();
    database_manager_ = new NiceMock<MockSafeBrowsingDatabaseManager>();
    ui_manager_ = new NiceMock<MockSafeBrowsingUIManager>();

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
        std::make_unique<NiceMock<MockSafeBrowsingTokenFetcher>>();
    raw_token_fetcher_ = token_fetcher.get();
    csd_host_->set_token_fetcher_for_testing(std::move(token_fetcher));
    auto delegate =
        std::make_unique<MockClientSideDetectionHostDelegate>(web_contents());
    raw_delegate_ = delegate.get();
    csd_host_->set_delegate_for_testing(std::move(delegate));
    // Commit to a URL for tests that do not explicitly NavigateAndCommit.
    // Committing to "about:blank" avoids triggering logic irrelevant for tests.
    NavigateAndCommit(GURL("about:blank"));

    testing::DefaultValue<CSDModelType>::Set(CSDModelType::kFlatbuffer);
  }

  void TearDown() override {
    raw_token_fetcher_ = nullptr;
    raw_delegate_ = nullptr;

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

  void PhishingDetectionDone(std::optional<mojo_base::ProtoWrapper> verdict) {
    csd_host_->PhishingDetectionDone(
        ClientSideDetectionType::TRIGGER_MODELS,
        /*is_sample_ping=*/false, /*did_match_high_confidence_allowlist=*/false,
        mojom::PhishingDetectorResult::SUCCESS, std::move(verdict));
  }

  void PhishingDetectionDoneWithHighConfidenceAllowlistMatch(
      std::optional<mojo_base::ProtoWrapper> verdict) {
    csd_host_->PhishingDetectionDone(
        ClientSideDetectionType::TRIGGER_MODELS,
        /*is_sample_ping=*/false, /*did_match_high_confidence_allowlist=*/true,
        mojom::PhishingDetectorResult::SUCCESS, std::move(verdict));
  }

  void PhishingDetectionError(mojom::PhishingDetectorResult error) {
    csd_host_->PhishingDetectionDone(
        ClientSideDetectionType::TRIGGER_MODELS,
        /*is_sample_ping=*/false, /*did_match_high_confidence_allowlist=*/false,
        error, std::nullopt);
  }

  void ExpectPreClassificationChecks(const GURL& url,
                                     const bool* is_private,
                                     const bool* match_csd_allowlist,
                                     const bool* get_valid_cached_result,
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
      EXPECT_CALL(*database_manager_.get(), CanCheckUrl(_))
          .WillOnce(Return(true));
    } else {
      EXPECT_CALL(*database_manager_.get(), CheckCsdAllowlistUrl(url, _))
          .Times(0);
    }
    if (get_valid_cached_result) {
      EXPECT_CALL(*csd_service_, GetValidCachedResult(url, NotNull()))
          .WillOnce(
              DoAll(SetArgPointee<1>(true), Return(*get_valid_cached_result)));
    }
    if (over_phishing_report_limit) {
      EXPECT_CALL(*csd_service_, AtPhishingReportLimit())
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

  std::string GetRequestTypeName(
      ClientSideDetectionType client_side_detection_type) {
    switch (client_side_detection_type) {
      case safe_browsing::ClientSideDetectionType::
          CLIENT_SIDE_DETECTION_TYPE_UNSPECIFIED:
        return "Unknown";
      case safe_browsing::ClientSideDetectionType::FORCE_REQUEST:
        return "ForceRequest";
      case safe_browsing::ClientSideDetectionType::
          NOTIFICATION_PERMISSION_PROMPT:
        return "NotificationPermissionPrompt";
      case safe_browsing::ClientSideDetectionType::TRIGGER_MODELS:
        return "TriggerModel";
      case safe_browsing::ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED:
        return "KeyboardLockRequested";
      case safe_browsing::ClientSideDetectionType::POINTER_LOCK_REQUESTED:
        return "PointerLockRequested";
      case safe_browsing::ClientSideDetectionType::VIBRATION_API:
        return "VibrationApi";
      case safe_browsing::ClientSideDetectionType::FULLSCREEN_API:
        return "FullscreenApi";
    }
  }

 protected:
  std::unique_ptr<ClientSideDetectionHost> csd_host_;
  std::unique_ptr<NiceMock<MockClientSideDetectionService>> csd_service_;
  scoped_refptr<NiceMock<MockSafeBrowsingUIManager>> ui_manager_;
  scoped_refptr<NiceMock<MockSafeBrowsingDatabaseManager>> database_manager_;
  FakePhishingDetector fake_phishing_detector_;
  raw_ptr<NiceMock<MockSafeBrowsingTokenFetcher>> raw_token_fetcher_ = nullptr;
  raw_ptr<MockClientSideDetectionHostDelegate> raw_delegate_ = nullptr;
  base::SimpleTestTickClock clock_;
  const bool is_incognito_;
  signin::IdentityTestEnvironment identity_test_env_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<WebContentsObserver> observer_;
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
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // Case 0: renderer sends an invalid protobuf that we're unable to
  // parse. This has the same behavior as providing nullopt.
  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(_, _, _)).Times(0);
  PhishingDetectionDone(std::nullopt);
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
  PhishingDetectionDone(mojo_base::ProtoWrapper(verdict));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  ASSERT_FALSE(cb.is_null());

  // Make sure DisplayBlockingPage is not going to be called.
  EXPECT_CALL(*ui_manager_.get(), DisplayBlockingPage(_)).Times(0);
  std::move(cb).Run(GURL(verdict.url()), false, net::HTTP_OK, std::nullopt);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Mock::VerifyAndClear(ui_manager_.get()));
}

TEST_F(ClientSideDetectionHostTest, PhishingDetectionDoneShowInterstitial) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  base::HistogramTester histogram_tester;

  // Case 2: client thinks the page is phishing and so does the server.
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
  PhishingDetectionDone(mojo_base::ProtoWrapper(verdict));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  ASSERT_FALSE(cb.is_null());

  UnsafeResource resource;
  EXPECT_CALL(*ui_manager_.get(), DisplayBlockingPage(_))
      .WillOnce(SaveArg<0>(&resource));
  std::move(cb).Run(phishing_url, true, net::HTTP_OK, std::nullopt);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Mock::VerifyAndClear(ui_manager_.get()));
  EXPECT_EQ(phishing_url, resource.url);
  EXPECT_EQ(phishing_url, resource.original_url);
  EXPECT_EQ(SBThreatType::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING,
            resource.threat_type);
  EXPECT_EQ(ThreatSource::CLIENT_SIDE_DETECTION, resource.threat_source);
  EXPECT_EQ(web_contents(),
            unsafe_resource_util::GetWebContentsForResource(resource));
  EXPECT_TRUE(resource.navigation_id.has_value());

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.HighConfidenceAllowlistMatchOnServerVerdictPhishy",
      false, 1);
}

TEST_F(ClientSideDetectionHostTest, PhishingDetectionDoneMultiplePings) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // Case 3 & 4: client thinks a page is phishing then navigates to
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
  PhishingDetectionDone(mojo_base::ProtoWrapper(verdict));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  ASSERT_FALSE(cb.is_null());

  GURL other_phishing_url("http://other_phishing_url.com/bla");
  database_manager_->SetAllowlistLookupDetailsForUrl(other_phishing_url, false);
  ExpectPreClassificationChecks(other_phishing_url, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse);
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
  PhishingDetectionDone(mojo_base::ProtoWrapper(verdict));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  ASSERT_FALSE(cb_other.is_null());

  // We expect that the interstitial is shown for the second phishing URL and
  // not for the first phishing URL.
  UnsafeResource resource;
  EXPECT_CALL(*ui_manager_.get(), DisplayBlockingPage(_))
      .WillOnce(SaveArg<0>(&resource));

  std::move(cb).Run(phishing_url, true, net::HTTP_OK,
                    std::nullopt);  // Should have no effect.
  std::move(cb_other).Run(other_phishing_url, true, net::HTTP_OK,
                          std::nullopt);  // Should show interstitial.

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Mock::VerifyAndClear(ui_manager_.get()));
  EXPECT_EQ(other_phishing_url, resource.url);
  EXPECT_EQ(other_phishing_url, resource.original_url);
  EXPECT_EQ(SBThreatType::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING,
            resource.threat_type);
  EXPECT_EQ(ThreatSource::CLIENT_SIDE_DETECTION, resource.threat_source);
  EXPECT_EQ(web_contents(),
            unsafe_resource_util::GetWebContentsForResource(resource));
}

TEST_F(ClientSideDetectionHostTest, PhishingDetectionDoneVerdictNotPhishing) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // Case 5: renderer sends a verdict string that isn't phishing.
  ClientPhishingRequest verdict;
  verdict.set_url("http://not-phishing.com/");
  verdict.set_client_score(0.1f);
  verdict.set_is_phishing(false);

  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(_, _, _)).Times(0);
  PhishingDetectionDone(mojo_base::ProtoWrapper(verdict));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
}

TEST_F(
    ClientSideDetectionHostTest,
    PhishingDetectionDoneServerModelPhishyAndExistsInHighConfidenceAllowlist) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  base::HistogramTester histogram_tester;

  // Client thinks the page is phishing and so does the server.
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
  // Bypass the preclassification check with where the allowlist check occurs,
  // since this unit test strictly tests post classification allowlist match
  // check.
  PhishingDetectionDoneWithHighConfidenceAllowlistMatch(
      mojo_base::ProtoWrapper(verdict));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  ASSERT_FALSE(cb.is_null());

  UnsafeResource resource;
  EXPECT_CALL(*ui_manager_.get(), DisplayBlockingPage(_))
      .WillOnce(SaveArg<0>(&resource));
  std::move(cb).Run(phishing_url, true, net::HTTP_OK, std::nullopt);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Mock::VerifyAndClear(ui_manager_.get()));
  EXPECT_EQ(phishing_url, resource.url);
  EXPECT_EQ(phishing_url, resource.original_url);
  EXPECT_EQ(SBThreatType::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING,
            resource.threat_type);
  EXPECT_EQ(ThreatSource::CLIENT_SIDE_DETECTION, resource.threat_source);
  EXPECT_EQ(web_contents(),
            unsafe_resource_util::GetWebContentsForResource(resource));

  // Test that the histogram has been logged that the allowlist did exist with
  // the server model verdict phishy.
  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.ServerModelDetectsPhishing", true, 1);
  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.HighConfidenceAllowlistMatchOnServerVerdictPhishy",
      true, 1);
}

TEST_F(ClientSideDetectionHostTest,
       PhishingDetectionDoneVerdictNotPhishingButSBMatchOnNewRVH) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // When navigating to a different host (thus creating a pending RVH) which
  // matches regular malware list, and after navigation the renderer sends a
  // verdict string that isn't phishing, we should still send the report.

  GURL start_url("http://safe.example.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(start_url, false);
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

  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  PhishingDetectionDone(mojo_base::ProtoWrapper(verdict));
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
  PhishingDetectionDone(mojo_base::ProtoWrapper(verdict));

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
  PhishingDetectionDone(mojo_base::ProtoWrapper(verdict));

  // Wait for token fetcher to be called.
  EXPECT_TRUE(Mock::VerifyAndClear(raw_token_fetcher_));

  ASSERT_FALSE(cb.is_null());
  std::move(cb).Run("fake_access_token_1");

  // Make the call again.
  EXPECT_CALL(*raw_token_fetcher_, Start(_))
      .Times(1)
      .WillRepeatedly(MoveArg<0>(&cb));
  PhishingDetectionDone(mojo_base::ProtoWrapper(verdict));
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
  PhishingDetectionDone(mojo_base::ProtoWrapper(verdict));
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
  PhishingDetectionDone(mojo_base::ProtoWrapper(verdict));
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
  EXPECT_CALL(*csd_service_, IsPrivateIPAddress(_))
      .WillOnce(Return(false))
      .WillOnce(Return(false));
  ExpectPreClassificationChecks(first_url, nullptr, &kFalse, nullptr, nullptr,
                                nullptr);
  ExpectPreClassificationChecks(second_url, nullptr, &kFalse, &kFalse, &kFalse,
                                nullptr);

  NavigateAndCommit(first_url);
  // Don't flush the message loop, as we want to navigate to a different
  // url before the final pre-classification checks are run.
  NavigateAndCommit(second_url);
  WaitAndCheckPreClassificationChecks();
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckPass) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  base::HistogramTester histogram_tester;

  // Navigate the tab to a page.  We should see a StartPhishingDetection IPC.
  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse);
  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(&url);

  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckResult",
      PreClassificationCheckResult::CLASSIFY, 1);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckResult.TriggerModel",
      PreClassificationCheckResult::CLASSIFY, 1);
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckMatchCSDAllowlist) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);
  ExpectPreClassificationChecks(url, &kFalse, &kTrue, nullptr, nullptr,
                                &kFalse);
  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckMatchHighConfidenceAllowlist) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  std::vector<base::test::FeatureRef> enabled_features = {};
  enabled_features.push_back(kClientSideDetectionAcceptHCAllowlist);
  SetFeatures(enabled_features, {});

  csd_host_->set_high_confidence_allowlist_acceptance_rate_for_testing(1.0f);
  base::HistogramTester histogram_tester;

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/true);
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, nullptr,
                                nullptr);
  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.MatchHighConfidenceAllowlist.TriggerModel", 1);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckResult",
      PreClassificationCheckResult::NO_CLASSIFY_MATCH_HC_ALLOWLIST, 1);
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckDoesNotMatchHighConfidenceAllowlist) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  std::vector<base::test::FeatureRef> enabled_features = {};
  enabled_features.push_back(kClientSideDetectionAcceptHCAllowlist);
  SetFeatures(enabled_features, {});

  csd_host_->set_high_confidence_allowlist_acceptance_rate_for_testing(0.0f);
  base::HistogramTester histogram_tester;

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/true);
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, nullptr,
                                nullptr);
  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.MatchHighConfidenceAllowlist.TriggerModel", 1);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckResult",
      PreClassificationCheckResult::NO_CLASSIFY_MATCH_HC_ALLOWLIST, 0);
}

TEST_F(
    ClientSideDetectionHostTest,
    TestPreClassificationCheckDoesNotMatchHighConfidenceAllowlistDueToDisabledFeature) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  std::vector<base::test::FeatureRef> disabled_features = {};
  disabled_features.push_back(kClientSideDetectionAcceptHCAllowlist);
  SetFeatures({}, disabled_features);

  // We will set the acceptance rate to 100%, but it won't be accepted because
  // the feature is disabled.
  csd_host_->set_high_confidence_allowlist_acceptance_rate_for_testing(1.0f);
  base::HistogramTester histogram_tester;

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/true);
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, nullptr,
                                nullptr);
  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.MatchHighConfidenceAllowlist.TriggerModel", 1);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckResult",
      PreClassificationCheckResult::NO_CLASSIFY_MATCH_HC_ALLOWLIST, 0);
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckSameDocumentNavigation) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);
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
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // Check that XHTML is supported, in addition to the default HTML type.
  GURL url("http://host.com/xhtml");
  auto navigation =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  navigation->SetContentsMimeType("application/xhtml+xml");
  navigation->SetKeepLoading(true);

  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);

  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse);
  navigation->Commit();
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(&url);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckTwoNavigations) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // Navigate to two hosts, which should cause two IPCs.
  GURL url1("http://host1.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url1, false);
  ExpectPreClassificationChecks(url1, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse);
  NavigateAndKeepLoading(web_contents(), url1);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(&url1);

  GURL url2("http://host2.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url2, false);
  ExpectPreClassificationChecks(url2, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse);
  NavigateAndKeepLoading(web_contents(), url2);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(&url2);
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckPrivateIpAddress) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // If IsPrivateIPAddress returns true, no IPC should be triggered.
  GURL url("http://host3.com/");
  ExpectPreClassificationChecks(url, &kTrue, nullptr, nullptr, nullptr,
                                nullptr);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(nullptr);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckLocalResource) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // If IsLocalResource returns true, no IPC should be triggered.
  GURL url("http://host3.com/");
  ExpectPreClassificationChecks(url, nullptr, nullptr, nullptr, nullptr,
                                &kTrue);
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
  GURL url("http://host4.com/");
  ExpectPreClassificationChecks(url, &kFalse, nullptr, nullptr, nullptr,
                                &kFalse);

  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(nullptr);
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckOverPhishingReportingLimit) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // If the url isn't in the cache and we are over the reporting limit, we
  // don't do classification.
  GURL url("http://host7.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kTrue,
                                &kFalse);
  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(nullptr);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckHttpsUrl) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  GURL url("https://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse);
  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(&url);
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckNoneHttpOrHttpsUrl) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  GURL url("file://host.com/");
  ExpectPreClassificationChecks(url, &kFalse, nullptr, nullptr, nullptr,
                                &kFalse);
  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(nullptr);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckValidCached) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // If result is cached, we will try and display the blocking page directly
  // with no start classification message.
  GURL url("http://host8.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);
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

TEST_F(ClientSideDetectionHostTest, TestPreClassificationAllowlistedByPolicy) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  // Configures enterprise allowlist.
  ScopedListPrefUpdate update(profile()->GetPrefs(),
                              prefs::kSafeBrowsingAllowlistDomains);
  update->Append("example.com");

  GURL url("http://example.com/");
  ExpectPreClassificationChecks(url, &kFalse, nullptr, nullptr, nullptr,
                                &kFalse);

  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(nullptr);
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
    PhishingDetectionDone(mojo_base::ProtoWrapper(verdict));
    EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));

    histogram_tester.ExpectUniqueSample(
        "SBClientPhishing.PhishingDetectorResult.TriggerModel",
        mojom::PhishingDetectorResult::SUCCESS, 1);
  }

  {
    base::HistogramTester histogram_tester;

    EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(_, _, _))
        .Times(0);
    PhishingDetectionError(mojom::PhishingDetectorResult::CLASSIFIER_NOT_READY);
    EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));

    histogram_tester.ExpectUniqueSample(
        "SBClientPhishing.PhishingDetectorResult.TriggerModel",
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
        "SBClientPhishing.PhishingDetectorResult.TriggerModel",
        mojom::PhishingDetectorResult::FORWARD_BACK_TRANSITION, 1);
  }
}

TEST_F(ClientSideDetectionHostTest, RecordsPhishingDetectionDuration) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PhishingDetectionDuration.TriggerModel", 0);

  GURL start_url("http://safe.example.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(start_url, false);
  ExpectPreClassificationChecks(start_url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse);
  NavigateAndCommit(start_url);
  WaitAndCheckPreClassificationChecks();
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PhishingDetectionDuration.TriggerModel", 1);

  GURL url("http://phishing.example.com/");
  ClientPhishingRequest verdict;
  verdict.set_url(url.spec());
  verdict.set_client_score(0.1f);
  verdict.set_is_phishing(false);

  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();
  const base::TimeDelta duration = base::Milliseconds(10);
  AdvanceTimeTickClock(duration);

  PhishingDetectionDone(mojo_base::ProtoWrapper(verdict));

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PhishingDetectionDuration.TriggerModel", 3);
  EXPECT_LE(duration.InMilliseconds(),
            histogram_tester
                .GetAllSamples(
                    "SBClientPhishing.PhishingDetectionDuration.TriggerModel")
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

  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  std::unique_ptr<ClientPhishingRequest> verdict_sent;
  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(_, _, _))
      .WillOnce(MoveArg<0>(&verdict_sent));
  PhishingDetectionDone(mojo_base::ProtoWrapper(verdict));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  ASSERT_EQ(1, verdict_sent->population().page_load_tokens_size());
}

TEST_F(ClientSideDetectionHostTest,
       CSDFeaturesCacheContainsVerdictAndFullDebuggingMetadata) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  std::vector<base::test::FeatureRef> enabled_features = {};
  enabled_features.push_back(kClientSideDetectionDebuggingMetadataCache);
  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);
  SetFeatures(enabled_features, {});

  ClientPhishingRequest* verdict_from_cache = nullptr;
  LoginReputationClientRequest::DebuggingMetadata* debugging_metadata = nullptr;

  GURL example_url("http://phishingurl.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(example_url, false);
  ExpectPreClassificationChecks(
      /*url=*/example_url, /*is_private=*/&kFalse,
      /*match_csd_allowlist=*/&kFalse, /*get_valid_cached_result=*/&kFalse,
      /*over_phishing_report_limit=*/&kFalse, /*is_local=*/&kFalse);
  NavigateAndCommit(example_url);
  WaitAndCheckPreClassificationChecks();

  ClientSideDetectionService::ClientReportPhishingRequestCallback cb;
  ClientPhishingRequest verdict;
  verdict.set_url(example_url.spec());
  verdict.set_client_score(1.0f);
  verdict.set_is_phishing(true);
  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(
                                 PartiallyEqualVerdict(verdict), _,
                                 "fake_access_token_for_debug_cache"))
      .WillOnce(MoveArg<1>(&cb));

  // Set up mock call to token fetcher.
  SafeBrowsingTokenFetcher::Callback token_cb;
  EXPECT_CALL(*raw_token_fetcher_, Start(_))
      .Times(1)
      .WillRepeatedly(MoveArg<0>(&token_cb));
  PhishingDetectionDone(mojo_base::ProtoWrapper(verdict));

  // Wait for token fetcher to be called.
  EXPECT_TRUE(Mock::VerifyAndClear(raw_token_fetcher_));

  ASSERT_FALSE(token_cb.is_null());
  std::move(token_cb).Run("fake_access_token_for_debug_cache");

  // Token is now fetched, so we will now callback on
  // ClientReportPhishingRequest.
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));

  ASSERT_FALSE(cb.is_null());
  std::move(cb).Run(example_url, false, net::HTTP_OK, std::nullopt);

  ClientSideDetectionFeatureCache* feature_cache_map =
      ClientSideDetectionFeatureCache::FromWebContents(web_contents());
  verdict_from_cache = feature_cache_map->GetVerdictForURL(example_url);
  debugging_metadata =
      feature_cache_map->GetDebuggingMetadataForURL(example_url);

  // Model version and force request field are not checked because the model
  // isn't deployed, and the verdict cache manager is not populated.
  EXPECT_NE(debugging_metadata, nullptr);
  EXPECT_EQ(debugging_metadata->preclassification_check_result(),
            PreClassificationCheckResult::CLASSIFY);
  EXPECT_EQ(debugging_metadata->network_result(), net::HTTP_OK);
  EXPECT_EQ(debugging_metadata->phishing_detector_result(),
            PhishingDetectorResult::CLASSIFICATION_SUCCESS);
  EXPECT_EQ(debugging_metadata->local_model_detects_phishing(),
            verdict_from_cache->is_phishing());

  EXPECT_NE(verdict_from_cache, nullptr);
  EXPECT_EQ(verdict_from_cache->is_phishing(), verdict.is_phishing());
  EXPECT_EQ(verdict_from_cache->client_score(), verdict.client_score());
}

TEST_F(ClientSideDetectionHostTest,
       RTLookupResponseForceRequestSendsCSPPPingWhenVerdictNotPhishing) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  base::HistogramTester histogram_tester;

  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  SetFeatures({kClientSideDetectionSendLlamaForcedTriggerInfo}, {});
  GURL example_url("http://suspiciousurl.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(example_url, false);
  ExpectPreClassificationChecks(
      /*url=*/example_url, /*is_private=*/&kFalse,
      /*match_csd_allowlist=*/&kFalse, /*get_valid_cached_result=*/&kFalse,
      /*over_phishing_report_limit=*/&kFalse, /*is_local=*/&kFalse);
  NavigateAndCommit(example_url);
  WaitAndCheckPreClassificationChecks();

  VerdictCacheManager* cache_manager =
      VerdictCacheManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()));

  RTLookupResponse response;

  RTLookupResponse::ThreatInfo* new_threat_info2 = response.add_threat_info();
  new_threat_info2->set_verdict_type(RTLookupResponse::ThreatInfo::DANGEROUS);
  new_threat_info2->set_threat_type(
      RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING);
  new_threat_info2->set_cache_duration_sec(60);
  new_threat_info2->set_cache_expression_using_match_type("suspiciousurl.com/");
  new_threat_info2->set_cache_expression_match_type(
      RTLookupResponse::ThreatInfo::EXACT_MATCH);

  response.set_client_side_detection_type(
      safe_browsing::ClientSideDetectionType::FORCE_REQUEST);
  cache_manager->CacheRealTimeUrlVerdict(response, base::Time::Now());
  EXPECT_EQ(
      static_cast<int>(safe_browsing::ClientSideDetectionType::FORCE_REQUEST),
      cache_manager->GetCachedRealTimeUrlClientSideDetectionType(example_url));

  ClientSideDetectionService::ClientReportPhishingRequestCallback cb;

  // The verdict's is_phishing is false, but we will still send a ping!
  ClientPhishingRequest verdict;
  verdict.set_url(example_url.spec());
  verdict.set_client_score(0.8f);
  verdict.set_is_phishing(false);
  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(
                                 PartiallyEqualVerdict(verdict), _,
                                 "fake_access_token_for_force_request"))
      .WillOnce(MoveArg<1>(&cb));

  // Set up mock call to token fetcher.
  SafeBrowsingTokenFetcher::Callback token_cb;
  EXPECT_CALL(*raw_token_fetcher_, Start(_))
      .Times(1)
      .WillRepeatedly(MoveArg<0>(&token_cb));
  PhishingDetectionDone(mojo_base::ProtoWrapper(verdict));

  // Wait for token fetcher to be called.
  EXPECT_TRUE(Mock::VerifyAndClear(raw_token_fetcher_));

  ASSERT_FALSE(token_cb.is_null());
  std::move(token_cb).Run("fake_access_token_for_force_request");

  // Token is now fetched, so we will now callback on
  // ClientReportPhishingRequest.
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));

  ASSERT_FALSE(cb.is_null());
  std::move(cb).Run(example_url, false, net::HTTP_OK, std::nullopt);

  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      ClientSideDetectionType::FORCE_REQUEST, 1);
  histogram_tester.ExpectBucketCount("SBClientPhishing.RTLookupForceRequest",
                                     true, 1);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.RTLookupForceRequest.HasLlamaForcedTriggerInfo", false,
      1);
}

TEST_F(ClientSideDetectionHostTest,
       RTLookupResponseOnFirstURLInRedirectChainTriggersForceRequest) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  base::HistogramTester histogram_tester;

  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  GURL first_url_redirect("http://firsturlsuspicious.com/");
  GURL second_url_redirect("http://secondurlnotsuspicious.com/");
  GURL third_url_redirect("http://thirdurlnotsuspicious.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(first_url_redirect, false);
  database_manager_->SetAllowlistLookupDetailsForUrl(second_url_redirect,
                                                     false);
  database_manager_->SetAllowlistLookupDetailsForUrl(third_url_redirect, false);

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      first_url_redirect, web_contents());
  navigation->Start();
  navigation->Redirect(second_url_redirect);
  navigation->Redirect(third_url_redirect);
  navigation->Commit();

  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();

  ASSERT_TRUE(entry);
  EXPECT_EQ(entry->GetRedirectChain().size(), 3u);

  VerdictCacheManager* cache_manager =
      VerdictCacheManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()));

  // We will only create a RTLookupResponse for the first URL and cache it in
  // the cache_manager.
  RTLookupResponse response;

  RTLookupResponse::ThreatInfo* new_threat_info2 = response.add_threat_info();
  new_threat_info2->set_verdict_type(RTLookupResponse::ThreatInfo::DANGEROUS);
  new_threat_info2->set_threat_type(
      RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING);
  new_threat_info2->set_cache_duration_sec(60);
  new_threat_info2->set_cache_expression_using_match_type(
      "firsturlsuspicious.com/");
  new_threat_info2->set_cache_expression_match_type(
      RTLookupResponse::ThreatInfo::EXACT_MATCH);

  response.set_client_side_detection_type(
      safe_browsing::ClientSideDetectionType::FORCE_REQUEST);
  cache_manager->CacheRealTimeUrlVerdict(response, base::Time::Now());
  EXPECT_EQ(
      static_cast<int>(safe_browsing::ClientSideDetectionType::FORCE_REQUEST),
      cache_manager->GetCachedRealTimeUrlClientSideDetectionType(
          first_url_redirect));

  ClientSideDetectionService::ClientReportPhishingRequestCallback cb;

  // The verdict's is_phishing is false, but we will still send a ping! We are
  // using the third URL for the verdict because it's the last in the referrer
  // chain, but the first is in the cache, so it should still send a ping.
  ClientPhishingRequest verdict;
  verdict.set_url(third_url_redirect.spec());
  verdict.set_client_score(0.8f);
  verdict.set_is_phishing(false);
  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(
                                 PartiallyEqualVerdict(verdict), _,
                                 "fake_access_token_for_force_request"))
      .WillOnce(MoveArg<1>(&cb));

  // Set up mock call to token fetcher.
  SafeBrowsingTokenFetcher::Callback token_cb;
  EXPECT_CALL(*raw_token_fetcher_, Start(_))
      .Times(1)
      .WillRepeatedly(MoveArg<0>(&token_cb));
  PhishingDetectionDone(mojo_base::ProtoWrapper(verdict));

  // Wait for token fetcher to be called.
  EXPECT_TRUE(Mock::VerifyAndClear(raw_token_fetcher_));

  ASSERT_FALSE(token_cb.is_null());
  std::move(token_cb).Run("fake_access_token_for_force_request");

  // Token is now fetched, so we will now callback on
  // ClientReportPhishingRequest.
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));

  ASSERT_FALSE(cb.is_null());
  std::move(cb).Run(third_url_redirect, false, net::HTTP_OK, std::nullopt);

  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      ClientSideDetectionType::FORCE_REQUEST, 1);
  histogram_tester.ExpectBucketCount("SBClientPhishing.RTLookupForceRequest",
                                     true, 1);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.RedirectChainContainsForceRequest", true, 1);
}

TEST_F(ClientSideDetectionHostTest,
       NoRTLookupResponseInRedirectChainContainsForceRequest) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  base::HistogramTester histogram_tester;

  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  GURL first_url_redirect("http://firsturlnotsuspicious.com/");
  GURL second_url_redirect("http://secondurlnotsuspicious.com/");
  GURL third_url_redirect("http://thirdurlnotsuspicious.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(first_url_redirect, false);
  database_manager_->SetAllowlistLookupDetailsForUrl(second_url_redirect,
                                                     false);
  database_manager_->SetAllowlistLookupDetailsForUrl(third_url_redirect, false);

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      first_url_redirect, web_contents());
  navigation->Start();
  navigation->Redirect(second_url_redirect);
  navigation->Redirect(third_url_redirect);
  navigation->Commit();

  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();

  ASSERT_TRUE(entry);
  EXPECT_EQ(entry->GetRedirectChain().size(), 3u);

  // The verdict's is_phishing is false, and there are no force requests at all
  // in the current URL and its redirect chain, so no ping will be sent.
  ClientPhishingRequest verdict;
  verdict.set_url(third_url_redirect.spec());
  verdict.set_client_score(0.8f);
  verdict.set_is_phishing(false);

  PhishingDetectionDone(mojo_base::ProtoWrapper(verdict));

  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));

  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      ClientSideDetectionType::FORCE_REQUEST, 0);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      ClientSideDetectionType::TRIGGER_MODELS, 1);
  histogram_tester.ExpectBucketCount("SBClientPhishing.RTLookupForceRequest",
                                     false, 1);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.RedirectChainContainsForceRequest", false, 1);
}

TEST_F(ClientSideDetectionHostTest,
       RedirectChainKillswitchDoesNotTriggersForceRequest) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  SetFeatures({kClientSideDetectionRedirectChainKillswitch}, {});

  base::HistogramTester histogram_tester;

  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  GURL first_url_redirect("http://firsturlsuspicious.com/");
  GURL second_url_redirect("http://secondurlnotsuspicious.com/");
  GURL third_url_redirect("http://thirdurlnotsuspicious.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(first_url_redirect, false);
  database_manager_->SetAllowlistLookupDetailsForUrl(second_url_redirect,
                                                     false);
  database_manager_->SetAllowlistLookupDetailsForUrl(third_url_redirect, false);

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      first_url_redirect, web_contents());
  navigation->Start();
  navigation->Redirect(second_url_redirect);
  navigation->Redirect(third_url_redirect);
  navigation->Commit();

  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();

  ASSERT_TRUE(entry);
  EXPECT_EQ(entry->GetRedirectChain().size(), 3u);

  VerdictCacheManager* cache_manager =
      VerdictCacheManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()));

  // We will only create a RTLookupResponse for the first URL and cache it in
  // the cache_manager.
  RTLookupResponse response;

  RTLookupResponse::ThreatInfo* new_threat_info2 = response.add_threat_info();
  new_threat_info2->set_verdict_type(RTLookupResponse::ThreatInfo::DANGEROUS);
  new_threat_info2->set_threat_type(
      RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING);
  new_threat_info2->set_cache_duration_sec(60);
  new_threat_info2->set_cache_expression_using_match_type(
      "firsturlsuspicious.com/");
  new_threat_info2->set_cache_expression_match_type(
      RTLookupResponse::ThreatInfo::EXACT_MATCH);

  response.set_client_side_detection_type(
      safe_browsing::ClientSideDetectionType::FORCE_REQUEST);
  cache_manager->CacheRealTimeUrlVerdict(response, base::Time::Now());
  EXPECT_EQ(
      static_cast<int>(safe_browsing::ClientSideDetectionType::FORCE_REQUEST),
      cache_manager->GetCachedRealTimeUrlClientSideDetectionType(
          first_url_redirect));

  // The verdict's is_phishing is false, but we will still send a ping! We are
  // using the third URL for the verdict because it's the last in the referrer
  // chain, but the first is in the cache, so it should still send a ping.
  ClientPhishingRequest verdict;
  verdict.set_url(third_url_redirect.spec());
  verdict.set_client_score(0.8f);
  verdict.set_is_phishing(false);
  PhishingDetectionDone(mojo_base::ProtoWrapper(verdict));

  // Token is now fetched, so we will now callback on
  // ClientReportPhishingRequest.
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));

  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      ClientSideDetectionType::TRIGGER_MODELS, 1);
  histogram_tester.ExpectBucketCount("SBClientPhishing.RTLookupForceRequest",
                                     false, 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.RedirectChainContainsForceRequest", 0);
}

class ClientSideDetectionHostNotificationTest
    : public ClientSideDetectionHostTest {
 public:
  ClientSideDetectionHostNotificationTest() = default;

  void SetUp() override {
    ClientSideDetectionHostTest::SetUp();

    SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);
    SetFeatures({kClientSideDetectionNotificationPrompt}, {});

    permissions::PermissionRequestManager::CreateForWebContents(web_contents());
    auto* manager =
        permissions::PermissionRequestManager::FromWebContents(web_contents());
    manager->set_enabled_app_level_notification_permission_for_testing(true);
    prompt_factory_ =
        std::make_unique<permissions::MockPermissionPromptFactory>(manager);

    // Set the prefs and feature and then register the request manager because
    // this is set up on tab start, but the prefs were not set when the test is
    // created.
    csd_host_->RegisterPermissionRequestManager();
    manager->clear_permission_ui_selector_for_testing();
  }

  void TearDown() override {
    prompt_factory_.reset();
    ClientSideDetectionHostTest::TearDown();
  }

  void PhishingDetectionDone(mojo_base::ProtoWrapper verdict) {
    csd_host_->PhishingDetectionDone(
        ClientSideDetectionType::NOTIFICATION_PERMISSION_PROMPT,
        /*is_sample_ping=*/false, /*did_match_high_confidence_allowlist=*/false,
        mojom::PhishingDetectorResult::SUCCESS, std::move(verdict));
  }

  void PhishingDetectionError(mojom::PhishingDetectorResult error) {
    csd_host_->PhishingDetectionDone(
        ClientSideDetectionType::NOTIFICATION_PERMISSION_PROMPT,
        /*is_sample_ping=*/false, /*did_match_high_confidence_allowlist=*/false,
        error, std::nullopt);
  }

  void WaitForBubbleToBeShown() {
    auto* manager =
        permissions::PermissionRequestManager::FromWebContents(web_contents());
    manager->DocumentOnLoadCompletedInPrimaryMainFrame();
    task_environment()->RunUntilIdle();
  }

 protected:
  std::unique_ptr<permissions::MockPermissionPromptFactory> prompt_factory_;
};

TEST_F(ClientSideDetectionHostNotificationTest,
       NotificationPermissionPromptTriggersClassificationRequest) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  base::HistogramTester histogram_tester;

  // First navigate to a page, which should trigger preclassification check.
  GURL url("http://example.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);
  ExpectPreClassificationChecks(
      url, /*is_private=*/&kFalse, /*match_csd_allowlist=*/&kFalse,
      /*get_valid_cached_result=*/&kFalse,
      /*over_phishing_report_limit=*/&kFalse, /*is_local=*/&kFalse);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      ClientSideDetectionType::TRIGGER_MODELS, 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest", 1);

  // Second, create a permission request that's specifically notifications, and
  // add to the request manager, which should also trigger a preclassification
  // check, this will skip the expect call for GetValidCachedResult. In
  // addition, we do not check the cache if the request type was not through
  // trigger model.
  ExpectPreClassificationChecks(
      url, /*is_private=*/&kFalse, /*match_csd_allowlist=*/&kFalse,
      /*get_valid_cached_result=*/nullptr,
      /*over_phishing_report_limit=*/&kFalse, /*is_local=*/&kFalse);

  ClientPhishingRequest verdict;
  verdict.set_client_score(0.8f);

  EXPECT_CALL(*csd_service_,
              SendClientReportPhishingRequest(
                  PartiallyEqualVerdict(verdict), _,
                  "fake_access_token_notification_permission_prompt"));

  // Set up mock call to token fetcher.
  SafeBrowsingTokenFetcher::Callback cb;
  EXPECT_CALL(*raw_token_fetcher_, Start(_)).WillOnce(MoveArg<0>(&cb));
  permissions::MockPermissionRequest::MockPermissionRequestState request_state;
  auto request1 = std::make_unique<permissions::MockPermissionRequest>(
      url, permissions::RequestType::kNotifications,
      permissions::PermissionRequestGestureType::GESTURE,
      request_state.GetWeakPtr());
  auto* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());
  manager->AddRequest(web_contents()->GetPrimaryMainFrame(),
                      std::move(request1));

  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_TRUE(prompt_factory_->RequestTypeSeen(request_state.request_type));
  ASSERT_EQ(prompt_factory_->request_count(), 1);

  // Wait for token fetcher to be called.
  EXPECT_TRUE(Mock::VerifyAndClear(raw_token_fetcher_));

  ASSERT_FALSE(cb.is_null());
  std::move(cb).Run("fake_access_token_notification_permission_prompt");

  manager->Accept();
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(request_state.granted);

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PhishingDetectorResult.NotificationPermissionPrompt",
      1);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      ClientSideDetectionType::NOTIFICATION_PERMISSION_PROMPT, 1);
  // Below histogram checks that there has been two classification requests.
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest", 2);
}

TEST_F(ClientSideDetectionHostNotificationTest,
       NotPhishingVerdictSendsPingFromNotificationPermissionPrompt) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  base::HistogramTester histogram_tester;

  ClientPhishingRequest verdict;
  verdict.set_url("http://example.com/");
  verdict.set_client_score(0.8f);
  verdict.set_is_phishing(false);
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::NOTIFICATION_PERMISSION_PROMPT);

  EXPECT_CALL(*csd_service_,
              SendClientReportPhishingRequest(
                  PartiallyEqualVerdict(verdict), _,
                  "fake_access_token_notification_permission_prompt"));

  // Set up mock call to token fetcher.
  SafeBrowsingTokenFetcher::Callback cb;
  EXPECT_CALL(*raw_token_fetcher_, Start(_)).WillOnce(MoveArg<0>(&cb));

  // Make the call.
  PhishingDetectionDone(mojo_base::ProtoWrapper(verdict));

  // Wait for token fetcher to be called.
  EXPECT_TRUE(Mock::VerifyAndClear(raw_token_fetcher_));

  ASSERT_FALSE(cb.is_null());
  std::move(cb).Run("fake_access_token_notification_permission_prompt");

  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      ClientSideDetectionType::NOTIFICATION_PERMISSION_PROMPT, 1);
}

class ClientSideDetectionRTLookupResponseForceRequestTest
    : public ClientSideDetectionHostTest {
 public:
  ClientSideDetectionRTLookupResponseForceRequestTest() = default;

  void SetUp() override {
    ClientSideDetectionHostTest::SetUp();

    SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);
    SetFeatures({kClientSideDetectionAcceptHCAllowlist}, {});

    AsyncCheckTracker::CreateForWebContents(
        web_contents(),
        /*ui_manager=*/nullptr,
        /*should_sync_checker_check_allowlist=*/false);
    csd_host_->RegisterAsyncCheckTracker();
  }

 protected:
  void SetRTResponseInCacheManager(bool is_enforced) {
    VerdictCacheManager* cache_manager =
        VerdictCacheManagerFactory::GetForProfile(
            Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
    RTLookupResponse response;
    RTLookupResponse::ThreatInfo* new_threat_info = response.add_threat_info();
    new_threat_info->set_verdict_type(RTLookupResponse::ThreatInfo::DANGEROUS);
    new_threat_info->set_threat_type(
        RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING);
    new_threat_info->set_cache_duration_sec(60);
    new_threat_info->set_cache_expression_using_match_type(
        "suspiciousurl.com/");
    new_threat_info->set_cache_expression_match_type(
        RTLookupResponse::ThreatInfo::EXACT_MATCH);

    response.set_client_side_detection_type(
        is_enforced ? safe_browsing::ClientSideDetectionType::FORCE_REQUEST
                    : safe_browsing::ClientSideDetectionType::
                          CLIENT_SIDE_DETECTION_TYPE_UNSPECIFIED);
    cache_manager->CacheRealTimeUrlVerdict(response, base::Time::Now());
  }

  void CompleteAsyncCheck() {
    auto* tracker = AsyncCheckTracker::GetOrCreateForWebContents(
        web_contents(), /*ui_manager=*/nullptr,
        /*should_sync_checker_check_allowlist=*/false);
    auto checker = std::make_unique<UrlCheckerHolder>(
        /*delegate_getter=*/base::NullCallback(), content::FrameTreeNodeId(),
        /*navigation_id=*/0,
        /*web_contents_getter=*/base::NullCallback(),
        /*complete_callback=*/base::NullCallback(),
        /*url_real_time_lookup_enabled=*/false,
        /*can_check_db=*/true,
        /*can_check_high_confidence_allowlist=*/true,
        /*url_lookup_service_metric_suffix=*/"",
        /*url_lookup_service=*/nullptr,
        /*hash_realtime_service=*/nullptr,
        /*hash_realtime_selection=*/
        hash_realtime_utils::HashRealTimeSelection::kNone,
        /*is_async_check=*/true, /*check_allowlist_before_hash_database=*/false,
        SessionID::InvalidValue(), /*referring_app_info=*/std::nullopt);
    tracker->TransferUrlChecker(std::move(checker));
    // all_checks_completed must be set to true to notify
    // ClientSideDetectionHost.
    UrlCheckerHolder::OnCompleteCheckResult result(
        /*proceed=*/true, /*showed_interstitial=*/false,
        /*has_post_commit_interstitial_skipped=*/false,
        SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck,
        /*all_checks_completed=*/true);
    tracker->PendingCheckerCompleted(/*navigation_id=*/0, result);
  }
};

TEST_F(ClientSideDetectionRTLookupResponseForceRequestTest,
       AsyncCheckTrackerTriggersClassificationRequest) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  base::HistogramTester histogram_tester;

  GURL example_url("http://suspiciousurl.com/");

  database_manager_->SetAllowlistLookupDetailsForUrl(example_url, false);

  // First navigate to a page, which should trigger preclassification check.
  ExpectPreClassificationChecks(
      /*url=*/example_url, /*is_private=*/&kFalse,
      /*match_csd_allowlist=*/&kFalse, /*get_valid_cached_result=*/&kFalse,
      /*over_phishing_report_limit=*/&kFalse, /*is_local=*/&kFalse);
  NavigateAndCommit(example_url);
  WaitAndCheckPreClassificationChecks();

  // Force request should not be triggered, because RTLookupResponse hasn't
  // been cached.
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      ClientSideDetectionType::FORCE_REQUEST, 0);

  SetRTResponseInCacheManager(/*is_enforced=*/true);

  // get_valid_cached_result is set to nullptr, because the request type is not
  // TRIGGER_MODELS. Force request triggers also bypass the CSD allowlist.
  ExpectPreClassificationChecks(
      example_url, &kFalse, /*match_csd_allowlist=*/nullptr,
      /*get_valid_cached_result=*/nullptr, &kFalse, &kFalse);
  // This call should trigger preclassification check again.
  CompleteAsyncCheck();

  // Set up mock call to token fetcher.
  SafeBrowsingTokenFetcher::Callback token_cb;
  EXPECT_CALL(*raw_token_fetcher_, Start(_))
      .Times(1)
      .WillRepeatedly(MoveArg<0>(&token_cb));
  task_environment()->RunUntilIdle();

  ClientSideDetectionService::ClientReportPhishingRequestCallback cb;
  // The verdict's is_phishing is false, but we will still send a ping!
  ClientPhishingRequest verdict;
  verdict.set_url(example_url.spec());
  verdict.set_client_score(0.8f);
  verdict.set_is_phishing(false);
  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(
                                 _, _, "fake_access_token_for_force_request"))
      .WillOnce(MoveArg<1>(&cb));

  ASSERT_FALSE(token_cb.is_null());
  std::move(token_cb).Run("fake_access_token_for_force_request");
  task_environment()->RunUntilIdle();

  // Token is now fetched, so we will now callback on
  // ClientReportPhishingRequest.
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));

  task_environment()->RunUntilIdle();

  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      ClientSideDetectionType::FORCE_REQUEST, 1);
  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.ClientSideDetection."
      "AsyncCheckTriggerForceRequestResult",
      ClientSideDetectionHost::AsyncCheckTriggerForceRequestResult::kTriggered,
      1);
}

TEST_F(ClientSideDetectionRTLookupResponseForceRequestTest,
       AsyncCheckTrackerTriggersClassificationRequestOnAllowlistMatch) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  base::HistogramTester histogram_tester;

  GURL example_url("http://suspiciousurl.com/");

  database_manager_->SetAllowlistLookupDetailsForUrl(example_url, false);

  // First navigate to a page, which should trigger preclassification check.
  ExpectPreClassificationChecks(
      /*url=*/example_url, /*is_private=*/&kFalse,
      /*match_csd_allowlist=*/&kFalse, /*get_valid_cached_result=*/&kFalse,
      /*over_phishing_report_limit=*/&kFalse, /*is_local=*/&kFalse);
  NavigateAndCommit(example_url);
  WaitAndCheckPreClassificationChecks();

  // Force request should not be triggered, because RTLookupResponse hasn't
  // been cached.
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      ClientSideDetectionType::FORCE_REQUEST, 0);

  SetRTResponseInCacheManager(/*is_enforced=*/true);

  // Generally, this never happens unless a sampled RTLookupResponse contains
  // the suspicious URL. For the purpose of this test, we will set that there's
  // a match.
  csd_host_->set_high_confidence_allowlist_acceptance_rate_for_testing(1.0f);
  database_manager_->SetAllowlistLookupDetailsForUrl(example_url, true);

  // get_valid_cached_result is set to nullptr, because the request type is not
  // TRIGGER_MODELS. Force request bypasses CSD allowlist check.
  ExpectPreClassificationChecks(
      example_url, &kFalse, /*match_csd_allowlist=*/nullptr,
      /*get_valid_cached_result=*/nullptr, &kFalse, &kFalse);
  // This call should trigger preclassification check again.
  CompleteAsyncCheck();

  // Set up mock call to token fetcher.
  SafeBrowsingTokenFetcher::Callback token_cb;
  EXPECT_CALL(*raw_token_fetcher_, Start(_))
      .Times(1)
      .WillRepeatedly(MoveArg<0>(&token_cb));
  task_environment()->RunUntilIdle();

  ClientSideDetectionService::ClientReportPhishingRequestCallback cb;
  // The verdict's is_phishing is false, but we will still send a ping!
  ClientPhishingRequest verdict;
  verdict.set_url(example_url.spec());
  verdict.set_client_score(0.8f);
  verdict.set_is_phishing(false);
  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(
                                 _, _, "fake_access_token_for_force_request"))
      .WillOnce(MoveArg<1>(&cb));

  ASSERT_FALSE(token_cb.is_null());
  std::move(token_cb).Run("fake_access_token_for_force_request");
  task_environment()->RunUntilIdle();

  // Token is now fetched, so we will now callback on
  // ClientReportPhishingRequest.
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));

  task_environment()->RunUntilIdle();

  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      ClientSideDetectionType::FORCE_REQUEST, 1);
  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.ClientSideDetection."
      "AsyncCheckTriggerForceRequestResult",
      ClientSideDetectionHost::AsyncCheckTriggerForceRequestResult::kTriggered,
      1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.MatchHighConfidenceAllowlist.ForceRequest", 1);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckResult",
      PreClassificationCheckResult::NO_CLASSIFY_MATCH_HC_ALLOWLIST, 0);
}

TEST_F(ClientSideDetectionRTLookupResponseForceRequestTest,
       AsyncCheckTrackerNotTriggerClassificationRequestNoEnforcedPing) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  base::HistogramTester histogram_tester;

  GURL example_url("http://suspiciousurl.com/");

  database_manager_->SetAllowlistLookupDetailsForUrl(example_url, false);

  // First navigate to a page, which should trigger preclassification check.
  ExpectPreClassificationChecks(
      /*url=*/example_url, /*is_private=*/&kFalse,
      /*match_csd_allowlist=*/&kFalse, /*get_valid_cached_result=*/&kFalse,
      /*over_phishing_report_limit=*/&kFalse, /*is_local=*/&kFalse);
  NavigateAndCommit(example_url);
  WaitAndCheckPreClassificationChecks();

  SetRTResponseInCacheManager(/*is_enforced=*/false);

  CompleteAsyncCheck();
  task_environment()->RunUntilIdle();

  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      ClientSideDetectionType::FORCE_REQUEST, 0);
  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.ClientSideDetection."
      "AsyncCheckTriggerForceRequestResult",
      ClientSideDetectionHost::AsyncCheckTriggerForceRequestResult::
          kSkippedNotForced,
      1);
}

TEST_F(ClientSideDetectionRTLookupResponseForceRequestTest,
       AsyncCheckTrackerTriggersClassificationRequestOnLocalModelPhishing) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  base::HistogramTester histogram_tester;

  GURL example_url("http://suspiciousurl.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(example_url, false);
  ExpectPreClassificationChecks(
      /*url=*/example_url, /*is_private=*/&kFalse,
      /*match_csd_allowlist=*/&kFalse, /*get_valid_cached_result=*/&kFalse,
      /*over_phishing_report_limit=*/&kFalse, /*is_local=*/&kFalse);
  NavigateAndCommit(example_url);
  WaitAndCheckPreClassificationChecks();

  SafeBrowsingTokenFetcher::Callback token_cb;
  EXPECT_CALL(*raw_token_fetcher_, Start(_))
      .Times(1)
      .WillRepeatedly(MoveArg<0>(&token_cb));
  ClientSideDetectionService::ClientReportPhishingRequestCallback cb;
  ClientPhishingRequest verdict;
  verdict.set_url(example_url.spec());
  verdict.set_client_score(1.0f);
  verdict.set_is_phishing(true);
  PhishingDetectionDone(mojo_base::ProtoWrapper(verdict));
  task_environment()->RunUntilIdle();

  // Check that the page is phishing and triggers a ping.
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.LocalModelDetectsPhishing", true, 1);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.LocalModelDetectsPhishing.TriggerModel", true, 1);

  SetRTResponseInCacheManager(/*is_enforced=*/true);

  // get_valid_cached_result is set to nullptr, because the request type is not
  // TRIGGER_MODELS. Force request triggers also bypass the CSD allowlist.
  ExpectPreClassificationChecks(
      example_url, &kFalse, /*match_csd_allowlist=*/nullptr,
      /*get_valid_cached_result=*/nullptr, &kFalse, &kFalse);
  // Calling this should trigger preclassification again, because local model
  // phishy verdict is not a condition to skip the async check force request
  // because the force request ping will contain information that the page load
  // request may have missed.
  CompleteAsyncCheck();
  // Set up mock call to token fetcher.
  SafeBrowsingTokenFetcher::Callback token_cb2;
  EXPECT_CALL(*raw_token_fetcher_, Start(_))
      .Times(1)
      .WillRepeatedly(MoveArg<0>(&token_cb2));
  task_environment()->RunUntilIdle();

  ClientSideDetectionService::ClientReportPhishingRequestCallback cb2;
  // The verdict's is_phishing is false, but we will still send a ping!
  ClientPhishingRequest verdict2;
  verdict2.set_url(example_url.spec());
  verdict2.set_client_score(0.8f);
  verdict2.set_is_phishing(false);
  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(
                                 _, _, "fake_access_token_for_force_request"))
      .WillOnce(MoveArg<1>(&cb2));

  ASSERT_FALSE(token_cb2.is_null());
  std::move(token_cb2).Run("fake_access_token_for_force_request");

  // Token is now fetched, so we will now callback on
  // ClientReportPhishingRequest.
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));

  task_environment()->RunUntilIdle();

  // Enforce request should be triggered again, because the local model will
  // think it's phishy, but force request pings can contain other information,
  // such as the LLM information.
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      ClientSideDetectionType::FORCE_REQUEST, 1);
  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.ClientSideDetection."
      "AsyncCheckTriggerForceRequestResult",
      ClientSideDetectionHost::AsyncCheckTriggerForceRequestResult::kTriggered,
      1);
}

TEST_F(
    ClientSideDetectionRTLookupResponseForceRequestTest,
    AsyncCheckTrackerNotTriggerClassificationRequestOnTriggerModelPingConvertedToForceRequest) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  base::HistogramTester histogram_tester;

  GURL example_url("http://suspiciousurl.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(example_url, false);
  ExpectPreClassificationChecks(
      /*url=*/example_url, /*is_private=*/&kFalse,
      /*match_csd_allowlist=*/&kFalse, /*get_valid_cached_result=*/&kFalse,
      /*over_phishing_report_limit=*/&kFalse, /*is_local=*/&kFalse);
  NavigateAndCommit(example_url);
  WaitAndCheckPreClassificationChecks();

  // Set up mock call to token fetcher.
  SafeBrowsingTokenFetcher::Callback token_cb;
  EXPECT_CALL(*raw_token_fetcher_, Start(_))
      .Times(1)
      .WillRepeatedly(MoveArg<0>(&token_cb));
  ClientSideDetectionService::ClientReportPhishingRequestCallback cb;
  ClientPhishingRequest verdict;
  verdict.set_url(example_url.spec());
  verdict.set_client_score(0.5f);
  verdict.set_is_phishing(false);

  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(
                                 PartiallyEqualVerdict(verdict), _,
                                 "fake_access_token_for_force_request"))
      .WillOnce(MoveArg<1>(&cb));
  // Setup RTResponse in cache prior to calling PhishingDetectionDone.
  SetRTResponseInCacheManager(/*is_enforced=*/true);

  PhishingDetectionDone(mojo_base::ProtoWrapper(verdict));
  task_environment()->RunUntilIdle();

  // Wait for token fetcher to be called.
  EXPECT_TRUE(Mock::VerifyAndClear(raw_token_fetcher_));

  ASSERT_FALSE(token_cb.is_null());
  std::move(token_cb).Run("fake_access_token_for_force_request");

  // Check that the page is phishing and triggers a ping.
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      ClientSideDetectionType::FORCE_REQUEST, 1);
  histogram_tester.ExpectBucketCount("SBClientPhishing.RTLookupForceRequest",
                                     true, 1);

  CompleteAsyncCheck();
  task_environment()->RunUntilIdle();

  // Enforce request should NOT be triggered again, because the page load ping
  // has been converted to a force request ping.
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      ClientSideDetectionType::FORCE_REQUEST, 1);
  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.ClientSideDetection."
      "AsyncCheckTriggerForceRequestResult",
      ClientSideDetectionHost::AsyncCheckTriggerForceRequestResult::
          kSkippedTriggerModelsPingSentAsForceRequest,
      1);
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

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);
  ExpectPreClassificationChecks(url, &kFalse, nullptr, nullptr, nullptr,
                                &kFalse);
  EXPECT_CALL(*database_manager_.get(), CheckCsdAllowlistUrl(url, _)).Times(0);
  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();
  fake_phishing_detector_.CheckMessage(&url);
}

TEST_F(ClientSideDetectionHostDebugFeaturesTest,
       SkipsCacheWhenDumpingFeatures) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);
  ExpectPreClassificationChecks(url, &kFalse, nullptr, nullptr, nullptr,
                                &kFalse);
  EXPECT_CALL(*csd_service_, GetValidCachedResult(url, NotNull())).Times(0);
  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();
  fake_phishing_detector_.CheckMessage(&url);
}

TEST_F(ClientSideDetectionHostDebugFeaturesTest,
       SkipsReportLimitWhenDumpingFeatures) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    GTEST_SKIP();

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);
  ExpectPreClassificationChecks(url, &kFalse, nullptr, nullptr, nullptr,
                                &kFalse);
  EXPECT_CALL(*csd_service_, AtPhishingReportLimit()).Times(0);
  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();
  fake_phishing_detector_.CheckMessage(&url);
}

class ClientSideDetectionHostScamDetectionTest
    : public ClientSideDetectionHostTest {
 public:
  ClientSideDetectionHostScamDetectionTest() = default;

  void SetUp() override {
    ClientSideDetectionHostTest::SetUp();
    SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);
    csd_service_->SetOnDeviceAvailabilityForTesting(true);
    database_manager_->SetAllowlistLookupDetailsForUrl(example_url_, false);

    ON_CALL(*raw_token_fetcher_, Start(_))
        .WillByDefault(
            testing::Invoke([&](SafeBrowsingTokenFetcher::Callback callback) {
              std::move(callback).Run("fake_access_token");
            }));
    NavigateAndCommit(example_url_);
  }

  void CacheForcedTriggerInfo(bool has_llama_forced_trigger_info,
                              bool intelligent_scan,
                              const std::string& cache_expression) {
    VerdictCacheManager* cache_manager =
        VerdictCacheManagerFactory::GetForProfile(
            Profile::FromBrowserContext(web_contents()->GetBrowserContext()));

    RTLookupResponse response;

    RTLookupResponse::ThreatInfo* new_threat_info2 = response.add_threat_info();
    new_threat_info2->set_verdict_type(RTLookupResponse::ThreatInfo::DANGEROUS);
    new_threat_info2->set_threat_type(
        RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING);
    new_threat_info2->set_cache_duration_sec(60);
    new_threat_info2->set_cache_expression_using_match_type(cache_expression);
    new_threat_info2->set_cache_expression_match_type(
        RTLookupResponse::ThreatInfo::EXACT_MATCH);

    response.set_client_side_detection_type(
        safe_browsing::ClientSideDetectionType::FORCE_REQUEST);

    if (has_llama_forced_trigger_info) {
      safe_browsing::LlamaForcedTriggerInfo llama_forced_trigger_info;
      safe_browsing::LlamaTriggerRuleInfo* llama_trigger_rule_info =
          llama_forced_trigger_info.add_llama_trigger_rule_infos();
      llama_trigger_rule_info->set_llama_trigger_rule_id(28);
      llama_trigger_rule_info->set_intelligent_scan(intelligent_scan);
      llama_forced_trigger_info.set_trigger_url(cache_expression);
      llama_forced_trigger_info.set_intelligent_scan(intelligent_scan);
      response.mutable_llama_forced_trigger_info()->Swap(
          &llama_forced_trigger_info);
    }

    cache_manager->CacheRealTimeUrlVerdict(response, base::Time::Now());
    EXPECT_EQ(
        static_cast<int>(safe_browsing::ClientSideDetectionType::FORCE_REQUEST),
        cache_manager->GetCachedRealTimeUrlClientSideDetectionType(
            example_url_));
    if (has_llama_forced_trigger_info) {
      safe_browsing::LlamaForcedTriggerInfo cache_llama_forced_trigger_info;
      EXPECT_TRUE(cache_manager->GetCachedRealTimeLlamaForcedTriggerInfo(
          example_url_, &cache_llama_forced_trigger_info));
      // Because llama_forced_trigger_info is not copied, but rather passed by
      // reference, we explicitly check with direct values set to the object
      // above.
      EXPECT_EQ(cache_llama_forced_trigger_info.intelligent_scan(),
                intelligent_scan);
      EXPECT_EQ(static_cast<int>(
                    cache_llama_forced_trigger_info.llama_trigger_rule_infos()
                        .size()),
                1);
      EXPECT_EQ(cache_llama_forced_trigger_info.llama_trigger_rule_infos()
                    .at(0)
                    .llama_trigger_rule_id(),
                28);
      EXPECT_EQ(cache_llama_forced_trigger_info.llama_trigger_rule_infos()
                    .at(0)
                    .intelligent_scan(),
                intelligent_scan);
    }
  }

  void SetInquireOnDeviceModelCallback(bool should_return_response) {
    EXPECT_CALL(*csd_service_, InquireOnDeviceModel(_, _))
        .WillOnce(testing::Invoke(
            [=, this](
                std::string rendered_text,
                base::OnceCallback<void(
                    std::optional<
                        optimization_guide::proto::ScamDetectionResponse>)>
                    callback) {
              if (!should_return_response) {
                std::move(callback).Run(std::nullopt);
                return;
              }
              optimization_guide::proto::ScamDetectionResponse
                  scam_detection_response;
              scam_detection_response.set_brand(example_brand_);
              scam_detection_response.set_intent(example_intent_);
              std::move(callback).Run(scam_detection_response);
            }));
  }

  void SetSendClientReportPhishingRequestCallback(
      bool has_expected_brand_and_intent,
      std::optional<IntelligentScanInfo::NoInfoReason> expected_no_info_reason,
      std::optional<std::string> expected_llama_forced_trigger_info_trigger_url,
      bool returned_is_phishing,
      IntelligentScanVerdict returned_intelligent_scan_verdict) {
    EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(_, _, _))
        .Times(1)
        .WillOnce(testing::Invoke(
            [=, this](
                std::unique_ptr<ClientPhishingRequest> request,
                ClientSideDetectionService::ClientReportPhishingRequestCallback
                    callback,
                const std::string&) {
              if (has_expected_brand_and_intent) {
                EXPECT_EQ(request->intelligent_scan_info().brand(),
                          example_brand_);
                EXPECT_EQ(request->intelligent_scan_info().intent(),
                          example_intent_);
              } else {
                EXPECT_FALSE(request->intelligent_scan_info().has_brand());
                EXPECT_FALSE(request->intelligent_scan_info().has_intent());
              }
              if (expected_no_info_reason.has_value()) {
                EXPECT_EQ(request->intelligent_scan_info().no_info_reason(),
                          expected_no_info_reason.value());
              } else {
                EXPECT_FALSE(
                    request->intelligent_scan_info().has_no_info_reason());
              }
              if (expected_llama_forced_trigger_info_trigger_url.has_value()) {
                EXPECT_EQ(
                    request->llama_forced_trigger_info().trigger_url(),
                    expected_llama_forced_trigger_info_trigger_url.value());
              } else {
                EXPECT_FALSE(
                    request->llama_forced_trigger_info().has_trigger_url());
              }
              std::move(callback).Run(example_url_, returned_is_phishing,
                                      net::HTTP_OK,
                                      returned_intelligent_scan_verdict);
            }));
  }

  void VerifyExpectedCalls() {
    EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
    EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
    EXPECT_TRUE(Mock::VerifyAndClear(raw_token_fetcher_));
  }

  void VerifyGeneralScamDetectionHistograms(
      ClientSideDetectionType expected_request_type,
      std::optional<bool> is_on_device_model_available,
      std::optional<bool> model_has_successful_response,
      std::optional<IntelligentScanVerdict> intelligent_scan_verdict) {
    histogram_tester_.ExpectBucketCount(
        "SBClientPhishing.ClientSideDetectionTypeRequest",
        expected_request_type, 1);
    if (is_on_device_model_available) {
      histogram_tester_.ExpectUniqueSample(
          "SBClientPhishing.IsOnDeviceModelAvailableAtInquiryTime",
          is_on_device_model_available.value(), 1);
      histogram_tester_.ExpectUniqueSample(
          "SBClientPhishing.IsOnDeviceModelAvailableAtInquiryTime." +
              GetRequestTypeName(expected_request_type),
          is_on_device_model_available.value(), 1);
    } else {
      histogram_tester_.ExpectTotalCount(
          "SBClientPhishing.IsOnDeviceModelAvailableAtInquiryTime", 0);
      histogram_tester_.ExpectTotalCount(
          "SBClientPhishing.IsOnDeviceModelAvailableAtInquiryTime." +
              GetRequestTypeName(expected_request_type),
          0);
    }
    if (model_has_successful_response.has_value()) {
      histogram_tester_.ExpectUniqueSample(
          "SBClientPhishing.OnDeviceModelHasSuccessfulResponse",
          model_has_successful_response.value(), 1);
      histogram_tester_.ExpectUniqueSample(
          "SBClientPhishing.OnDeviceModelHasSuccessfulResponse." +
              GetRequestTypeName(expected_request_type),
          model_has_successful_response.value(), 1);
    } else {
      histogram_tester_.ExpectTotalCount(
          "SBClientPhishing.OnDeviceModelHasSuccessfulResponse", 0);
      histogram_tester_.ExpectTotalCount(
          "SBClientPhishing.OnDeviceModelHasSuccessfulResponse." +
              GetRequestTypeName(expected_request_type),
          0);
    }
    if (intelligent_scan_verdict.has_value()) {
      histogram_tester_.ExpectUniqueSample(
          "SBClientPhishing.IntelligentScanVerdict",
          intelligent_scan_verdict.value(), 1);
    } else {
      histogram_tester_.ExpectTotalCount(
          "SBClientPhishing.IntelligentScanVerdict", 0);
    }
  }

  void VerifyForcedTriggerScamDetectionHistograms(
      bool force_request,
      bool has_llama_forced_trigger_info,
      bool intelligent_scan,
      std::optional<bool> redirect_chain_contains_llama_forced_trigger_info) {
    histogram_tester_.ExpectBucketCount("SBClientPhishing.RTLookupForceRequest",
                                        force_request, 1);
    histogram_tester_.ExpectBucketCount(
        "SBClientPhishing.RTLookupForceRequest.HasLlamaForcedTriggerInfo",
        has_llama_forced_trigger_info, 1);
    if (redirect_chain_contains_llama_forced_trigger_info.has_value()) {
      histogram_tester_.ExpectBucketCount(
          "SBClientPhishing.RedirectChainContainsForcedTriggerInfo",
          *redirect_chain_contains_llama_forced_trigger_info, 1);
    }

    if (has_llama_forced_trigger_info) {
      histogram_tester_.ExpectBucketCount(
          "SBClientPhishing.LlamaForcedTriggerInfo.IntelligentScan",
          intelligent_scan, 1);
      histogram_tester_.ExpectBucketCount(
          "SBClientPhishing.LlamaForcedTriggerInfo.LlamaTriggerRuleInfosSize",
          1, 1);
      histogram_tester_.ExpectBucketCount(
          "SBClientPhishing.LlamaForcedTriggerInfo.LlamaTriggerRuleId", 28, 1);
    }
  }

  void PhishingDetectionDone(bool is_phishing,
                             float client_score,
                             ClientSideDetectionType type,
                             bool did_match_high_confidence_allowlist) {
    ClientPhishingRequest verdict;
    verdict.set_url(example_url_.spec());
    verdict.set_client_score(client_score);
    verdict.set_is_phishing(is_phishing);
    csd_host_->PhishingDetectionDone(type,
                                     /*is_sample_ping=*/false,
                                     did_match_high_confidence_allowlist,
                                     mojom::PhishingDetectorResult::SUCCESS,
                                     mojo_base::ProtoWrapper(verdict));
  }

  void SetExampleUrl(GURL example_url) { example_url_ = example_url; }

  base::HistogramTester histogram_tester_;
  GURL example_url_{"http://suspiciousurl.com/"};
  std::string example_brand_ = "Example Brand";
  std::string example_intent_ = "Example Intent";
};

TEST_F(ClientSideDetectionHostScamDetectionTest,
       KeyboardLockRequestTriggersOnDeviceLLMWithEmptyResponse) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  SetFeatures({kClientSideDetectionBrandAndIntentForScamDetection}, {});
  // Because the client side detection type is KEYBOARD_LOCK_REQUESTED, we will
  // call to inquire the on-device model.
  SetInquireOnDeviceModelCallback(/*should_return_response=*/false);
  SetSendClientReportPhishingRequestCallback(
      /*has_expected_brand_and_intent=*/false,
      /*expected_no_info_reason=*/std::nullopt,
      /*expected_llama_forced_trigger_info_trigger_url=*/std::nullopt,
      /*returned_is_phishing=*/false,
      /*returned_intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);

  // Although the score is not phishy at all, we will still inquire the
  // on-device model because the ping is triggered by keyboard lock.
  PhishingDetectionDone(/*is_phishing=*/false, /*client_score=*/0.0f,
                        ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED,
                        /*did_match_high_confidence_allowlist=*/false);

  VerifyExpectedCalls();
  VerifyGeneralScamDetectionHistograms(
      /*expected_request_type=*/ClientSideDetectionType::
          KEYBOARD_LOCK_REQUESTED,
      /*is_on_device_model_available=*/true,
      /*model_has_successful_response=*/false,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);
}

TEST_F(ClientSideDetectionHostScamDetectionTest,
       KeyboardLockRequestTriggersOnDeviceLLMWithFullResponse) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  SetFeatures({kClientSideDetectionBrandAndIntentForScamDetection}, {});
  SetInquireOnDeviceModelCallback(/*should_return_response=*/true);
  SetSendClientReportPhishingRequestCallback(
      /*has_expected_brand_and_intent=*/true,
      /*expected_no_info_reason=*/std::nullopt,
      /*expected_llama_forced_trigger_info_trigger_url=*/std::nullopt,
      /*returned_is_phishing=*/false,
      /*returned_intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);

  PhishingDetectionDone(/*is_phishing=*/false, /*client_score=*/0.0f,
                        ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED,
                        /*did_match_high_confidence_allowlist=*/false);

  VerifyExpectedCalls();
  VerifyGeneralScamDetectionHistograms(
      /*expected_request_type=*/ClientSideDetectionType::
          KEYBOARD_LOCK_REQUESTED,
      /*is_on_device_model_available=*/true,
      /*model_has_successful_response=*/true,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);
}

TEST_F(ClientSideDetectionHostScamDetectionTest,
       EmptyInnerTextDoesNotTriggersOnDeviceLLM) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  SetFeatures({kClientSideDetectionBrandAndIntentForScamDetection}, {});
  raw_delegate_->ForceEmptyInnerText();
  // Because the inner text is empty, we will NOT inquire the on-device model.
  EXPECT_CALL(*csd_service_, LogOnDeviceModelEligibilityReason()).Times(0);
  EXPECT_CALL(*csd_service_, InquireOnDeviceModel(_, _)).Times(0);
  SetSendClientReportPhishingRequestCallback(
      /*has_expected_brand_and_intent=*/false,
      /*expected_no_info_reason=*/IntelligentScanInfo::EMPTY_TEXT,
      /*expected_llama_forced_trigger_info_trigger_url=*/std::nullopt,
      /*returned_is_phishing=*/false,
      /*returned_intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);

  PhishingDetectionDone(/*is_phishing=*/false, /*client_score=*/0.0f,
                        ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED,
                        /*did_match_high_confidence_allowlist=*/false);

  VerifyExpectedCalls();
  VerifyGeneralScamDetectionHistograms(
      /*expected_request_type=*/ClientSideDetectionType::
          KEYBOARD_LOCK_REQUESTED,
      /*is_on_device_model_available=*/true,
      /*model_has_successful_response=*/std::nullopt,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);
}

TEST_F(ClientSideDetectionHostScamDetectionTest,
       AllowlistedOnHCDoesNotTriggersOnDeviceLLM) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  SetFeatures({kClientSideDetectionBrandAndIntentForScamDetection}, {});
  // Because the URL is on the HC allowlist, we will NOT inquire the
  // on-device model.
  EXPECT_CALL(*csd_service_, LogOnDeviceModelEligibilityReason()).Times(0);
  EXPECT_CALL(*csd_service_, InquireOnDeviceModel(_, _)).Times(0);
  SetSendClientReportPhishingRequestCallback(
      /*has_expected_brand_and_intent=*/false,
      /*expected_no_info_reason=*/IntelligentScanInfo::ALLOWLISTED,
      /*expected_llama_forced_trigger_info_trigger_url=*/std::nullopt,
      /*returned_is_phishing=*/false,
      /*returned_intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);

  PhishingDetectionDone(/*is_phishing=*/false, /*client_score=*/0.0f,
                        ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED,
                        /*did_match_high_confidence_allowlist=*/true);

  VerifyExpectedCalls();
  // Allowlisted page does not check whether the on-device model is available,
  // because it exists through the allowlist check beforehand.
  VerifyGeneralScamDetectionHistograms(
      /*expected_request_type=*/ClientSideDetectionType::
          KEYBOARD_LOCK_REQUESTED,
      /*is_on_device_model_available=*/std::nullopt,
      /*model_has_successful_response=*/std::nullopt,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);
}

TEST_F(ClientSideDetectionHostScamDetectionTest,
       NoOnDeviceModelDoesNotTriggersOnDeviceLLM) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  SetFeatures({kClientSideDetectionBrandAndIntentForScamDetection}, {});
  csd_service_->SetOnDeviceAvailabilityForTesting(false);
  // Because the on-device model is unavailable, we will NOT inquire the
  // on-device model.
  EXPECT_CALL(*csd_service_, LogOnDeviceModelEligibilityReason()).Times(1);
  EXPECT_CALL(*csd_service_, InquireOnDeviceModel(_, _)).Times(0);
  SetSendClientReportPhishingRequestCallback(
      /*has_expected_brand_and_intent=*/false,
      /*expected_no_info_reason=*/
      IntelligentScanInfo::ON_DEVICE_MODEL_UNAVAILABLE,
      /*expected_llama_forced_trigger_info_trigger_url=*/std::nullopt,
      /*returned_is_phishing=*/false,
      /*returned_intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);

  PhishingDetectionDone(/*is_phishing=*/false, /*client_score=*/0.0f,
                        ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED,
                        /*did_match_high_confidence_allowlist=*/false);

  VerifyExpectedCalls();
  VerifyGeneralScamDetectionHistograms(
      /*expected_request_type=*/ClientSideDetectionType::
          KEYBOARD_LOCK_REQUESTED,
      /*is_on_device_model_available=*/false,
      /*model_has_successful_response=*/std::nullopt,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);
}

TEST_F(ClientSideDetectionHostScamDetectionTest,
       NonKeyboardLockRequestDoesNotTriggersOnDeviceLLM) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  SetFeatures({kClientSideDetectionBrandAndIntentForScamDetection}, {});

  // Because the client side detection type is POINTER_LOCK_REQUESTED, we will
  // NOT inquire the on-device model.
  EXPECT_CALL(*csd_service_, LogOnDeviceModelEligibilityReason()).Times(0);
  EXPECT_CALL(*csd_service_, InquireOnDeviceModel(_, _)).Times(0);
  SetSendClientReportPhishingRequestCallback(
      /*has_expected_brand_and_intent=*/false,
      /*expected_no_info_reason=*/std::nullopt,
      /*expected_llama_forced_trigger_info_trigger_url=*/std::nullopt,
      /*returned_is_phishing=*/false,
      /*returned_intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);

  PhishingDetectionDone(/*is_phishing=*/false, /*client_score=*/0.0f,
                        ClientSideDetectionType::POINTER_LOCK_REQUESTED,
                        /*did_match_high_confidence_allowlist=*/false);

  VerifyExpectedCalls();
  // Because the request is non-keyboard lock request, we don't check for
  // on-device model availability.
  VerifyGeneralScamDetectionHistograms(
      /*expected_request_type=*/ClientSideDetectionType::POINTER_LOCK_REQUESTED,
      /*is_on_device_model_available=*/std::nullopt,
      /*model_has_successful_response=*/std::nullopt,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);
}

TEST_F(
    ClientSideDetectionHostScamDetectionTest,
    ScamExperimentVerdictOnClientPhishingResponseButDoesntShowBlockingPageDueToDisabledFlag) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  SetFeatures({kClientSideDetectionBrandAndIntentForScamDetection},
              {kClientSideDetectionShowScamVerdictWarning});
  SetInquireOnDeviceModelCallback(/*should_return_response=*/true);
  SetSendClientReportPhishingRequestCallback(
      /*has_expected_brand_and_intent=*/true,
      /*expected_no_info_reason=*/std::nullopt,
      /*expected_llama_forced_trigger_info_trigger_url=*/std::nullopt,
      /*returned_is_phishing=*/false,
      /*returned_intelligent_scan_verdict=*/
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_1);
  // We do not expect the blocking page to pop up because
  // kClientSideDetectionShowScamVerdictWarning is disabled.
  EXPECT_CALL(*ui_manager_.get(), DisplayBlockingPage(_)).Times(0);

  PhishingDetectionDone(/*is_phishing=*/false, /*client_score=*/0.0f,
                        ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED,
                        /*did_match_high_confidence_allowlist=*/false);

  VerifyExpectedCalls();
  VerifyGeneralScamDetectionHistograms(
      /*expected_request_type=*/ClientSideDetectionType::
          KEYBOARD_LOCK_REQUESTED,
      /*is_on_device_model_available=*/true,
      /*model_has_successful_response=*/true,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_1);
}

TEST_F(ClientSideDetectionHostScamDetectionTest,
       ScamExperimentVerdictOnClientPhishingResponseAndShowBlockingPage) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  SetFeatures({kClientSideDetectionBrandAndIntentForScamDetection,
               kClientSideDetectionShowScamVerdictWarning},
              {});
  SetInquireOnDeviceModelCallback(/*should_return_response=*/true);
  SetSendClientReportPhishingRequestCallback(
      /*has_expected_brand_and_intent=*/true,
      /*expected_no_info_reason=*/std::nullopt,
      /*expected_llama_forced_trigger_info_trigger_url=*/std::nullopt,
      /*returned_is_phishing=*/false,
      /*returned_intelligent_scan_verdict=*/
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_1);
  // Now we run the callback to receive a server response. We do expect the
  // blocking page to pop up on a non-phishy response with the scam experiment
  // verdict because the feature is now enabled despite the is_phishy field is
  // false.
  UnsafeResource resource;
  resource.threat_subtype = ThreatSubtype::SCAM_EXPERIMENT_VERDICT_1;
  EXPECT_CALL(*ui_manager_.get(),
              DisplayBlockingPage(HasScamThreatSubtype(resource)))
      .Times(1);

  PhishingDetectionDone(
      /*is_phishing=*/false, /*client_score=*/0.0f,
      ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED,
      /*did_match_high_confidence_allowlist=*/false);

  VerifyExpectedCalls();
  VerifyGeneralScamDetectionHistograms(
      /*expected_request_type=*/ClientSideDetectionType::
          KEYBOARD_LOCK_REQUESTED,
      /*is_on_device_model_available=*/true,
      /*model_has_successful_response=*/true,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_1);
}

TEST_F(ClientSideDetectionHostScamDetectionTest,
       RTLookupResponseLlamaForcedTriggerInfoTriggersOnDeviceLLM) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  SetFeatures({kClientSideDetectionBrandAndIntentForScamDetection,
               kClientSideDetectionSendLlamaForcedTriggerInfo,
               kClientSideDetectionLlamaForcedTriggerInfoForScamDetection},
              {});
  CacheForcedTriggerInfo(
      /*has_llama_forced_trigger_info=*/true,
      /*intelligent_scan=*/true,
      /*cache_expression=*/example_url_.GetContent());
  SetInquireOnDeviceModelCallback(/*should_return_response=*/true);
  SetSendClientReportPhishingRequestCallback(
      /*has_expected_brand_and_intent=*/true,
      /*expected_no_info_reason=*/std::nullopt,
      /*expected_llama_forced_trigger_info_trigger_url=*/
      example_url_.GetContent(),
      /*returned_is_phishing=*/false,
      /*returned_intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);

  // Although the phishing detection done is set to TRIGGER_MODELS, it will
  // eventually switch to FORCE_REQUEST because the verdict cache manager
  // contains a suspicious RTLookupResponse.
  PhishingDetectionDone(/*is_phishing=*/false, /*client_score=*/0.8f,
                        ClientSideDetectionType::TRIGGER_MODELS,
                        /*did_match_high_confidence_allowlist=*/false);

  VerifyExpectedCalls();
  VerifyGeneralScamDetectionHistograms(
      /*expected_request_type=*/ClientSideDetectionType::FORCE_REQUEST,
      /*is_on_device_model_available=*/true,
      /*model_has_successful_response=*/true,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);
  VerifyForcedTriggerScamDetectionHistograms(
      /*force_request=*/true, /*has_llama_forced_trigger_info=*/true,
      /*intelligent_scan=*/true,
      /*redirect_chain_contains_llama_forced_trigger_info=*/std::nullopt);
}

TEST_F(ClientSideDetectionHostScamDetectionTest,
       RTLookupResponseHaveFalseIntelligentScanSoItDoesNotTriggersOnDeviceLLM) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  SetFeatures({kClientSideDetectionBrandAndIntentForScamDetection,
               kClientSideDetectionSendLlamaForcedTriggerInfo,
               kClientSideDetectionLlamaForcedTriggerInfoForScamDetection},
              {});
  CacheForcedTriggerInfo(
      /*has_llama_forced_trigger_info=*/true,
      /*intelligent_scan=*/false,
      /*cache_expression=*/example_url_.GetContent());
  // Because the RTLookupResponse does contain the LlamaForcedTriggerInfo but
  // intelligent_scan field is set to false, we will not inquire the on device
  // model.
  EXPECT_CALL(*csd_service_, InquireOnDeviceModel(_, _)).Times(0);
  SetSendClientReportPhishingRequestCallback(
      /*has_expected_brand_and_intent=*/false,
      /*expected_no_info_reason=*/std::nullopt,
      /*expected_llama_forced_trigger_info_trigger_url=*/
      example_url_.GetContent(),
      /*returned_is_phishing=*/false,
      /*returned_intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);

  // Although the phishing detection done is set to TRIGGER_MODELS, it will
  // eventually switch to FORCE_REQUEST because the verdict cache manager
  // contains a suspicious RTLookupResponse.
  PhishingDetectionDone(/*is_phishing=*/false, /*client_score=*/0.8f,
                        ClientSideDetectionType::TRIGGER_MODELS,
                        /*did_match_high_confidence_allowlist=*/false);

  VerifyExpectedCalls();
  // We do not check for on-device model availability if LLAMA force request
  // does not request it initially.
  VerifyGeneralScamDetectionHistograms(
      /*expected_request_type=*/ClientSideDetectionType::FORCE_REQUEST,
      /*is_on_device_model_available=*/std::nullopt,
      /*model_has_successful_response=*/std::nullopt,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);
  VerifyForcedTriggerScamDetectionHistograms(
      /*force_request=*/true, /*has_llama_forced_trigger_info=*/true,
      /*intelligent_scan=*/false,
      /*redirect_chain_contains_llama_forced_trigger_info=*/std::nullopt);
}

TEST_F(
    ClientSideDetectionHostScamDetectionTest,
    RTLookupResponseDoesNotHaveLlamaForcedTriggerInfoSoItDoesNotTriggersOnDeviceLLM) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  SetFeatures({kClientSideDetectionBrandAndIntentForScamDetection,
               kClientSideDetectionSendLlamaForcedTriggerInfo,
               kClientSideDetectionLlamaForcedTriggerInfoForScamDetection},
              {});
  CacheForcedTriggerInfo(
      /*has_llama_forced_trigger_info=*/false,
      /*intelligent_scan=*/false,
      /*cache_expression=*/example_url_.GetContent());
  // Because the RTLookupResponse does not contain the LlamaForcedTriggerInfo at
  // all and it wasn't found in the cache, we will not inquire the on device
  // model.
  EXPECT_CALL(*csd_service_, InquireOnDeviceModel(_, _)).Times(0);
  SetSendClientReportPhishingRequestCallback(
      /*has_expected_brand_and_intent=*/false,
      /*expected_no_info_reason=*/std::nullopt,
      /*expected_llama_forced_trigger_info_trigger_url=*/std::nullopt,
      /*returned_is_phishing=*/false,
      /*returned_intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);

  // Although the phishing detection done is set to TRIGGER_MODELS, it will
  // eventually switch to FORCE_REQUEST because the verdict cache manager
  // contains a suspicious RTLookupResponse.
  PhishingDetectionDone(/*is_phishing=*/false, /*client_score=*/0.8f,
                        ClientSideDetectionType::TRIGGER_MODELS,
                        /*did_match_high_confidence_allowlist=*/false);

  VerifyExpectedCalls();
  VerifyGeneralScamDetectionHistograms(
      /*expected_request_type=*/ClientSideDetectionType::FORCE_REQUEST,
      /*is_on_device_model_available=*/std::nullopt,
      /*model_has_successful_response=*/std::nullopt,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);
  VerifyForcedTriggerScamDetectionHistograms(
      /*force_request=*/true, /*has_llama_forced_trigger_info=*/false,
      /*intelligent_scan=*/false,
      /*redirect_chain_contains_llama_forced_trigger_info=*/std::nullopt);
}

TEST_F(
    ClientSideDetectionHostScamDetectionTest,
    RedirectChainContainsRTLookupResponseLlamaForcedTriggerInfoSoItTriggersOnDeviceLLM) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  SetFeatures({kClientSideDetectionSendLlamaForcedTriggerInfo,
               kClientSideDetectionLlamaForcedTriggerInfoForScamDetection},
              {});

  GURL first_url_redirect("http://firsturlsuspicious.com/");
  GURL second_url_redirect("http://secondurlnotsuspicious.com/");
  GURL third_url_redirect("http://thirdurlnotsuspicious.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(first_url_redirect, false);
  database_manager_->SetAllowlistLookupDetailsForUrl(second_url_redirect,
                                                     false);
  database_manager_->SetAllowlistLookupDetailsForUrl(third_url_redirect, false);

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      first_url_redirect, web_contents());
  navigation->Start();
  navigation->Redirect(second_url_redirect);
  navigation->Redirect(third_url_redirect);
  navigation->Commit();

  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();

  ASSERT_TRUE(entry);
  EXPECT_EQ(entry->GetRedirectChain().size(), 3u);

  // Set the example url to the first in the redirect chain so that the cache is
  // done for the first URL.
  SetExampleUrl(first_url_redirect);
  CacheForcedTriggerInfo(/*has_llama_forced_trigger_info=*/true,
                         /*intelligent_scan=*/true,
                         /*cache_expression=*/first_url_redirect.GetContent());

  SetInquireOnDeviceModelCallback(/*should_return_response=*/true);

  // Re-set the example URL to the final url in the redirect chain.
  SetExampleUrl(third_url_redirect);
  SetSendClientReportPhishingRequestCallback(
      /*has_expected_brand_and_intent=*/true,
      /*expected_no_info_reason=*/std::nullopt,
      /*expected_llama_forced_trigger_info_trigger_url=*/
      first_url_redirect.GetContent(),
      /*returned_is_phishing=*/false,
      /*returned_intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);

  // Although the phishing detection done is set to TRIGGER_MODELS, it will
  // eventually switch to FORCE_REQUEST because the verdict cache manager
  // contains a suspicious RTLookupResponse.
  PhishingDetectionDone(/*is_phishing=*/false, /*client_score=*/0.8f,
                        ClientSideDetectionType::TRIGGER_MODELS,
                        /*did_match_high_confidence_allowlist=*/false);

  VerifyExpectedCalls();
  VerifyGeneralScamDetectionHistograms(
      /*expected_request_type=*/ClientSideDetectionType::FORCE_REQUEST,
      /*is_on_device_model_available=*/true,
      /*model_has_successful_response=*/true,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);
  VerifyForcedTriggerScamDetectionHistograms(
      /*force_request=*/true, /*has_llama_forced_trigger_info=*/true,
      /*intelligent_scan=*/true,
      /*redirect_chain_contains_llama_forced_trigger_info=*/true);
}

TEST_F(ClientSideDetectionHostScamDetectionTest,
       RedirectChainDoesNotContainRTLookupResponseLlamaForcedTriggerInfo) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  SetFeatures({kClientSideDetectionSendLlamaForcedTriggerInfo,
               kClientSideDetectionLlamaForcedTriggerInfoForScamDetection},
              {});

  GURL first_url_redirect("http://firsturlnotsuspicious.com/");
  GURL second_url_redirect("http://secondurlnotsuspicious.com/");
  GURL third_url_redirect("http://thirdurlnotsuspicious.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(first_url_redirect, false);
  database_manager_->SetAllowlistLookupDetailsForUrl(second_url_redirect,
                                                     false);
  database_manager_->SetAllowlistLookupDetailsForUrl(third_url_redirect, false);

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      first_url_redirect, web_contents());
  navigation->Start();
  navigation->Redirect(second_url_redirect);
  navigation->Redirect(third_url_redirect);
  navigation->Commit();

  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();

  ASSERT_TRUE(entry);
  EXPECT_EQ(entry->GetRedirectChain().size(), 3u);

  // Set the example url to the first in the redirect chain so that the cache is
  // done for the first URL. The force request will exist, but the
  // LlamaForcedTriggerInfo will not.
  SetExampleUrl(first_url_redirect);
  CacheForcedTriggerInfo(/*has_llama_forced_trigger_info=*/false,
                         /*intelligent_scan=*/false,
                         /*cache_expression=*/first_url_redirect.GetContent());

  // Re-set the example URL to the final url in the redirect chain.
  SetExampleUrl(third_url_redirect);

  // Because there is no forced trigger info in the first URL in the referrer
  // chain either, there won't be any on-device model calls.
  SetSendClientReportPhishingRequestCallback(
      /*has_expected_brand_and_intent=*/false,
      /*expected_no_info_reason=*/std::nullopt,
      /*expected_llama_forced_trigger_info_trigger_url=*/std::nullopt,
      /*returned_is_phishing=*/false,
      /*returned_intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);

  // Although the phishing detection done is set to TRIGGER_MODELS, it will
  // eventually switch to FORCE_REQUEST because the verdict cache manager
  // contains a suspicious RTLookupResponse.
  PhishingDetectionDone(/*is_phishing=*/false, /*client_score=*/0.8f,
                        ClientSideDetectionType::TRIGGER_MODELS,
                        /*did_match_high_confidence_allowlist=*/false);

  VerifyExpectedCalls();
  VerifyGeneralScamDetectionHistograms(
      /*expected_request_type=*/ClientSideDetectionType::FORCE_REQUEST,
      /*is_on_device_model_available=*/std::nullopt,
      /*model_has_successful_response=*/std::nullopt,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);
  VerifyForcedTriggerScamDetectionHistograms(
      /*force_request=*/true,
      /*has_llama_forced_trigger_info=*/false,
      /*intelligent_scan=*/false,
      /*redirect_chain_contains_llama_forced_trigger_info=*/false);
}

TEST_F(
    ClientSideDetectionHostScamDetectionTest,
    RedirectChainDoesContainRTLookupResponseLlamaForcedTriggerInfoButKillswitchIsEnabled) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  SetFeatures({kClientSideDetectionForcedLlamaRedirectChainKillswitch,
               kClientSideDetectionSendLlamaForcedTriggerInfo,
               kClientSideDetectionLlamaForcedTriggerInfoForScamDetection},
              {});

  GURL first_url_redirect("http://firsturlnotsuspicious.com/");
  GURL second_url_redirect("http://secondurlnotsuspicious.com/");
  GURL third_url_redirect("http://thirdurlnotsuspicious.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(first_url_redirect, false);
  database_manager_->SetAllowlistLookupDetailsForUrl(second_url_redirect,
                                                     false);
  database_manager_->SetAllowlistLookupDetailsForUrl(third_url_redirect, false);

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      first_url_redirect, web_contents());
  navigation->Start();
  navigation->Redirect(second_url_redirect);
  navigation->Redirect(third_url_redirect);
  navigation->Commit();

  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();

  ASSERT_TRUE(entry);
  EXPECT_EQ(entry->GetRedirectChain().size(), 3u);

  // Set the example url to the first in the redirect chain so that the cache is
  // done for the first URL. The force request will exist and the
  // LlamaForcedTriggerInfo as well, but due to killswitch, it won't matter.
  SetExampleUrl(first_url_redirect);
  CacheForcedTriggerInfo(/*has_llama_forced_trigger_info=*/true,
                         /*intelligent_scan=*/true,
                         /*cache_expression=*/first_url_redirect.GetContent());

  // Re-set the example URL to the final url in the redirect chain.
  SetExampleUrl(third_url_redirect);

  // There is a LlamaForcedTriggerInfo, but due to the killswitch, there won't
  // be any on-device model calls.
  SetSendClientReportPhishingRequestCallback(
      /*has_expected_brand_and_intent=*/false,
      /*expected_no_info_reason=*/std::nullopt,
      /*expected_llama_forced_trigger_info_trigger_url=*/std::nullopt,
      /*returned_is_phishing=*/false,
      /*returned_intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);

  // Although the phishing detection done is set to TRIGGER_MODELS, it will
  // eventually switch to FORCE_REQUEST because the verdict cache manager
  // contains a suspicious RTLookupResponse.
  PhishingDetectionDone(/*is_phishing=*/false, /*client_score=*/0.8f,
                        ClientSideDetectionType::TRIGGER_MODELS,
                        /*did_match_high_confidence_allowlist=*/false);

  VerifyExpectedCalls();
  VerifyGeneralScamDetectionHistograms(
      /*expected_request_type=*/ClientSideDetectionType::FORCE_REQUEST,
      /*is_on_device_model_available=*/std::nullopt,
      /*model_has_successful_response=*/std::nullopt,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);

  // Due to the killswitch, there will be no LlamaForcedTriggerInfo found in the
  // redirect chain.
  VerifyForcedTriggerScamDetectionHistograms(
      /*force_request=*/true,
      /*has_llama_forced_trigger_info=*/false,
      /*intelligent_scan=*/true,
      /*redirect_chain_contains_llama_forced_trigger_info=*/std::nullopt);
}

TEST_F(
    ClientSideDetectionHostTest,
    FullscreenApiCallChecksAllowlistInPreClassificationAndDoesNotProceedWithClassification) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  std::vector<base::test::FeatureRef> enabled_features = {};
  enabled_features.push_back(kClientSideDetectionAcceptHCAllowlist);
  SetFeatures(enabled_features, {});

  csd_host_->set_high_confidence_allowlist_acceptance_rate_for_testing(1.0f);
  base::HistogramTester histogram_tester;

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/true);
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, nullptr,
                                nullptr);
  NavigateAndKeepLoading(web_contents(), url);
  WaitAndCheckPreClassificationChecks();

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.MatchHighConfidenceAllowlist.TriggerModel", 1);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckResult",
      PreClassificationCheckResult::NO_CLASSIFY_MATCH_HC_ALLOWLIST, 1);

  ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, nullptr,
                                nullptr);
  csd_host_->DidToggleFullscreenModeForTab(false, false);
  WaitAndCheckPreClassificationChecks();

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.MatchCSDAllowlistOnFullscreenApi", 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.MatchHighConfidenceAllowlist.FullscreenApi", 1);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckResult.FullscreenApi",
      PreClassificationCheckResult::NO_CLASSIFY_ALLOWLIST_METRIC, 1);
}

TEST_F(ClientSideDetectionHostTest,
       TwoFullscreenApiTriggersOnSamePageOnlyLogsOnePreclassificationCheck) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  base::HistogramTester histogram_tester;

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/true);
  ExpectPreClassificationChecks(url, nullptr, nullptr, nullptr, nullptr,
                                nullptr);
  csd_host_->DidToggleFullscreenModeForTab(false, false);
  WaitAndCheckPreClassificationChecks();

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.FullscreenApi", 1);

  // We do not expect preclassification checks this time because we've done it
  // already on the same page.
  csd_host_->DidToggleFullscreenModeForTab(false, false);

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.FullscreenApi", 1);
}

TEST_F(ClientSideDetectionHostTest,
       TwoKeyboardLockRequestsOnSamePageOnlyLogsOnePreclassificationCheck) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  base::HistogramTester histogram_tester;

  GURL url("http://host3.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, true);

  // Keyboard lock request incoming, which triggers preclassification checks.
  ExpectPreClassificationChecks(
      /*url=*/url, /*is_private=*/&kFalse,
      /*match_csd_allowlist=*/nullptr, /*get_valid_cached_result=*/nullptr,
      /*over_phishing_report_limit=*/nullptr, /*is_local=*/nullptr);

  csd_host_->KeyboardLockRequested();
  WaitAndCheckPreClassificationChecks();

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.KeyboardLockRequested", 1);

  // We trigger keyboard lock again, but because we're still on the same page,
  // we do not trigger preclassification again.
  csd_host_->KeyboardLockRequested();

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.KeyboardLockRequested", 1);
}

TEST_F(
    ClientSideDetectionHostScamDetectionTest,
    RTLookupResponseLlamaForcedTriggerInfoTriggersOnDeviceLLMAndShowWarning) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  SetFeatures({kClientSideDetectionSendLlamaForcedTriggerInfo,
               kClientSideDetectionLlamaForcedTriggerInfoForScamDetection,
               kClientSideDetectionShowLlamaScamVerdictWarning},
              {});
  CacheForcedTriggerInfo(
      /*has_llama_forced_trigger_info=*/true,
      /*intelligent_scan=*/true,
      /*cache_expression=*/example_url_.GetContent());
  SetInquireOnDeviceModelCallback(/*should_return_response=*/true);
  SetSendClientReportPhishingRequestCallback(
      /*has_expected_brand_and_intent=*/true,
      /*expected_no_info_reason=*/std::nullopt,
      /*expected_llama_forced_trigger_info_trigger_url=*/
      example_url_.GetContent(),
      /*returned_is_phishing=*/false,
      /*returned_intelligent_scan_verdict=*/
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_2);

  UnsafeResource resource;
  resource.threat_subtype = ThreatSubtype::SCAM_EXPERIMENT_VERDICT_2;
  // We do expect the blocking page to pop up on a non-phishy response with the
  // scam experiment verdict because
  // kClientSideDetectionShowLlamaScamVerdictWarning is now enabled despite the
  // is_phishy field is false.
  EXPECT_CALL(*ui_manager_.get(),
              DisplayBlockingPage(HasScamThreatSubtype(resource)))
      .Times(1);

  // Although the phishing detection done is set to TRIGGER_MODELS, it will
  // eventually switch to FORCE_REQUEST because the verdict cache manager
  // contains a suspicious RTLookupResponse.
  PhishingDetectionDone(/*is_phishing=*/false, /*client_score=*/0.8f,
                        ClientSideDetectionType::TRIGGER_MODELS,
                        /*did_match_high_confidence_allowlist=*/false);

  VerifyExpectedCalls();
  VerifyGeneralScamDetectionHistograms(
      /*expected_request_type=*/ClientSideDetectionType::FORCE_REQUEST,
      /*is_on_device_model_available=*/true,
      /*model_has_successful_response=*/true,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_2);
  VerifyForcedTriggerScamDetectionHistograms(
      /*force_request=*/true, /*has_llama_forced_trigger_info=*/true,
      /*intelligent_scan=*/true,
      /*redirect_chain_contains_llama_forced_trigger_info=*/std::nullopt);
}

TEST_F(
    ClientSideDetectionHostScamDetectionTest,
    RTLookupResponseLlamaForcedTriggerInfoTriggersOnDeviceLLMButNotShowWarningDueToDisabledStudy) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  SetFeatures({kClientSideDetectionSendLlamaForcedTriggerInfo,
               kClientSideDetectionLlamaForcedTriggerInfoForScamDetection},
              {kClientSideDetectionShowLlamaScamVerdictWarning});

  CacheForcedTriggerInfo(
      /*has_llama_forced_trigger_info=*/true,
      /*intelligent_scan=*/true,
      /*cache_expression=*/example_url_.GetContent());
  SetInquireOnDeviceModelCallback(/*should_return_response=*/true);
  SetSendClientReportPhishingRequestCallback(
      /*has_expected_brand_and_intent=*/true,
      /*expected_no_info_reason=*/std::nullopt,
      /*expected_llama_forced_trigger_info_trigger_url=*/
      example_url_.GetContent(),
      /*returned_is_phishing=*/false,
      /*returned_intelligent_scan_verdict=*/
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_2);
  // Now we run the callback to receive a server response. Because the study is
  // disabled, we do NOT expect the blocking page to pop up on a non-phishy
  // response with the scam experiment verdict.
  EXPECT_CALL(*ui_manager_.get(), DisplayBlockingPage(_)).Times(0);

  // Although the phishing detection done is set to TRIGGER_MODELS, it will
  // eventually switch to FORCE_REQUEST because the verdict cache manager
  // contains a suspicious RTLookupResponse.
  PhishingDetectionDone(/*is_phishing=*/false, /*client_score=*/0.8f,
                        ClientSideDetectionType::TRIGGER_MODELS,
                        /*did_match_high_confidence_allowlist=*/false);

  VerifyExpectedCalls();
  VerifyGeneralScamDetectionHistograms(
      /*expected_request_type=*/ClientSideDetectionType::FORCE_REQUEST,
      /*is_on_device_model_available=*/true,
      /*model_has_successful_response=*/true,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_2);
  VerifyForcedTriggerScamDetectionHistograms(
      /*force_request=*/true, /*has_llama_forced_trigger_info=*/true,
      /*intelligent_scan=*/true,
      /*redirect_chain_contains_llama_forced_trigger_info=*/std::nullopt);
}

TEST_F(ClientSideDetectionHostScamDetectionTest,
       CatchAllScamExperimentVerdictDoesNotShowWarning) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  SetFeatures({kClientSideDetectionBrandAndIntentForScamDetection,
               kClientSideDetectionSendLlamaForcedTriggerInfo,
               kClientSideDetectionLlamaForcedTriggerInfoForScamDetection,
               kClientSideDetectionShowScamVerdictWarning,
               kClientSideDetectionShowLlamaScamVerdictWarning},
              {});

  EXPECT_CALL(*csd_service_, InquireOnDeviceModel(_, _)).Times(0);
  SetSendClientReportPhishingRequestCallback(
      /*has_expected_brand_and_intent=*/false,
      /*expected_no_info_reason=*/std::nullopt,
      /*expected_llama_forced_trigger_info_trigger_url=*/std::nullopt,
      /*returned_is_phishing=*/false,
      /*returned_intelligent_scan_verdict=*/
      IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_TELEMETRY);
  // Because the callback responds with the catch all verdict, we will not show
  // a warning.
  EXPECT_CALL(*ui_manager_.get(), DisplayBlockingPage(_)).Times(0);

  // The verdict's is_phishing is true, so that we send a ping and await a
  // response.
  PhishingDetectionDone(/*is_phishing=*/true, /*client_score=*/0.8f,
                        ClientSideDetectionType::TRIGGER_MODELS,
                        /*did_match_high_confidence_allowlist=*/false);

  VerifyExpectedCalls();
  // Although the warning has not been displayed, we should still check that the
  // histogram has been logged. In addition, because the client side detection
  // type is TRIGGER_MODELS, we do not check for on device model availability.
  VerifyGeneralScamDetectionHistograms(
      /*expected_request_type=*/ClientSideDetectionType::TRIGGER_MODELS,
      /*is_on_device_model_available=*/std::nullopt,
      /*model_has_successful_response=*/std::nullopt,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_TELEMETRY);
}

TEST_F(ClientSideDetectionHostScamDetectionTest,
       CatchAllEnforcementScamExperimentVerdictDoesNotShowWarningDueToStudies) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  SetFeatures({kClientSideDetectionBrandAndIntentForScamDetection},
              {kClientSideDetectionSendLlamaForcedTriggerInfo,
               kClientSideDetectionLlamaForcedTriggerInfoForScamDetection,
               kClientSideDetectionShowScamVerdictWarning,
               kClientSideDetectionShowLlamaScamVerdictWarning});
  SetInquireOnDeviceModelCallback(/*should_return_response=*/true);
  SetSendClientReportPhishingRequestCallback(
      /*has_expected_brand_and_intent=*/true,
      /*expected_no_info_reason=*/std::nullopt,
      /*expected_llama_forced_trigger_info_trigger_url=*/std::nullopt,
      /*returned_is_phishing=*/false,
      /*returned_intelligent_scan_verdict=*/
      IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_ENFORCEMENT);

  // Now we run the callback to receive a server response. Because the callback
  // responds with the catch all enforcement verdict, but the studies are
  // disabled, we will NOT show a warning.
  EXPECT_CALL(*ui_manager_.get(), DisplayBlockingPage(_)).Times(0);

  PhishingDetectionDone(/*is_phishing=*/false, /*client_score=*/0.8f,
                        ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED,
                        /*did_match_high_confidence_allowlist=*/false);

  VerifyExpectedCalls();
  VerifyGeneralScamDetectionHistograms(
      /*expected_request_type=*/ClientSideDetectionType::
          KEYBOARD_LOCK_REQUESTED,
      /*is_on_device_model_available=*/true,
      /*model_has_successful_response=*/true,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_ENFORCEMENT);
}

TEST_F(ClientSideDetectionHostScamDetectionTest,
       CatchAllEnforcementScamExperimentVerdictDoesShowWarning) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  SetFeatures({kClientSideDetectionBrandAndIntentForScamDetection,
               kClientSideDetectionSendLlamaForcedTriggerInfo,
               kClientSideDetectionLlamaForcedTriggerInfoForScamDetection,
               kClientSideDetectionShowScamVerdictWarning,
               kClientSideDetectionShowLlamaScamVerdictWarning},
              {});
  SetInquireOnDeviceModelCallback(/*should_return_response=*/true);
  SetSendClientReportPhishingRequestCallback(
      /*has_expected_brand_and_intent=*/true,
      /*expected_no_info_reason=*/std::nullopt,
      /*expected_llama_forced_trigger_info_trigger_url=*/std::nullopt,
      /*returned_is_phishing=*/false,
      /*returned_intelligent_scan_verdict=*/
      IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_ENFORCEMENT);

  UnsafeResource resource;
  resource.threat_subtype =
      ThreatSubtype::SCAM_EXPERIMENT_CATCH_ALL_ENFORCEMENT;
  // Because the callback responds with the catch all enforcement verdict, we
  // WILL show a warning.
  EXPECT_CALL(*ui_manager_.get(),
              DisplayBlockingPage(HasScamThreatSubtype(resource)))
      .Times(1);

  PhishingDetectionDone(/*is_phishing=*/false, /*client_score=*/0.8f,
                        ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED,
                        /*did_match_high_confidence_allowlist=*/false);

  VerifyExpectedCalls();
  VerifyGeneralScamDetectionHistograms(
      /*expected_request_type=*/ClientSideDetectionType::
          KEYBOARD_LOCK_REQUESTED,
      /*is_on_device_model_available=*/true,
      /*model_has_successful_response=*/true,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_ENFORCEMENT);
}

}  // namespace safe_browsing
