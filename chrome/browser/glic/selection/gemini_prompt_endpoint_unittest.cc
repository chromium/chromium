// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/gemini_prompt_endpoint.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/glic_warming_checks.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_service_factory.h"
#include "chrome/browser/glic/test_support/glic_api_test_passkeys.h"
#include "chrome/browser/glic/test_support/mock_glic_instance.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/selection/area_of_interest.h"
#include "chrome/browser/selection/page_context.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content_metadata.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace glic {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::NiceMock;
using ::testing::Return;

class MockGlicKeyedServiceForEndpoint : public MockGlicKeyedService {
 public:
  using MockGlicKeyedService::MockGlicKeyedService;

  MOCK_METHOD(void, TryPreload, (GlicWarmingTrigger), (override));
};

SkBitmap MakeBitmap(int width, int height, SkColor color = SK_ColorBLUE) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  return bitmap;
}

TEST(GeminiPromptEndpointTest,
     BuildTabContextResultPreservesPdfOriginSizeLimitAndMetadata) {
  ::selection::PageContext page_context;
  page_context.apc.set_version(
      optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  auto frame_metadata = blink::mojom::FrameMetadata::New();
  frame_metadata->url = GURL("https://example.com/article");
  frame_metadata->meta_tags.push_back(
      blink::mojom::MetaTag::New("author", "Alice"));
  page_context.metadata = blink::mojom::PageMetadata::New();
  page_context.metadata->frame_metadata.push_back(std::move(frame_metadata));

  ::selection::PageContext::PdfDocumentData pdf;
  pdf.origin = url::Origin::Create(GURL("https://cdn.example.com/doc.pdf"));
  pdf.bytes = std::vector<uint8_t>{0x25, 0x50, 0x44, 0x46};
  pdf.size_limit_exceeded = false;
  page_context.pdf_data = std::move(pdf);

  ::selection::PageContext::EncodedScreenshot encoded;
  encoded.data = std::vector<uint8_t>{0xFF, 0xD8, 0xFF};
  encoded.mime_type = "image/webp";
  encoded.size = gfx::Size(640, 480);

  mojom::TabContextResultPtr result =
      GeminiPromptEndpoint::BuildTabContextResult(
          /*tab_data=*/nullptr, std::move(encoded), std::move(page_context));

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->viewport_screenshot);
  EXPECT_EQ(result->viewport_screenshot->width_pixels, 640u);
  EXPECT_EQ(result->viewport_screenshot->height_pixels, 480u);
  EXPECT_EQ(result->viewport_screenshot->data,
            (std::vector<uint8_t>{0xFF, 0xD8, 0xFF}));
  EXPECT_EQ(result->viewport_screenshot->mime_type, "image/webp");

  ASSERT_TRUE(result->annotated_page_data);
  EXPECT_TRUE(result->annotated_page_data->annotated_page_content.has_value());
  ASSERT_TRUE(result->annotated_page_data->metadata);
  ASSERT_EQ(result->annotated_page_data->metadata->frame_metadata.size(), 1u);
  EXPECT_EQ(result->annotated_page_data->metadata->frame_metadata[0]->url,
            GURL("https://example.com/article"));

  ASSERT_TRUE(result->pdf_document_data);
  EXPECT_EQ(result->pdf_document_data->origin,
            url::Origin::Create(GURL("https://cdn.example.com/doc.pdf")));
  EXPECT_FALSE(result->pdf_document_data->pdf_size_limit_exceeded);
  ASSERT_TRUE(result->pdf_document_data->pdf_data.has_value());
  EXPECT_EQ(*result->pdf_document_data->pdf_data,
            (std::vector<uint8_t>{0x25, 0x50, 0x44, 0x46}));
}

TEST(GeminiPromptEndpointTest,
     BuildTabContextResultPreservesPdfSizeLimitExceededWithoutBytes) {
  ::selection::PageContext page_context;
  ::selection::PageContext::PdfDocumentData pdf;
  pdf.origin = url::Origin::Create(GURL("https://example.com/large.pdf"));
  pdf.size_limit_exceeded = true;
  page_context.pdf_data = std::move(pdf);

  ::selection::PageContext::EncodedScreenshot encoded;
  encoded.data = std::vector<uint8_t>{1, 2};
  encoded.size = gfx::Size(100, 100);

  mojom::TabContextResultPtr result =
      GeminiPromptEndpoint::BuildTabContextResult(
          /*tab_data=*/nullptr, std::move(encoded), std::move(page_context));

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->viewport_screenshot);
  EXPECT_TRUE(result->viewport_screenshot->mime_type.empty());
  ASSERT_TRUE(result->pdf_document_data);
  EXPECT_EQ(result->pdf_document_data->origin,
            url::Origin::Create(GURL("https://example.com/large.pdf")));
  EXPECT_TRUE(result->pdf_document_data->pdf_size_limit_exceeded);
  EXPECT_FALSE(result->pdf_document_data->pdf_data.has_value());
}

class GeminiPromptEndpointSubmitTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    bypass_enablement_.emplace();
    TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
        /*profile_manager=*/true);
    ChromeRenderViewHostTestHarness::SetUp();

    GlicKeyedServiceFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating(&GeminiPromptEndpointSubmitTest::CreateService,
                            base::Unretained(this)));
    GlicKeyedServiceFactory::GetGlicKeyedService(profile(), /*create=*/true);

    ON_CALL(mock_tab_, GetProfile()).WillByDefault(Return(profile()));
    ON_CALL(mock_tab_, GetContents()).WillByDefault(Return(web_contents()));
    ON_CALL(mock_tab_, GetWeakPtr()).WillByDefault([this]() {
      return tab_weak_factory_.GetWeakPtr();
    });

    endpoint_ = std::make_unique<GeminiPromptEndpoint>(
        GlicApiTestPasskeys::GetPassKey());
  }

  void TearDown() override {
    endpoint_.reset();
    tab_weak_factory_.InvalidateWeakPtrs();
    mock_service_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
    bypass_enablement_.reset();
  }

  std::unique_ptr<KeyedService> CreateService(
      content::BrowserContext* context) {
    Profile* p = Profile::FromBrowserContext(context);
    auto service = std::make_unique<NiceMock<MockGlicKeyedServiceForEndpoint>>(
        context, IdentityManagerFactory::GetForProfile(p),
        TestingBrowserProcess::GetGlobal()->profile_manager(),
        &glic_profile_manager_,
        ContextualCueingServiceFactory::GetForProfile(p),
        actor::ActorKeyedServiceFactory::GetActorKeyedService(p));
    mock_service_ = service.get();
    return service;
  }

  GeminiPromptEndpoint& endpoint() { return *endpoint_; }
  MockGlicKeyedServiceForEndpoint& mock_service() { return *mock_service_; }
  tabs::MockTabInterface& mock_tab() { return mock_tab_; }

  void DisableGlicEnablement() { bypass_enablement_.reset(); }

  void FlushAsyncScreenshotEncode() {
    // Wait for the ThreadPool task to finish and post its reply back to the UI
    // sequence, then drain that reply via an explicit quit barrier.
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  std::optional<GlicEnabling::ScopedBypassEnablementChecksForTesting>
      bypass_enablement_;
  GlicProfileManager glic_profile_manager_;
  raw_ptr<MockGlicKeyedServiceForEndpoint> mock_service_ = nullptr;
  tabs::MockTabInterface mock_tab_;
  base::WeakPtrFactory<tabs::TabInterface> tab_weak_factory_{&mock_tab_};
  std::unique_ptr<GeminiPromptEndpoint> endpoint_;
};

TEST_F(GeminiPromptEndpointSubmitTest, IsAvailableRespectsGlicEnabling) {
  EXPECT_TRUE(endpoint().IsAvailable(mock_tab()));

  DisableGlicEnablement();
  EXPECT_FALSE(endpoint().IsAvailable(mock_tab()));
}

TEST_F(GeminiPromptEndpointSubmitTest,
       PreparePreloadsOnlyWhenAllowedAndNoInstance) {
  EXPECT_CALL(mock_service(), GetInstanceForTab(&mock_tab()))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(mock_service(),
              TryPreload(GlicWarmingTrigger::kSelectionOverlay));
  endpoint().Prepare(mock_tab());
  testing::Mock::VerifyAndClearExpectations(&mock_service());

  // Already has an active instance for the tab: should not preload.
  NiceMock<MockGlicInstance> existing_instance;
  EXPECT_CALL(mock_service(), GetInstanceForTab(&mock_tab()))
      .WillOnce(Return(&existing_instance));
  EXPECT_CALL(mock_service(), TryPreload(_)).Times(0);
  endpoint().Prepare(mock_tab());
  testing::Mock::VerifyAndClearExpectations(&mock_service());

  // Disabled profile: should not preload.
  DisableGlicEnablement();
  EXPECT_CALL(mock_service(), TryPreload(_)).Times(0);
  endpoint().Prepare(mock_tab());
}

TEST_F(GeminiPromptEndpointSubmitTest, SubmitIgnoredWhenNotAllowed) {
  DisableGlicEnablement();
  EXPECT_CALL(mock_service(), Invoke(_)).Times(0);
  EXPECT_CALL(mock_service(), InvokeWithAutoSubmit(_, _)).Times(0);

  endpoint().Submit(mock_tab(), "Explain this",
                    ::selection::PageContextWithSelection());
}

TEST_F(GeminiPromptEndpointSubmitTest,
       SubmitEmptyPromptCallsInvokeInsteadOfAutoSubmit) {
  std::optional<GlicInvokeOptions> captured;
  EXPECT_CALL(mock_service(), InvokeWithAutoSubmit(_, _)).Times(0);
  EXPECT_CALL(mock_service(), Invoke(_))
      .WillOnce([&](GlicInvokeOptions options) {
        captured = std::move(options);
        return nullptr;
      });

  ::selection::PageContext page_context;
  page_context.screenshot = MakeBitmap(40, 50);
  ::selection::PageContext::EncodedScreenshot encoded;
  encoded.data = std::vector<uint8_t>{0xAA, 0xBB, 0xCC};
  encoded.mime_type = "image/jpeg";
  encoded.size = gfx::Size(40, 50);
  page_context.encoded_screenshot = std::move(encoded);

  ::selection::AreaOfInterest aoi;
  aoi.bounds = gfx::Rect(5, 5, 10, 10);

  endpoint().Submit(mock_tab(), "",
                    ::selection::PageContextWithSelection(
                        std::move(page_context), std::move(aoi)));

  ASSERT_TRUE(captured.has_value());
  EXPECT_TRUE(captured->prompts.empty());
  ASSERT_TRUE(captured->additional_context.has_value());
  ASSERT_TRUE(captured->additional_context->context);
  EXPECT_EQ(captured->additional_context->context->source,
            mojom::AdditionalContextSource::kRegionSelection);
}

TEST_F(GeminiPromptEndpointSubmitTest,
       SubmitPreEncodedScreenshotFastPathAttachesClampedRectAndContext) {
  std::optional<GlicInvokeOptions> captured;
  EXPECT_CALL(mock_service(), Invoke(_)).Times(0);
  EXPECT_CALL(mock_service(), InvokeWithAutoSubmit(_, _))
      .WillOnce([&](InvokeWithAutoSubmitPasskey, GlicInvokeOptions options) {
        captured = std::move(options);
        return nullptr;
      });

  ::selection::PageContext page_context;
  page_context.screenshot = MakeBitmap(40, 50);
  page_context.apc.set_version(
      optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  ::selection::PageContext::EncodedScreenshot encoded;
  encoded.data = std::vector<uint8_t>{0x52, 0x49, 0x46, 0x46};
  encoded.mime_type = "image/webp";
  encoded.size = gfx::Size(40, 50);
  page_context.encoded_screenshot = std::move(encoded);

  ::selection::AreaOfInterest aoi;
  // Partially extends outside the 40x50 screenshot and must be clamped.
  aoi.bounds = gfx::Rect(-5, 10, 60, 100);

  endpoint().Submit(mock_tab(), "Explain this chart",
                    ::selection::PageContextWithSelection(
                        std::move(page_context), std::move(aoi)));

  // Synchronous fast path: invoked immediately without pumping ThreadPool.
  ASSERT_TRUE(captured.has_value());
  EXPECT_THAT(captured->prompts, ElementsAre("Explain this chart"));
  ASSERT_TRUE(captured->additional_context.has_value());
  const mojom::AdditionalContextPtr& additional =
      captured->additional_context->context;
  ASSERT_TRUE(additional);
  EXPECT_EQ(additional->source,
            mojom::AdditionalContextSource::kRegionSelection);
  ASSERT_EQ(additional->parts.size(), 2u);

  ASSERT_TRUE(additional->parts[0]->is_tab_context());
  const mojom::TabContextResultPtr& tab_context =
      additional->parts[0]->get_tab_context();
  ASSERT_TRUE(tab_context->viewport_screenshot);
  EXPECT_EQ(tab_context->viewport_screenshot->width_pixels, 40u);
  EXPECT_EQ(tab_context->viewport_screenshot->height_pixels, 50u);
  EXPECT_EQ(tab_context->viewport_screenshot->mime_type, "image/webp");
  EXPECT_EQ(tab_context->viewport_screenshot->data,
            (std::vector<uint8_t>{0x52, 0x49, 0x46, 0x46}));
  EXPECT_TRUE(tab_context->annotated_page_data);

  ASSERT_TRUE(additional->parts[1]->is_pending_region());
  const mojom::CapturedRegionPtr& region =
      additional->parts[1]->get_pending_region()->region;
  ASSERT_TRUE(region->is_rect());
  EXPECT_EQ(region->get_rect(), gfx::Rect(0, 10, 40, 40));
}

TEST_F(GeminiPromptEndpointSubmitTest,
       SubmitTextSelectionSkipsAdditionalContext) {
  std::optional<GlicInvokeOptions> captured;
  EXPECT_CALL(mock_service(), InvokeWithAutoSubmit(_, _))
      .WillOnce([&](InvokeWithAutoSubmitPasskey, GlicInvokeOptions options) {
        captured = std::move(options);
        return nullptr;
      });

  ::selection::PageContext page_context;
  page_context.screenshot = MakeBitmap(40, 50);
  ::selection::AreaOfInterest aoi;
  aoi.bounds = gfx::Rect(5, 5, 10, 10);
  aoi.is_text_selection = true;

  endpoint().Submit(mock_tab(), "Summarize",
                    ::selection::PageContextWithSelection(
                        std::move(page_context), std::move(aoi)));

  ASSERT_TRUE(captured.has_value());
  EXPECT_THAT(captured->prompts, ElementsAre("Summarize"));
  EXPECT_FALSE(captured->additional_context.has_value());
}

TEST_F(GeminiPromptEndpointSubmitTest,
       SubmitOutOfBoundsRectOmitsAdditionalContext) {
  std::optional<GlicInvokeOptions> captured;
  EXPECT_CALL(mock_service(), InvokeWithAutoSubmit(_, _))
      .WillOnce([&](InvokeWithAutoSubmitPasskey, GlicInvokeOptions options) {
        captured = std::move(options);
        return nullptr;
      });

  ::selection::PageContext page_context;
  page_context.screenshot = MakeBitmap(40, 50);
  ::selection::AreaOfInterest aoi;
  aoi.bounds = gfx::Rect(100, 100, 10, 10);

  endpoint().Submit(mock_tab(), "What is this?",
                    ::selection::PageContextWithSelection(
                        std::move(page_context), std::move(aoi)));

  ASSERT_TRUE(captured.has_value());
  EXPECT_THAT(captured->prompts, ElementsAre("What is this?"));
  EXPECT_FALSE(captured->additional_context.has_value());
}

TEST_F(GeminiPromptEndpointSubmitTest, SubmitPolylineRespectsFlagAndBounds) {
  const std::vector<gfx::Point> valid_points = {gfx::Point(0, 0),
                                                gfx::Point(39, 49)};

  // Enabled + in-bounds polyline attaches pending polyline region.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kGlicRegionSelectionLine);

    std::optional<GlicInvokeOptions> captured;
    EXPECT_CALL(mock_service(), InvokeWithAutoSubmit(_, _))
        .WillOnce([&](InvokeWithAutoSubmitPasskey, GlicInvokeOptions options) {
          captured = std::move(options);
          return nullptr;
        });

    ::selection::PageContext page_context;
    page_context.screenshot = MakeBitmap(40, 50);
    ::selection::PageContext::EncodedScreenshot encoded;
    encoded.data = std::vector<uint8_t>{1, 2, 3};
    encoded.mime_type = "image/jpeg";
    encoded.size = gfx::Size(40, 50);
    page_context.encoded_screenshot = std::move(encoded);

    ::selection::AreaOfInterest aoi;
    aoi.bounds = valid_points;

    endpoint().Submit(mock_tab(), "Circle this",
                      ::selection::PageContextWithSelection(
                          std::move(page_context), std::move(aoi)));

    ASSERT_TRUE(captured.has_value());
    ASSERT_TRUE(captured->additional_context.has_value());
    ASSERT_TRUE(captured->additional_context->context);
    ASSERT_EQ(captured->additional_context->context->parts.size(), 2u);
    ASSERT_TRUE(
        captured->additional_context->context->parts[1]->is_pending_region());
    const mojom::CapturedRegionPtr& region =
        captured->additional_context->context->parts[1]
            ->get_pending_region()
            ->region;
    ASSERT_TRUE(region->is_polyline());
    EXPECT_EQ(region->get_polyline(), valid_points);
    testing::Mock::VerifyAndClearExpectations(&mock_service());
  }

  // Enabled + out-of-bounds point (x == width) omits additional context.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kGlicRegionSelectionLine);

    std::optional<GlicInvokeOptions> captured;
    EXPECT_CALL(mock_service(), InvokeWithAutoSubmit(_, _))
        .WillOnce([&](InvokeWithAutoSubmitPasskey, GlicInvokeOptions options) {
          captured = std::move(options);
          return nullptr;
        });

    ::selection::PageContext page_context;
    page_context.screenshot = MakeBitmap(40, 50);
    ::selection::AreaOfInterest aoi;
    aoi.bounds = std::vector<gfx::Point>{gfx::Point(5, 5), gfx::Point(40, 10)};

    endpoint().Submit(mock_tab(), "OOB line",
                      ::selection::PageContextWithSelection(
                          std::move(page_context), std::move(aoi)));

    ASSERT_TRUE(captured.has_value());
    EXPECT_FALSE(captured->additional_context.has_value());
    testing::Mock::VerifyAndClearExpectations(&mock_service());
  }

  // Disabled feature flag omits additional context even for valid points.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kGlicRegionSelectionLine);

    std::optional<GlicInvokeOptions> captured;
    EXPECT_CALL(mock_service(), InvokeWithAutoSubmit(_, _))
        .WillOnce([&](InvokeWithAutoSubmitPasskey, GlicInvokeOptions options) {
          captured = std::move(options);
          return nullptr;
        });

    ::selection::PageContext page_context;
    page_context.screenshot = MakeBitmap(40, 50);
    ::selection::AreaOfInterest aoi;
    aoi.bounds = valid_points;

    endpoint().Submit(mock_tab(), "Disabled line",
                      ::selection::PageContextWithSelection(
                          std::move(page_context), std::move(aoi)));

    ASSERT_TRUE(captured.has_value());
    EXPECT_FALSE(captured->additional_context.has_value());
  }
}

TEST_F(GeminiPromptEndpointSubmitTest,
       SubmitEncodesScreenshotAsyncWhenPreEncodedAbsent) {
  base::RunLoop run_loop;
  std::optional<GlicInvokeOptions> captured;
  EXPECT_CALL(mock_service(), InvokeWithAutoSubmit(_, _))
      .WillOnce([&](InvokeWithAutoSubmitPasskey, GlicInvokeOptions options) {
        captured = std::move(options);
        run_loop.Quit();
        return nullptr;
      });

  ::selection::PageContext page_context;
  page_context.screenshot = MakeBitmap(16, 12);
  ::selection::AreaOfInterest aoi;
  aoi.bounds = gfx::Rect(2, 2, 8, 6);

  endpoint().Submit(mock_tab(), "Async encode",
                    ::selection::PageContextWithSelection(
                        std::move(page_context), std::move(aoi)));

  // Not yet invoked synchronously; encoding runs on ThreadPool.
  EXPECT_FALSE(captured.has_value());

  run_loop.Run();
  ASSERT_TRUE(captured.has_value());
  EXPECT_THAT(captured->prompts, ElementsAre("Async encode"));
  ASSERT_TRUE(captured->additional_context.has_value());
  const mojom::AdditionalContextPtr& additional =
      captured->additional_context->context;
  ASSERT_TRUE(additional);
  ASSERT_EQ(additional->parts.size(), 2u);
  ASSERT_TRUE(additional->parts[0]->is_tab_context());
  const mojom::TabContextResultPtr& tab_context =
      additional->parts[0]->get_tab_context();
  ASSERT_TRUE(tab_context->viewport_screenshot);
  EXPECT_EQ(tab_context->viewport_screenshot->mime_type, "image/jpeg");
  EXPECT_EQ(tab_context->viewport_screenshot->width_pixels, 16u);
  EXPECT_EQ(tab_context->viewport_screenshot->height_pixels, 12u);
  EXPECT_FALSE(tab_context->viewport_screenshot->data.empty());

  ASSERT_TRUE(additional->parts[1]->is_pending_region());
  const mojom::CapturedRegionPtr& region =
      additional->parts[1]->get_pending_region()->region;
  ASSERT_TRUE(region->is_rect());
  EXPECT_EQ(region->get_rect(), gfx::Rect(2, 2, 8, 6));
}

TEST_F(GeminiPromptEndpointSubmitTest,
       CancelPendingRequestsSuppressesInFlightAsyncReply) {
  EXPECT_CALL(mock_service(), Invoke(_)).Times(0);
  EXPECT_CALL(mock_service(), InvokeWithAutoSubmit(_, _)).Times(0);

  ::selection::PageContext page_context;
  page_context.screenshot = MakeBitmap(16, 12);
  ::selection::AreaOfInterest aoi;
  aoi.bounds = gfx::Rect(2, 2, 8, 6);

  endpoint().Submit(mock_tab(), "Will cancel",
                    ::selection::PageContextWithSelection(
                        std::move(page_context), std::move(aoi)));
  endpoint().CancelPendingRequests();

  FlushAsyncScreenshotEncode();
}

TEST_F(GeminiPromptEndpointSubmitTest,
       SubsequentSubmitCancelsPreviousAsyncRequest) {
  std::vector<std::string> submitted_prompts;
  EXPECT_CALL(mock_service(), InvokeWithAutoSubmit(_, _))
      .WillOnce([&](InvokeWithAutoSubmitPasskey, GlicInvokeOptions options) {
        if (!options.prompts.empty()) {
          submitted_prompts.push_back(options.prompts[0]);
        }
        return nullptr;
      });

  ::selection::PageContext async_context;
  async_context.screenshot = MakeBitmap(16, 12);
  ::selection::AreaOfInterest region_aoi;
  region_aoi.bounds = gfx::Rect(2, 2, 8, 6);
  endpoint().Submit(mock_tab(), "Stale async prompt",
                    ::selection::PageContextWithSelection(
                        std::move(async_context), std::move(region_aoi)));

  // Second submission completes synchronously (text-only) and invalidates the
  // first in-flight encode callback.
  endpoint().Submit(mock_tab(), "Latest prompt",
                    ::selection::PageContextWithSelection());

  FlushAsyncScreenshotEncode();
  EXPECT_THAT(submitted_prompts, ElementsAre("Latest prompt"));
}

}  // namespace
}  // namespace glic
