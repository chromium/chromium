// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_animation_view.h"

#include <memory>

#include "ash/ambient/model/ambient_animation_photo_config.h"
#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/test/ambient_test_util.h"
#include "ash/ambient/test/fake_ambient_animation_static_resources.h"
#include "ash/ambient/test/mock_ambient_view_event_handler.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/paint/skottie_wrapper.h"
#include "cc/test/lottie_test_data.h"
#include "cc/test/skia_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace ash {

using ::testing::_;
using ::testing::Mock;
using ::testing::NotNull;

namespace {

const cc::DrawSkottieOp* FindSkottieOp(
    const cc::PaintOpBuffer& paint_op_buffer) {
  for (const cc::PaintOp* op : cc::PaintOpBuffer::Iterator(&paint_op_buffer)) {
    if (op->GetType() == cc::PaintOpType::DrawSkottie)
      return static_cast<const cc::DrawSkottieOp*>(op);

    if (op->GetType() == cc::PaintOpType::DrawRecord) {
      const cc::DrawSkottieOp* skottie_op =
          FindSkottieOp(*static_cast<const cc::DrawRecordOp*>(op)->record);
      if (skottie_op)
        return skottie_op;
    }
  }
  return nullptr;
}

}  // namespace

class AmbientAnimationViewTest : public views::ViewsTestBase {
 protected:
  AmbientAnimationViewTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ViewsTestBase::SetUp();

    auto static_resources =
        std::make_unique<FakeAmbientAnimationStaticResources>();
    static_resources->SetLottieData(cc::CreateCustomLottieDataWith2Assets(
        GenerateTestLottieDynamicAssetId(/*unique_id=*/0),
        GenerateTestLottieDynamicAssetId(/*unique_id=*/1)));

    model_ =
        std::make_unique<AmbientBackendModel>(CreateAmbientAnimationPhotoConfig(
            cc::CreateSkottieFromString(static_resources->GetLottieData())
                ->GetImageAssetMetadata()));
    PhotoWithDetails image;
    image.photo = gfx::test::CreateImageSkia(100, 100);
    model_->AddNextImage(image);
    model_->AddNextImage(image);

    widget_ = CreateTestWidget();
    view_ = widget_->GetRootView()->AddChildView(
        std::make_unique<AmbientAnimationView>(model_.get(), &event_handler_,
                                               std::move(static_resources)));
  }

  MockAmbientViewEventHandler event_handler_;
  std::unique_ptr<AmbientBackendModel> model_;
  std::unique_ptr<views::Widget> widget_;
  AmbientAnimationView* view_;
};

// Flaky: crbug.com/1287542
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_NotifiesDelegateOfAnimationCycleMarkers \
  DISABLED_NotifiesDelegateOfAnimationCycleMarkers
#else
#define MAYBE_NotifiesDelegateOfAnimationCycleMarkers \
  NotifiesDelegateOfAnimationCycleMarkers
#endif
TEST_F(AmbientAnimationViewTest,
       MAYBE_NotifiesDelegateOfAnimationCycleMarkers) {
  view_->SetBoundsRect(widget_->GetWindowBoundsInScreen());
  widget_->Show();

  EXPECT_CALL(event_handler_,
              OnMarkerHit(AmbientPhotoConfig::Marker::kUiStartRendering));
  task_environment()->FastForwardBy(cc::kLottieDataWith2AssetsDuration * 0.1);
  Mock::VerifyAndClearExpectations(&event_handler_);

  EXPECT_CALL(event_handler_,
              OnMarkerHit(AmbientPhotoConfig::Marker::kUiCycleEnded));
  task_environment()->FastForwardBy(cc::kLottieDataWith2AssetsDuration);
  Mock::VerifyAndClearExpectations(&event_handler_);

  EXPECT_CALL(event_handler_,
              OnMarkerHit(AmbientPhotoConfig::Marker::kUiCycleEnded));
  task_environment()->FastForwardBy(cc::kLottieDataWith2AssetsDuration);
  Mock::VerifyAndClearExpectations(&event_handler_);
}

TEST_F(AmbientAnimationViewTest, WaitsForViewBoundariesBeforeRendering) {
  gfx::Rect widget_bounds = widget_->GetWindowBoundsInScreen();

  widget_->Show();

  EXPECT_CALL(event_handler_, OnMarkerHit(_)).Times(0);
  task_environment()->FastForwardBy(cc::kLottieDataWith2AssetsDuration * 0.1);
  Mock::VerifyAndClearExpectations(&event_handler_);

  view_->SetBoundsRect(widget_bounds);

  EXPECT_CALL(event_handler_,
              OnMarkerHit(AmbientPhotoConfig::Marker::kUiStartRendering));
  task_environment()->FastForwardBy(cc::kLottieDataWith2AssetsDuration * 0.1);
  Mock::VerifyAndClearExpectations(&event_handler_);
}

}  // namespace ash
