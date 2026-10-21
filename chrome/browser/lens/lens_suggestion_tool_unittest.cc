// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lens/lens_suggestion_tool.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "chrome/browser/lens/core/mojom/geometry.mojom.h"
#include "chrome/browser/selection/mojom/action.mojom.h"
#include "chrome/browser/selection/suggestion.h"
#include "chrome/browser/selection/suggestion_service.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/optimization_guide/proto/features/smart_selection_suggestions.pb.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/generic_pending_associated_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace lens {
namespace {

using ::testing::_;
using ::testing::ReturnRef;

class MockLensSearchController : public LensSearchController {
 public:
  explicit MockLensSearchController(tabs::TabInterface* tab)
      : LensSearchController(tab) {}
  ~MockLensSearchController() override = default;

  MOCK_METHOD(void,
              OpenLensOverlayWithPendingRegion,
              (lens::LensOverlayInvocationSource invocation_source,
               lens::mojom::CenterRotatedBoxPtr region,
               const SkBitmap& region_bitmap),
              (override));
};

class LensSuggestionToolTest : public testing::Test {
 public:
  LensSuggestionToolTest() {
    ON_CALL(mock_tab_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(user_data_host_));
    service_.emplace(&mock_tab_, /*remote_model_executor=*/nullptr,
                     /*google_groups_manager=*/nullptr);
    lens_search_controller_.emplace(&mock_tab_);
    tool_.emplace(mock_tab_);
  }
  ~LensSuggestionToolTest() override = default;

 protected:
  tabs::MockTabInterface& mock_tab() { return mock_tab_; }
  MockLensSearchController& lens_search_controller() {
    return *lens_search_controller_;
  }
  LensSuggestionTool& tool() { return *tool_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ui::UnownedUserDataHost user_data_host_;
  tabs::MockTabInterface mock_tab_;
  std::optional<::selection::SuggestionService> service_;
  std::optional<MockLensSearchController> lens_search_controller_;
  std::optional<LensSuggestionTool> tool_;
};

TEST_F(LensSuggestionToolTest, GetToolId) {
  EXPECT_EQ(tool().GetToolId(),
            optimization_guide::proto::SMART_SELECTION_TOOL_GOOGLE_LENS);
}

TEST_F(LensSuggestionToolTest, RegistersAndUnregistersWithService) {
  ui::UnownedUserDataHost other_host;
  tabs::MockTabInterface other_tab;
  ON_CALL(other_tab, GetUnownedUserDataHost())
      .WillByDefault(ReturnRef(other_host));
  ::selection::SuggestionService other_service{
      &other_tab, /*remote_model_executor=*/nullptr,
      /*google_groups_manager=*/nullptr};

  {
    const LensSuggestionTool scoped_tool{other_tab};
  }

  // Re-registering with the same ToolId succeeds because `scoped_tool`
  // unregistered itself on destruction.
  const LensSuggestionTool second_tool{other_tab};
}

TEST_F(LensSuggestionToolTest, CreateSuggestionWithEmptyLabelUsesFallback) {
  ::selection::AreaOfInterest aoi;
  aoi.bounds = gfx::Rect(10, 20, 30, 40);
  optimization_guide::proto::SmartSelectionSuggestion server_suggestion;
  server_suggestion.set_label("");

  std::unique_ptr<::selection::Suggestion> suggestion =
      tool().CreateSuggestion(aoi, server_suggestion);
  ASSERT_NE(suggestion, nullptr);
  EXPECT_EQ(suggestion->GetLabel(), u"Search with Google Lens");
}

TEST_F(LensSuggestionToolTest, CreateSuggestionWithEmptyBoundsReturnsNullptr) {
  ::selection::AreaOfInterest aoi;
  aoi.bounds = gfx::Rect();
  optimization_guide::proto::SmartSelectionSuggestion server_suggestion;
  server_suggestion.set_label("Search with Google Lens");

  EXPECT_EQ(tool().CreateSuggestion(aoi, server_suggestion), nullptr);
}

TEST_F(LensSuggestionToolTest,
       ExecuteSuggestionWithRectBoundsOpensLensOverlay) {
  ::selection::AreaOfInterest aoi;
  aoi.screenshot.allocN32Pixels(200, 100);
  aoi.screenshot.eraseColor(SK_ColorBLUE);
  aoi.bounds = gfx::Rect(20, 10, 60, 40);

  optimization_guide::proto::SmartSelectionSuggestion server_suggestion;
  server_suggestion.set_label("Search with Google Lens");

  std::unique_ptr<::selection::Suggestion> suggestion =
      tool().CreateSuggestion(aoi, server_suggestion);
  ASSERT_NE(suggestion, nullptr);
  EXPECT_EQ(suggestion->GetLabel(), u"Search with Google Lens");
  EXPECT_EQ(suggestion->GetToolId(),
            optimization_guide::proto::SMART_SELECTION_TOOL_GOOGLE_LENS);
  ASSERT_TRUE(suggestion->GetAction()->is_handoff());

  base::RunLoop run_loop;
  EXPECT_CALL(
      lens_search_controller(),
      OpenLensOverlayWithPendingRegion(
          lens::LensOverlayInvocationSource::kSelectionSuggestion, _, _))
      .WillOnce([&run_loop](lens::LensOverlayInvocationSource invocation_source,
                            lens::mojom::CenterRotatedBoxPtr region,
                            const SkBitmap& region_bitmap) {
        ASSERT_TRUE(region);
        EXPECT_EQ(region->coordinate_type,
                  lens::mojom::CenterRotatedBox_CoordinateType::kNormalized);
        // Center of (20, 10, 60, 40) in (200, 100) is (50/200, 30/100) =
        // (0.25, 0.3), size is (60/200, 40/100) = (0.3, 0.4).
        EXPECT_FLOAT_EQ(region->box.x(), 0.25f);
        EXPECT_FLOAT_EQ(region->box.y(), 0.3f);
        EXPECT_FLOAT_EQ(region->box.width(), 0.3f);
        EXPECT_FLOAT_EQ(region->box.height(), 0.4f);
        EXPECT_EQ(region_bitmap.width(), 60);
        EXPECT_EQ(region_bitmap.height(), 40);
        run_loop.Quit();
      });

  suggestion->Execute(mojo::GenericPendingAssociatedReceiver());
  run_loop.Run();
}

TEST_F(LensSuggestionToolTest,
       ExecuteSuggestionWithPolylineBoundsOpensLensOverlay) {
  ::selection::AreaOfInterest aoi;
  aoi.screenshot.allocN32Pixels(100, 100);
  aoi.screenshot.eraseColor(SK_ColorGREEN);
  aoi.bounds = std::vector<gfx::Point>{
      gfx::Point(10, 20),
      gfx::Point(50, 80),
      gfx::Point(30, 40),
  };

  optimization_guide::proto::SmartSelectionSuggestion server_suggestion;
  server_suggestion.set_label("Search region");

  std::unique_ptr<::selection::Suggestion> suggestion =
      tool().CreateSuggestion(aoi, server_suggestion);
  ASSERT_NE(suggestion, nullptr);

  base::RunLoop run_loop;
  EXPECT_CALL(
      lens_search_controller(),
      OpenLensOverlayWithPendingRegion(
          lens::LensOverlayInvocationSource::kSelectionSuggestion, _, _))
      .WillOnce([&run_loop](lens::LensOverlayInvocationSource invocation_source,
                            lens::mojom::CenterRotatedBoxPtr region,
                            const SkBitmap& region_bitmap) {
        ASSERT_TRUE(region);
        // Bounding rect of (10,20), (50,80), (30,40) is (10, 20, 40, 60).
        // Center in (100, 100) is (30/100, 50/100) = (0.3, 0.5),
        // size is (40/100, 60/100) = (0.4, 0.6).
        EXPECT_FLOAT_EQ(region->box.x(), 0.3f);
        EXPECT_FLOAT_EQ(region->box.y(), 0.5f);
        EXPECT_FLOAT_EQ(region->box.width(), 0.4f);
        EXPECT_FLOAT_EQ(region->box.height(), 0.6f);
        EXPECT_EQ(region_bitmap.width(), 40);
        EXPECT_EQ(region_bitmap.height(), 60);
        run_loop.Quit();
      });

  suggestion->Execute(mojo::GenericPendingAssociatedReceiver());
  run_loop.Run();
}

}  // namespace
}  // namespace lens
