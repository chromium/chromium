// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lens/google_search_prompt_endpoint.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "base/uuid.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/selection/area_of_interest.h"
#include "chrome/browser/selection/page_context.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/mock_contextual_search_service.h"
#include "components/contextual_search/mock_contextual_search_session_handle.h"
#include "components/contextual_search/pref_names.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/pdf/common/constants.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/version_info/channel.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace lens {
namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

using SearchUrlType =
    contextual_search::ContextualSearchContextController::SearchUrlType;

SkBitmap CreateSolidBitmap(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(SK_ColorBLUE);
  return bitmap;
}

class FakeEligibilityManager
    : public contextual_tasks::ContextualTasksEligibilityManager {
 public:
  FakeEligibilityManager()
      : contextual_tasks::ContextualTasksEligibilityManager(
            /*pref_service=*/nullptr,
            /*identity_manager=*/nullptr,
            /*aim_eligibility_service=*/nullptr) {}
  ~FakeEligibilityManager() override = default;

  bool IsSidePanelAvailable() const override { return side_panel_available_; }

  void set_side_panel_available(bool available) {
    side_panel_available_ = available;
  }

 private:
  bool side_panel_available_ = true;
};

class TestContextualTasksUiService
    : public contextual_tasks::MockContextualTasksUiService {
 public:
  explicit TestContextualTasksUiService(Profile* profile)
      : contextual_tasks::MockContextualTasksUiService(
            profile,
            /*service=*/nullptr,
            /*identity_manager=*/nullptr,
            /*aim_eligibility_service=*/nullptr,
            /*eligibility_manager=*/nullptr,
            /*cookie_synchronizer=*/nullptr) {
    ON_CALL(*this, GetEligibilityManager())
        .WillByDefault(Return(&eligibility_manager_));
  }
  ~TestContextualTasksUiService() override = default;

  FakeEligibilityManager& eligibility_manager() { return eligibility_manager_; }

  MOCK_METHOD(void,
              InitSidePanelWithGhostLoader,
              (BrowserWindowInterface * browser_window_interface,
               tabs::TabInterface* tab_interface,
               std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
                   session_handle,
               omnibox::ChromeAimEntryPoint entry_point),
              (override));
  MOCK_METHOD(bool, IsTaskWaitingForUrl, (const base::Uuid&), (override));
  MOCK_METHOD(void,
              DestroyClosedSidePanel,
              (BrowserWindowInterface*),
              (override));

 private:
  FakeEligibilityManager eligibility_manager_;
};

TEST(GoogleSearchPromptEndpointTest, ConvertRectBoundsToCenterRotatedBox) {
  // 400x300 image; rect covering x=[120, 200] (center 160 = 0.4, w=80 = 0.2),
  // y=[105, 195] (center 150 = 0.5, h=90 = 0.3).
  std::variant<gfx::Rect, std::vector<gfx::Point>> bounds =
      gfx::Rect(120, 105, 80, 90);
  auto box = GoogleSearchPromptEndpoint::ConvertBoundsToCenterRotatedBox(
      bounds, gfx::Size(400, 300));
  ASSERT_TRUE(box);
  EXPECT_EQ(box->coordinate_type,
            mojom::CenterRotatedBox_CoordinateType::kNormalized);
  EXPECT_FLOAT_EQ(box->box.x(), 0.4f);
  EXPECT_FLOAT_EQ(box->box.y(), 0.5f);
  EXPECT_FLOAT_EQ(box->box.width(), 0.2f);
  EXPECT_FLOAT_EQ(box->box.height(), 0.3f);
}

TEST(GoogleSearchPromptEndpointTest, ClampsOutOfBoundsRect) {
  // 400x300 image; rect [300, -45]..[460, 75] clamps to [0.75, 0.0]..[1.0,
  // 0.25].
  std::variant<gfx::Rect, std::vector<gfx::Point>> bounds =
      gfx::Rect(300, -45, 160, 120);
  auto box = GoogleSearchPromptEndpoint::ConvertBoundsToCenterRotatedBox(
      bounds, gfx::Size(400, 300));
  ASSERT_TRUE(box);
  // Left = 0.75, Right = 1.0 -> center_x = 0.875, width = 0.25.
  // Top = 0.0, Bottom = 0.25 -> center_y = 0.125, height = 0.25.
  EXPECT_FLOAT_EQ(box->box.x(), 0.875f);
  EXPECT_FLOAT_EQ(box->box.y(), 0.125f);
  EXPECT_FLOAT_EQ(box->box.width(), 0.25f);
  EXPECT_FLOAT_EQ(box->box.height(), 0.25f);
}

TEST(GoogleSearchPromptEndpointTest, EmptyRectReturnsNull) {
  std::variant<gfx::Rect, std::vector<gfx::Point>> bounds = gfx::Rect();
  EXPECT_FALSE(GoogleSearchPromptEndpoint::ConvertBoundsToCenterRotatedBox(
      bounds, gfx::Size(400, 300)));
}

TEST(GoogleSearchPromptEndpointTest, OutOfBoundsRectReturnsNull) {
  std::variant<gfx::Rect, std::vector<gfx::Point>> bounds =
      gfx::Rect(500, 400, 50, 50);
  EXPECT_FALSE(GoogleSearchPromptEndpoint::ConvertBoundsToCenterRotatedBox(
      bounds, gfx::Size(400, 300)));
}

TEST(GoogleSearchPromptEndpointTest,
     ConvertCollinearPolylineRetainsNonZeroBox) {
  std::variant<gfx::Rect, std::vector<gfx::Point>> bounds =
      std::vector<gfx::Point>{
          gfx::Point(40, 50),
          gfx::Point(120, 50),
      };
  auto box = GoogleSearchPromptEndpoint::ConvertBoundsToCenterRotatedBox(
      bounds, gfx::Size(200, 100));
  ASSERT_TRUE(box);
  EXPECT_FLOAT_EQ(box->box.x(), 0.4f);
  EXPECT_FLOAT_EQ(box->box.width(), 0.4f);
  EXPECT_GT(box->box.height(), 0.0f);
}

TEST(GoogleSearchPromptEndpointTest,
     BottomRightEdgeCollinearPolylineRetainsNonZeroBox) {
  std::variant<gfx::Rect, std::vector<gfx::Point>> bottom_edge =
      std::vector<gfx::Point>{
          gfx::Point(40, 100),
          gfx::Point(120, 100),
      };
  auto box = GoogleSearchPromptEndpoint::ConvertBoundsToCenterRotatedBox(
      bottom_edge, gfx::Size(200, 100));
  ASSERT_TRUE(box);
  EXPECT_FLOAT_EQ(box->box.x(), 0.4f);
  EXPECT_FLOAT_EQ(box->box.width(), 0.4f);
  EXPECT_NEAR(box->box.y(), 0.995f, 1e-5f);
  EXPECT_NEAR(box->box.height(), 0.01f, 1e-5f);

  std::variant<gfx::Rect, std::vector<gfx::Point>> corner_point =
      std::vector<gfx::Point>{gfx::Point(200, 100)};
  auto corner_box = GoogleSearchPromptEndpoint::ConvertBoundsToCenterRotatedBox(
      corner_point, gfx::Size(200, 100));
  ASSERT_TRUE(corner_box);
  EXPECT_GT(corner_box->box.width(), 0.0f);
  EXPECT_GT(corner_box->box.height(), 0.0f);
}

TEST(GoogleSearchPromptEndpointTest, OutOfBoundsPolylineReturnsNull) {
  std::variant<gfx::Rect, std::vector<gfx::Point>> outside_box =
      std::vector<gfx::Point>{
          gfx::Point(300, 50),
          gfx::Point(400, 80),
      };
  EXPECT_FALSE(GoogleSearchPromptEndpoint::ConvertBoundsToCenterRotatedBox(
      outside_box, gfx::Size(200, 100)));

  std::variant<gfx::Rect, std::vector<gfx::Point>> outside_collinear =
      std::vector<gfx::Point>{
          gfx::Point(40, -10),
          gfx::Point(120, -10),
      };
  EXPECT_FALSE(GoogleSearchPromptEndpoint::ConvertBoundsToCenterRotatedBox(
      outside_collinear, gfx::Size(200, 100)));
}

TEST(
    GoogleSearchPromptEndpointTest,
    BuildSearchUrlRequestInfoUsesStandardMultimodalCropForTextAndSpatialRegions) {
  SkBitmap screenshot = CreateSolidBitmap(200, 100);
  std::variant<gfx::Rect, std::vector<gfx::Point>> bounds =
      gfx::Rect(60, 30, 80, 40);
  auto region_box = GoogleSearchPromptEndpoint::ConvertBoundsToCenterRotatedBox(
      bounds, gfx::Size(200, 100));
  ASSERT_TRUE(region_box);

  const base::UnguessableToken token = base::UnguessableToken::Create();
  auto request_info = GoogleSearchPromptEndpoint::BuildSearchUrlRequestInfo(
      "explain simply", token, screenshot, std::move(region_box));

  ASSERT_TRUE(request_info);
  EXPECT_EQ(request_info->search_url_type, SearchUrlType::kStandard);
  EXPECT_EQ(request_info->query_text, "explain simply");
  ASSERT_EQ(request_info->file_tokens.size(), 1u);
  EXPECT_EQ(request_info->file_tokens[0], token);
  EXPECT_EQ(request_info->lens_overlay_selection_type, MULTIMODAL_SEARCH);
  ASSERT_TRUE(request_info->image_crop.has_value());
  EXPECT_FALSE(request_info->image_crop->image().image_content().empty());
}

TEST(GoogleSearchPromptEndpointTest,
     BuildSearchUrlRequestInfoUsesRegionSearchForEmptyPrompt) {
  SkBitmap screenshot = CreateSolidBitmap(200, 100);
  std::variant<gfx::Rect, std::vector<gfx::Point>> bounds =
      gfx::Rect(60, 30, 80, 40);
  auto region_box = GoogleSearchPromptEndpoint::ConvertBoundsToCenterRotatedBox(
      bounds, gfx::Size(200, 100));
  ASSERT_TRUE(region_box);

  const base::UnguessableToken token = base::UnguessableToken::Create();
  auto request_info = GoogleSearchPromptEndpoint::BuildSearchUrlRequestInfo(
      /*prompt=*/"", token, screenshot, std::move(region_box));

  ASSERT_TRUE(request_info);
  EXPECT_EQ(request_info->search_url_type, SearchUrlType::kStandard);
  EXPECT_TRUE(request_info->query_text.empty());
  EXPECT_EQ(request_info->lens_overlay_selection_type, REGION_SEARCH);
  ASSERT_TRUE(request_info->image_crop.has_value());
  EXPECT_FALSE(request_info->image_crop->image().image_content().empty());
}

class GoogleSearchPromptEndpointWithProfileTest
    : public ChromeRenderViewHostTestHarness {
 public:
  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {
        TestingProfile::TestingFactory{
            TemplateURLServiceFactory::GetInstance(),
            base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor)},
        TestingProfile::TestingFactory{
            AimEligibilityServiceFactory::GetInstance(),
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              Profile* p = Profile::FromBrowserContext(context);
              auto mock = std::make_unique<NiceMock<MockAimEligibilityService>>(
                  *p->GetPrefs(), /*template_url_service=*/nullptr,
                  /*url_loader_factory=*/nullptr,
                  /*identity_manager=*/nullptr);
              ON_CALL(*mock, IsAimEligible()).WillByDefault(Return(true));
              ON_CALL(*mock, IsCsbEligible()).WillByDefault(Return(true));
              return mock;
            })},
        TestingProfile::TestingFactory{
            ContextualSearchServiceFactory::GetInstance(),
            base::BindRepeating(
                [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
                  return std::make_unique<
                      NiceMock<contextual_search::MockContextualSearchService>>(
                      /*identity_manager=*/nullptr,
                      /*url_loader_factory=*/nullptr,
                      /*template_url_service=*/nullptr,
                      /*variations_client=*/nullptr,
                      version_info::Channel::UNKNOWN, "en-US");
                })},
        TestingProfile::TestingFactory{
            contextual_tasks::ContextualTasksUiServiceFactory::GetInstance(),
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<NiceMock<TestContextualTasksUiService>>(
                  Profile::FromBrowserContext(context));
            })},
    };
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    sessions::SessionTabHelper::CreateForWebContents(
        web_contents(),
        base::BindRepeating(
            [](content::WebContents*) -> sessions::SessionTabHelperDelegate* {
              return nullptr;
            }));
    NavigateAndCommit(GURL("https://example.com/page"));
    content::WebContentsTester::For(web_contents())
        ->SetTitle(u"Example Page Title");

    mock_aim_eligibility_service_ = static_cast<MockAimEligibilityService*>(
        AimEligibilityServiceFactory::GetForProfile(profile()));
    mock_search_service_ =
        static_cast<contextual_search::MockContextualSearchService*>(
            ContextualSearchServiceFactory::GetForProfile(profile()));
    mock_ui_service_ = static_cast<TestContextualTasksUiService*>(
        contextual_tasks::ContextualTasksUiServiceFactory::GetForBrowserContext(
            profile()));

    mock_panel_controller_.emplace(&mock_bwi_);

    ON_CALL(mock_tab_, GetProfile()).WillByDefault(Return(profile()));
    ON_CALL(mock_tab_, GetContents()).WillByDefault(Return(web_contents()));
    ON_CALL(mock_tab_, GetBrowserWindowInterface())
        .WillByDefault(Return(&mock_bwi_));
    ON_CALL(mock_tab_, IsActivated()).WillByDefault(Return(true));
    ON_CALL(mock_tab_, GetWeakPtr()).WillByDefault([this]() {
      return tab_weak_factory_.GetWeakPtr();
    });
  }

  void TearDown() override {
    retained_session_handle_.reset();
    tab_weak_factory_.InvalidateWeakPtrs();
    mock_panel_controller_.reset();
    mock_ui_service_ = nullptr;
    mock_search_service_ = nullptr;
    mock_aim_eligibility_service_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  void SetContentSharingEnabled(bool enabled) {
    profile()->GetPrefs()->SetInteger(
        contextual_search::kSearchContentSharingSettings,
        static_cast<int>(
            enabled
                ? contextual_search::SearchContentSharingSettingsValue::kEnabled
                : contextual_search::SearchContentSharingSettingsValue::
                      kDisabled));
  }

  ::selection::PageContext CreateScreenshotOnlyPageContext() {
    ::selection::PageContext page_context;
    page_context.screenshot = CreateSolidBitmap(200, 100);
    return page_context;
  }

  ::selection::AreaOfInterest CreateRectAoi() {
    ::selection::AreaOfInterest aoi;
    aoi.bounds = gfx::Rect(20, 10, 60, 40);
    return aoi;
  }

  ::selection::PageContextWithSelection CreateRectContext() {
    return ::selection::PageContextWithSelection(
        CreateScreenshotOnlyPageContext(), CreateRectAoi());
  }

  NiceMock<MockBrowserWindowInterface> mock_bwi_;
  std::optional<NiceMock<contextual_tasks::MockContextualTasksPanelController>>
      mock_panel_controller_;
  tabs::MockTabInterface mock_tab_;
  raw_ptr<MockAimEligibilityService> mock_aim_eligibility_service_ = nullptr;
  raw_ptr<contextual_search::MockContextualSearchService> mock_search_service_ =
      nullptr;
  raw_ptr<TestContextualTasksUiService> mock_ui_service_ = nullptr;
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      retained_session_handle_;
  base::WeakPtrFactory<tabs::TabInterface> tab_weak_factory_{&mock_tab_};
};

TEST_F(GoogleSearchPromptEndpointWithProfileTest,
       BuildContextualInputDataIncludesPdfBytes) {
  content::WebContentsTester::For(web_contents())
      ->SetMainFrameMimeType(pdf::kPDFMimeType);
  ::selection::PageContext page_context = CreateScreenshotOnlyPageContext();
  ::selection::PageContext::PdfDocumentData pdf;
  pdf.origin = url::Origin::Create(GURL("https://example.com/doc.pdf"));
  pdf.bytes = std::vector<uint8_t>{0x25, 0x50, 0x44, 0x46};
  pdf.size_limit_exceeded = false;
  page_context.pdf_data = std::move(pdf);

  auto input_data = GoogleSearchPromptEndpoint::BuildContextualInputData(
      mock_tab_, page_context);

  ASSERT_TRUE(input_data);
  EXPECT_TRUE(input_data->is_implicit_upload);
  EXPECT_TRUE(input_data->has_lens_usage_intent);
  EXPECT_EQ(input_data->is_page_context_eligible, true);
  EXPECT_EQ(
      input_data->upload_type,
      LensOverlayContextualInputUploadType::
          CONTEXTUAL_INPUT_UPLOAD_TYPE_CONTEXTUAL_SEARCHBOX_INITIAL_QUERY);
  ASSERT_TRUE(input_data->viewport_screenshot.has_value());
  EXPECT_EQ(input_data->viewport_screenshot->width(), 200);
  EXPECT_EQ(input_data->primary_content_type, MimeType::kPdf);
  EXPECT_EQ(input_data->page_url, GURL("https://example.com/page"));
  EXPECT_EQ(input_data->page_title, "Example Page Title");
  EXPECT_EQ(input_data->tab_session_id,
            sessions::SessionTabHelper::IdForTab(web_contents()));
  ASSERT_TRUE(input_data->context_input.has_value());
  ASSERT_EQ(input_data->context_input->size(), 1u);
  EXPECT_EQ((*input_data->context_input)[0].content_type_, MimeType::kPdf);
  EXPECT_EQ((*input_data->context_input)[0].bytes_,
            (std::vector<uint8_t>{0x25, 0x50, 0x44, 0x46}));
}

TEST_F(GoogleSearchPromptEndpointWithProfileTest,
       BuildContextualInputDataIncludesSerializedApc) {
  ::selection::PageContext page_context = CreateScreenshotOnlyPageContext();
  page_context.apc.set_version(
      optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);

  auto input_data = GoogleSearchPromptEndpoint::BuildContextualInputData(
      mock_tab_, page_context);

  ASSERT_TRUE(input_data);
  EXPECT_EQ(input_data->primary_content_type, MimeType::kAnnotatedPageContent);
  EXPECT_EQ(input_data->page_url, GURL("https://example.com/page"));
  EXPECT_EQ(input_data->page_title, "Example Page Title");
  EXPECT_EQ(input_data->tab_session_id,
            sessions::SessionTabHelper::IdForTab(web_contents()));
  ASSERT_TRUE(input_data->context_input.has_value());
  ASSERT_EQ(input_data->context_input->size(), 1u);
  EXPECT_EQ((*input_data->context_input)[0].content_type_,
            MimeType::kAnnotatedPageContent);

  optimization_guide::proto::AnnotatedPageContent parsed_apc;
  const auto& bytes = (*input_data->context_input)[0].bytes_;
  ASSERT_TRUE(parsed_apc.ParseFromArray(bytes.data(), bytes.size()));
  EXPECT_EQ(parsed_apc.version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
}

TEST_F(GoogleSearchPromptEndpointWithProfileTest,
       BuildContextualInputDataPrefersPdfOverApc) {
  content::WebContentsTester::For(web_contents())
      ->SetMainFrameMimeType(pdf::kPDFMimeType);
  ::selection::PageContext page_context = CreateScreenshotOnlyPageContext();
  page_context.apc.set_version(
      optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  ::selection::PageContext::PdfDocumentData pdf;
  pdf.bytes = std::vector<uint8_t>{0x25, 0x50, 0x44, 0x46};
  page_context.pdf_data = std::move(pdf);

  auto input_data = GoogleSearchPromptEndpoint::BuildContextualInputData(
      mock_tab_, page_context);

  ASSERT_TRUE(input_data);
  EXPECT_EQ(input_data->primary_content_type, MimeType::kPdf);
  ASSERT_TRUE(input_data->context_input.has_value());
  ASSERT_EQ(input_data->context_input->size(), 1u);
  EXPECT_EQ((*input_data->context_input)[0].content_type_, MimeType::kPdf);
}

TEST_F(GoogleSearchPromptEndpointWithProfileTest,
       BuildContextualInputDataIgnoresEmbeddedPdfOnHtmlPage) {
  // Default main frame MIME type is non-PDF (e.g. an HTML page embedding a
  // cross-origin PDF iframe).
  ::selection::PageContext page_context = CreateScreenshotOnlyPageContext();
  page_context.apc.set_version(
      optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  ::selection::PageContext::PdfDocumentData embedded_pdf;
  embedded_pdf.origin =
      url::Origin::Create(GURL("https://other.example/x.pdf"));
  embedded_pdf.bytes = std::vector<uint8_t>{0x25, 0x50, 0x44, 0x46};
  page_context.pdf_data = std::move(embedded_pdf);

  auto input_data = GoogleSearchPromptEndpoint::BuildContextualInputData(
      mock_tab_, page_context);

  ASSERT_TRUE(input_data);
  EXPECT_EQ(input_data->primary_content_type, MimeType::kAnnotatedPageContent);
  ASSERT_TRUE(input_data->context_input.has_value());
  ASSERT_EQ(input_data->context_input->size(), 1u);
  EXPECT_EQ((*input_data->context_input)[0].content_type_,
            MimeType::kAnnotatedPageContent);

  // Without APC, embedded PDF bytes alone fall back to image-only rather than
  // attaching iframe PDF bytes under the host page's URL.
  page_context.apc.Clear();
  auto image_only = GoogleSearchPromptEndpoint::BuildContextualInputData(
      mock_tab_, page_context);
  ASSERT_TRUE(image_only);
  EXPECT_EQ(image_only->primary_content_type, MimeType::kImage);
  ASSERT_TRUE(image_only->context_input.has_value());
  EXPECT_TRUE(image_only->context_input->empty());
  EXPECT_FALSE(image_only->page_url.has_value());
}

TEST_F(GoogleSearchPromptEndpointWithProfileTest,
       BuildContextualInputDataImageOnlyWhenNoPageContent) {
  ::selection::PageContext page_context = CreateScreenshotOnlyPageContext();

  auto input_data = GoogleSearchPromptEndpoint::BuildContextualInputData(
      mock_tab_, page_context);

  ASSERT_TRUE(input_data);
  ASSERT_TRUE(input_data->viewport_screenshot.has_value());
  EXPECT_EQ(input_data->viewport_screenshot->width(), 200);
  EXPECT_EQ(input_data->primary_content_type, MimeType::kImage);
  ASSERT_TRUE(input_data->context_input.has_value());
  EXPECT_TRUE(input_data->context_input->empty());
  EXPECT_FALSE(input_data->page_url.has_value());
  EXPECT_FALSE(input_data->page_title.has_value());
  EXPECT_FALSE(input_data->tab_session_id.has_value());
}

TEST_F(GoogleSearchPromptEndpointWithProfileTest,
       BuildContextualInputDataImageOnlyWhenPdfSizeLimitExceeded) {
  content::WebContentsTester::For(web_contents())
      ->SetMainFrameMimeType(pdf::kPDFMimeType);
  ::selection::PageContext page_context = CreateScreenshotOnlyPageContext();
  // Blink still produces a wrapper APC for top-level PDFs; when PDF bytes
  // exceed the size limit, it must stay image-only rather than uploading the
  // wrapper APC.
  page_context.apc.set_version(
      optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  ::selection::PageContext::PdfDocumentData pdf;
  pdf.origin = url::Origin::Create(GURL("https://example.com/large.pdf"));
  pdf.size_limit_exceeded = true;
  page_context.pdf_data = std::move(pdf);

  auto input_data = GoogleSearchPromptEndpoint::BuildContextualInputData(
      mock_tab_, page_context);

  ASSERT_TRUE(input_data);
  EXPECT_EQ(input_data->primary_content_type, MimeType::kImage);
  ASSERT_TRUE(input_data->context_input.has_value());
  EXPECT_TRUE(input_data->context_input->empty());
  EXPECT_FALSE(input_data->page_url.has_value());
}

TEST_F(GoogleSearchPromptEndpointWithProfileTest,
       BuildContextualInputDataImageOnlyWhenCsbDisabled) {
  content::WebContentsTester::For(web_contents())
      ->SetMainFrameMimeType(pdf::kPDFMimeType);
  ON_CALL(*mock_aim_eligibility_service_, IsCsbEligible())
      .WillByDefault(Return(false));

  ::selection::PageContext page_context = CreateScreenshotOnlyPageContext();
  page_context.apc.set_version(
      optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  ::selection::PageContext::PdfDocumentData pdf;
  pdf.bytes = std::vector<uint8_t>{0x25, 0x50, 0x44, 0x46};
  page_context.pdf_data = std::move(pdf);

  auto input_data = GoogleSearchPromptEndpoint::BuildContextualInputData(
      mock_tab_, page_context);

  ASSERT_TRUE(input_data);
  EXPECT_EQ(input_data->primary_content_type, MimeType::kImage);
  ASSERT_TRUE(input_data->context_input.has_value());
  EXPECT_TRUE(input_data->context_input->empty());
  EXPECT_FALSE(input_data->page_url.has_value());
}

TEST_F(GoogleSearchPromptEndpointWithProfileTest,
       IsAvailableChecksFeatureAndContextSharingSetting) {
  GoogleSearchPromptEndpoint endpoint;

  // Feature is disabled by default.
  EXPECT_FALSE(endpoint.IsAvailable(mock_tab_));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kSmartSelectionGoogleSearch);

  SetContentSharingEnabled(true);
  EXPECT_TRUE(endpoint.IsAvailable(mock_tab_));

  // Even if the eligibility manager reports the side panel available (e.g.
  // when `kContextualTasks` is disabled), disabling search content sharing
  // directly gates availability.
  SetContentSharingEnabled(false);
  EXPECT_FALSE(endpoint.IsAvailable(mock_tab_));
}

TEST_F(GoogleSearchPromptEndpointWithProfileTest,
       SubmitAbortsWhenContentSharingDisabledOnNewSession) {
  SetContentSharingEnabled(false);

  auto mock_handle = std::make_unique<
      NiceMock<contextual_search::MockContextualSearchSessionHandle>>();
  auto* raw_handle = mock_handle.get();
  EXPECT_CALL(*mock_search_service_, CreateSession(_, _, _))
      .WillOnce(Return(std::move(mock_handle)));
  EXPECT_CALL(*raw_handle, NotifySessionStarted()).Times(0);
  EXPECT_CALL(*raw_handle, CreateContextToken()).Times(0);
  EXPECT_CALL(*raw_handle, StartTabContextUploadFlow(_, _, _)).Times(0);
  EXPECT_CALL(*raw_handle, CreateSearchUrl(_, _)).Times(0);
  EXPECT_CALL(*mock_ui_service_, InitSidePanelWithGhostLoader(_, _, _, _))
      .Times(0);

  GoogleSearchPromptEndpoint endpoint;
  endpoint.Submit(mock_tab_, "what is this?", CreateRectContext());
}

TEST_F(GoogleSearchPromptEndpointWithProfileTest,
       SubmitAbortsWhenContentSharingDisabledOnReusedPanelSession) {
  SetContentSharingEnabled(false);

  NiceMock<contextual_search::MockContextualSearchSessionHandle> panel_handle;
  ON_CALL(*mock_panel_controller_, GetActiveWebContents())
      .WillByDefault(Return(web_contents()));
  ON_CALL(*mock_panel_controller_, IsPanelOpenForContextualTask())
      .WillByDefault(Return(true));
  ON_CALL(*mock_panel_controller_, GetContextualSearchSessionHandleForPanel())
      .WillByDefault(Return(&panel_handle));

  EXPECT_CALL(*mock_search_service_, CreateSession(_, _, _)).Times(0);
  EXPECT_CALL(panel_handle, CreateContextToken()).Times(0);
  EXPECT_CALL(panel_handle, StartTabContextUploadFlow(_, _, _)).Times(0);
  EXPECT_CALL(panel_handle, CreateSearchUrl(_, _)).Times(0);

  GoogleSearchPromptEndpoint endpoint;
  endpoint.Submit(mock_tab_, "what is this?", CreateRectContext());
}

TEST_F(GoogleSearchPromptEndpointWithProfileTest,
       CancelPendingRequestsClosesPendingGhostLoader) {
  SetContentSharingEnabled(true);

  const base::Uuid task_id = base::Uuid::GenerateRandomV4();
  base::OnceCallback<void(GURL)> pending_url_callback;

  auto mock_handle = std::make_unique<
      NiceMock<contextual_search::MockContextualSearchSessionHandle>>();
  auto* raw_handle = mock_handle.get();
  ON_CALL(*raw_handle, CreateContextToken())
      .WillByDefault(Return(base::UnguessableToken::Create()));
  EXPECT_CALL(*mock_search_service_, CreateSession(_, _, _))
      .WillOnce(Return(std::move(mock_handle)));
  EXPECT_CALL(*raw_handle, NotifySessionStarted()).Times(1);
  EXPECT_CALL(*raw_handle, StartTabContextUploadFlow(_, _, _)).Times(1);
  EXPECT_CALL(*mock_ui_service_, InitSidePanelWithGhostLoader(_, _, _, _))
      .WillOnce(
          [this, task_id](
              BrowserWindowInterface*, tabs::TabInterface*,
              std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
                  handle,
              omnibox::ChromeAimEntryPoint) {
            retained_session_handle_ = std::move(handle);
            ON_CALL(*mock_panel_controller_, GetCurrentTask())
                .WillByDefault(
                    Return(contextual_tasks::ContextualTask(task_id)));
          });
  EXPECT_CALL(*raw_handle, CreateSearchUrl(_, _))
      .WillOnce([&pending_url_callback](
                    std::unique_ptr<
                        contextual_search::ContextualSearchContextController::
                            CreateSearchUrlRequestInfo>,
                    base::OnceCallback<void(GURL)> callback) {
        pending_url_callback = std::move(callback);
      });

  GoogleSearchPromptEndpoint endpoint;
  endpoint.Submit(mock_tab_, "explain", CreateRectContext());
  ASSERT_TRUE(pending_url_callback);

  ON_CALL(*mock_panel_controller_, IsPanelOpenForContextualTask())
      .WillByDefault(Return(true));
  ON_CALL(*mock_ui_service_, IsTaskWaitingForUrl(task_id))
      .WillByDefault(Return(true));

  EXPECT_CALL(*mock_panel_controller_, Close()).Times(1);
  EXPECT_CALL(*mock_ui_service_, DestroyClosedSidePanel(&mock_bwi_)).Times(1);

  endpoint.CancelPendingRequests();

  // Subsequent CancelPendingRequests is a no-op once cleared.
  EXPECT_CALL(*mock_panel_controller_, Close()).Times(0);
  EXPECT_CALL(*mock_ui_service_, DestroyClosedSidePanel(_)).Times(0);
  endpoint.CancelPendingRequests();
}

TEST_F(GoogleSearchPromptEndpointWithProfileTest,
       CancelPendingRequestsDestroysAlreadyClosedPendingGhostLoader) {
  SetContentSharingEnabled(true);

  const base::Uuid task_id = base::Uuid::GenerateRandomV4();
  auto mock_handle = std::make_unique<
      NiceMock<contextual_search::MockContextualSearchSessionHandle>>();
  auto* raw_handle = mock_handle.get();
  ON_CALL(*raw_handle, CreateContextToken())
      .WillByDefault(Return(base::UnguessableToken::Create()));
  EXPECT_CALL(*mock_search_service_, CreateSession(_, _, _))
      .WillOnce(Return(std::move(mock_handle)));
  EXPECT_CALL(*mock_ui_service_, InitSidePanelWithGhostLoader(_, _, _, _))
      .WillOnce(
          [this, task_id](
              BrowserWindowInterface*, tabs::TabInterface*,
              std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
                  handle,
              omnibox::ChromeAimEntryPoint) {
            retained_session_handle_ = std::move(handle);
            ON_CALL(*mock_panel_controller_, GetCurrentTask())
                .WillByDefault(
                    Return(contextual_tasks::ContextualTask(task_id)));
          });

  GoogleSearchPromptEndpoint endpoint;
  endpoint.Submit(mock_tab_, "explain", CreateRectContext());

  ON_CALL(*mock_panel_controller_, IsPanelOpenForContextualTask())
      .WillByDefault(Return(false));
  ON_CALL(*mock_ui_service_, IsTaskWaitingForUrl(task_id))
      .WillByDefault(Return(true));

  EXPECT_CALL(*mock_panel_controller_, Close()).Times(0);
  EXPECT_CALL(*mock_ui_service_, DestroyClosedSidePanel(&mock_bwi_)).Times(1);
  endpoint.CancelPendingRequests();
}

TEST_F(GoogleSearchPromptEndpointWithProfileTest,
       CancelPendingRequestsSkipsCleanupWhenTaskNotWaitingForUrl) {
  SetContentSharingEnabled(true);

  const base::Uuid task_id = base::Uuid::GenerateRandomV4();
  auto mock_handle = std::make_unique<
      NiceMock<contextual_search::MockContextualSearchSessionHandle>>();
  auto* raw_handle = mock_handle.get();
  ON_CALL(*raw_handle, CreateContextToken())
      .WillByDefault(Return(base::UnguessableToken::Create()));
  EXPECT_CALL(*mock_search_service_, CreateSession(_, _, _))
      .WillOnce(Return(std::move(mock_handle)));
  EXPECT_CALL(*mock_ui_service_, InitSidePanelWithGhostLoader(_, _, _, _))
      .WillOnce(
          [this, task_id](
              BrowserWindowInterface*, tabs::TabInterface*,
              std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
                  handle,
              omnibox::ChromeAimEntryPoint) {
            retained_session_handle_ = std::move(handle);
            ON_CALL(*mock_panel_controller_, GetCurrentTask())
                .WillByDefault(
                    Return(contextual_tasks::ContextualTask(task_id)));
          });

  GoogleSearchPromptEndpoint endpoint;
  endpoint.Submit(mock_tab_, "explain", CreateRectContext());

  ON_CALL(*mock_panel_controller_, IsPanelOpenForContextualTask())
      .WillByDefault(Return(true));
  ON_CALL(*mock_ui_service_, IsTaskWaitingForUrl(task_id))
      .WillByDefault(Return(false));

  EXPECT_CALL(*mock_panel_controller_, Close()).Times(0);
  EXPECT_CALL(*mock_ui_service_, DestroyClosedSidePanel(_)).Times(0);
  endpoint.CancelPendingRequests();
}

TEST_F(GoogleSearchPromptEndpointWithProfileTest,
       CancelPendingRequestsSkipsCleanupWhenDifferentTaskIsActive) {
  SetContentSharingEnabled(true);

  const base::Uuid task_id = base::Uuid::GenerateRandomV4();
  const base::Uuid other_task_id = base::Uuid::GenerateRandomV4();
  auto mock_handle = std::make_unique<
      NiceMock<contextual_search::MockContextualSearchSessionHandle>>();
  auto* raw_handle = mock_handle.get();
  ON_CALL(*raw_handle, CreateContextToken())
      .WillByDefault(Return(base::UnguessableToken::Create()));
  EXPECT_CALL(*mock_search_service_, CreateSession(_, _, _))
      .WillOnce(Return(std::move(mock_handle)));
  EXPECT_CALL(*mock_ui_service_, InitSidePanelWithGhostLoader(_, _, _, _))
      .WillOnce(
          [this, task_id](
              BrowserWindowInterface*, tabs::TabInterface*,
              std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
                  handle,
              omnibox::ChromeAimEntryPoint) {
            retained_session_handle_ = std::move(handle);
            ON_CALL(*mock_panel_controller_, GetCurrentTask())
                .WillByDefault(
                    Return(contextual_tasks::ContextualTask(task_id)));
          });

  GoogleSearchPromptEndpoint endpoint;
  endpoint.Submit(mock_tab_, "explain", CreateRectContext());

  // Simulate another flow (e.g. Lens region search) replacing the active tab's
  // task with its own pending task before our URL completes.
  ON_CALL(*mock_panel_controller_, GetCurrentTask())
      .WillByDefault(Return(contextual_tasks::ContextualTask(other_task_id)));
  ON_CALL(*mock_panel_controller_, IsPanelOpenForContextualTask())
      .WillByDefault(Return(true));
  ON_CALL(*mock_ui_service_, IsTaskWaitingForUrl(other_task_id))
      .WillByDefault(Return(true));

  EXPECT_CALL(*mock_panel_controller_, Close()).Times(0);
  EXPECT_CALL(*mock_ui_service_, DestroyClosedSidePanel(_)).Times(0);
  endpoint.CancelPendingRequests();
}

TEST_F(GoogleSearchPromptEndpointWithProfileTest,
       CancelPendingRequestsSkipsCleanupWhenTabNotActivated) {
  SetContentSharingEnabled(true);

  const base::Uuid task_id = base::Uuid::GenerateRandomV4();
  auto mock_handle = std::make_unique<
      NiceMock<contextual_search::MockContextualSearchSessionHandle>>();
  auto* raw_handle = mock_handle.get();
  ON_CALL(*raw_handle, CreateContextToken())
      .WillByDefault(Return(base::UnguessableToken::Create()));
  EXPECT_CALL(*mock_search_service_, CreateSession(_, _, _))
      .WillOnce(Return(std::move(mock_handle)));
  EXPECT_CALL(*mock_ui_service_, InitSidePanelWithGhostLoader(_, _, _, _))
      .WillOnce(
          [this, task_id](
              BrowserWindowInterface*, tabs::TabInterface*,
              std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
                  handle,
              omnibox::ChromeAimEntryPoint) {
            retained_session_handle_ = std::move(handle);
            ON_CALL(*mock_panel_controller_, GetCurrentTask())
                .WillByDefault(
                    Return(contextual_tasks::ContextualTask(task_id)));
          });

  GoogleSearchPromptEndpoint endpoint;
  endpoint.Submit(mock_tab_, "explain", CreateRectContext());

  ON_CALL(mock_tab_, IsActivated()).WillByDefault(Return(false));
  ON_CALL(*mock_panel_controller_, IsPanelOpenForContextualTask())
      .WillByDefault(Return(true));
  ON_CALL(*mock_ui_service_, IsTaskWaitingForUrl(task_id))
      .WillByDefault(Return(true));

  EXPECT_CALL(*mock_panel_controller_, Close()).Times(0);
  EXPECT_CALL(*mock_ui_service_, DestroyClosedSidePanel(_)).Times(0);
  endpoint.CancelPendingRequests();
}

TEST_F(GoogleSearchPromptEndpointWithProfileTest,
       EmptySearchUrlClosesPendingGhostLoader) {
  SetContentSharingEnabled(true);

  const base::Uuid task_id = base::Uuid::GenerateRandomV4();
  auto mock_handle = std::make_unique<
      NiceMock<contextual_search::MockContextualSearchSessionHandle>>();
  auto* raw_handle = mock_handle.get();
  ON_CALL(*raw_handle, CreateContextToken())
      .WillByDefault(Return(base::UnguessableToken::Create()));
  EXPECT_CALL(*mock_search_service_, CreateSession(_, _, _))
      .WillOnce(Return(std::move(mock_handle)));
  EXPECT_CALL(*mock_ui_service_, InitSidePanelWithGhostLoader(_, _, _, _))
      .WillOnce(
          [this, task_id](
              BrowserWindowInterface*, tabs::TabInterface*,
              std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
                  handle,
              omnibox::ChromeAimEntryPoint) {
            retained_session_handle_ = std::move(handle);
            ON_CALL(*mock_panel_controller_, GetCurrentTask())
                .WillByDefault(
                    Return(contextual_tasks::ContextualTask(task_id)));
          });

  ON_CALL(*mock_panel_controller_, IsPanelOpenForContextualTask())
      .WillByDefault(Return(false));
  EXPECT_CALL(*raw_handle, CreateSearchUrl(_, _))
      .WillOnce([this, task_id](
                    std::unique_ptr<
                        contextual_search::ContextualSearchContextController::
                            CreateSearchUrlRequestInfo>,
                    base::OnceCallback<void(GURL)> callback) {
        ON_CALL(*mock_panel_controller_, IsPanelOpenForContextualTask())
            .WillByDefault(Return(true));
        ON_CALL(*mock_ui_service_, IsTaskWaitingForUrl(task_id))
            .WillByDefault(Return(true));
        std::move(callback).Run(GURL());
      });

  EXPECT_CALL(*mock_panel_controller_, Close()).Times(1);
  EXPECT_CALL(*mock_ui_service_, DestroyClosedSidePanel(&mock_bwi_)).Times(1);
  EXPECT_CALL(*mock_ui_service_, StartTaskUiInSidePanelImpl(_, _, _, _, _))
      .Times(0);

  GoogleSearchPromptEndpoint endpoint;
  endpoint.Submit(mock_tab_, "explain", CreateRectContext());
}

TEST_F(GoogleSearchPromptEndpointWithProfileTest,
       DroppedSessionHandleClosesPendingGhostLoader) {
  SetContentSharingEnabled(true);

  const base::Uuid task_id = base::Uuid::GenerateRandomV4();
  auto mock_handle = std::make_unique<
      NiceMock<contextual_search::MockContextualSearchSessionHandle>>();
  auto* raw_handle = mock_handle.get();
  ON_CALL(*raw_handle, CreateContextToken())
      .WillByDefault(Return(base::UnguessableToken::Create()));
  EXPECT_CALL(*mock_search_service_, CreateSession(_, _, _))
      .WillOnce(Return(std::move(mock_handle)));
  // Dropping `session_handle` inside `InitSidePanelWithGhostLoader` invalidates
  // `weak_handle`, exercising the `!weak_handle` cleanup branch.
  EXPECT_CALL(*mock_ui_service_, InitSidePanelWithGhostLoader(_, _, _, _))
      .WillOnce(
          [this, task_id](
              BrowserWindowInterface*, tabs::TabInterface*,
              std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
                  handle,
              omnibox::ChromeAimEntryPoint) {
            handle.reset();
            ON_CALL(*mock_panel_controller_, GetCurrentTask())
                .WillByDefault(
                    Return(contextual_tasks::ContextualTask(task_id)));
            ON_CALL(*mock_panel_controller_, IsPanelOpenForContextualTask())
                .WillByDefault(Return(true));
            ON_CALL(*mock_ui_service_, IsTaskWaitingForUrl(task_id))
                .WillByDefault(Return(true));
          });

  EXPECT_CALL(*mock_panel_controller_, Close()).Times(1);
  EXPECT_CALL(*mock_ui_service_, DestroyClosedSidePanel(&mock_bwi_)).Times(1);

  GoogleSearchPromptEndpoint endpoint;
  endpoint.Submit(mock_tab_, "explain", CreateRectContext());
}

TEST_F(GoogleSearchPromptEndpointWithProfileTest,
       SuccessfulSearchUrlStartsTaskUiAndClearsPendingGhostLoader) {
  SetContentSharingEnabled(true);

  const base::Uuid task_id = base::Uuid::GenerateRandomV4();
  const GURL search_url("https://www.google.com/search?q=explain");
  auto mock_handle = std::make_unique<
      NiceMock<contextual_search::MockContextualSearchSessionHandle>>();
  auto* raw_handle = mock_handle.get();
  ON_CALL(*raw_handle, CreateContextToken())
      .WillByDefault(Return(base::UnguessableToken::Create()));
  EXPECT_CALL(*mock_search_service_, CreateSession(_, _, _))
      .WillOnce(Return(std::move(mock_handle)));
  EXPECT_CALL(*mock_ui_service_, InitSidePanelWithGhostLoader(_, _, _, _))
      .WillOnce(
          [this, task_id](
              BrowserWindowInterface*, tabs::TabInterface*,
              std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
                  handle,
              omnibox::ChromeAimEntryPoint) {
            retained_session_handle_ = std::move(handle);
            ON_CALL(*mock_panel_controller_, GetCurrentTask())
                .WillByDefault(
                    Return(contextual_tasks::ContextualTask(task_id)));
          });
  EXPECT_CALL(*raw_handle, CreateSearchUrl(_, _))
      .WillOnce([search_url](
                    std::unique_ptr<
                        contextual_search::ContextualSearchContextController::
                            CreateSearchUrlRequestInfo>,
                    base::OnceCallback<void(GURL)> callback) {
        std::move(callback).Run(search_url);
      });
  EXPECT_CALL(*mock_ui_service_, StartTaskUiInSidePanelImpl(
                                     &mock_bwi_, &mock_tab_, search_url, _, _))
      .Times(1);

  GoogleSearchPromptEndpoint endpoint;
  endpoint.Submit(mock_tab_, "explain", CreateRectContext());

  // Even if the mock claims the task is still waiting for URL, the endpoint
  // already cleared `pending_ghost_loader_tab_` /
  // `pending_ghost_loader_task_id_` when `OnSearchUrlCreated` ran.
  ON_CALL(*mock_panel_controller_, IsPanelOpenForContextualTask())
      .WillByDefault(Return(true));
  ON_CALL(*mock_ui_service_, IsTaskWaitingForUrl(task_id))
      .WillByDefault(Return(true));
  EXPECT_CALL(*mock_panel_controller_, Close()).Times(0);
  EXPECT_CALL(*mock_ui_service_, DestroyClosedSidePanel(_)).Times(0);

  endpoint.CancelPendingRequests();
}

}  // namespace
}  // namespace lens
