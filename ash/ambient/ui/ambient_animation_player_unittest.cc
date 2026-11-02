// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_animation_player.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "ash/ambient/ui/ambient_animation_progress_tracker.h"
#include "ash/test/ash_test_base.h"
#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "cc/paint/skottie_marker.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "cc/paint/skottie_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkSize.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/lottie/animation.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

using ::testing::Eq;

constexpr gfx::Size kTestAnimationSize{100, 100};

// Tailor-made for creating an animation with custom markers without having to
// create full-blown valid lottie JSON data with markers embedded in it.
class TestSkottieWrapper : public cc::SkottieWrapper {
 public:
  TestSkottieWrapper(base::TimeDelta duration,
                     std::vector<cc::SkottieMarker> markers)
      : duration_(duration), markers_(std::move(markers)) {}
  TestSkottieWrapper(const TestSkottieWrapper&) = delete;
  TestSkottieWrapper& operator=(const TestSkottieWrapper&) = delete;

  // cc::SkottieWrapper implementation:
  bool is_valid() const override { return true; }
  const cc::SkottieResourceMetadataMap& GetImageAssetMetadata() const override {
    return image_asset_map_;
  }
  const base::flat_set<std::string>& GetTextNodeNames() const override {
    return text_node_names_;
  }
  cc::SkottieTextPropertyValueMap GetCurrentTextPropertyValues()
      const override {
    return cc::SkottieTextPropertyValueMap();
  }
  cc::SkottieTransformPropertyValueMap GetCurrentTransformPropertyValues()
      const override {
    return cc::SkottieTransformPropertyValueMap();
  }
  cc::SkottieColorMap GetCurrentColorPropertyValues() const override {
    return cc::SkottieColorMap();
  }
  const std::vector<cc::SkottieMarker>& GetAllMarkers() const override {
    return markers_;
  }
  void Seek(float t, FrameDataCallback frame_data_cb) override {}
  void Draw(SkCanvas* canvas,
            float t,
            const SkRect& rect,
            FrameDataCallback frame_data_cb,
            const cc::SkottieColorMap& color_map,
            const cc::SkottieTextPropertyValueMap& text_map) override {}
  float duration() const override { return duration_.InSecondsF(); }
  float GetNormalizedTimeForFrame(float frame) const override { return 0.f; }
  SkSize size() const override {
    return gfx::SizeFToSkSize(gfx::SizeF(kTestAnimationSize));
  }
  base::span<const uint8_t> raw_data() const override {
    return base::span<const uint8_t>();
  }
  uint32_t id() const override { return 0; }

 private:
  ~TestSkottieWrapper() override = default;

  const base::TimeDelta duration_;
  const base::flat_set<std::string> text_node_names_;
  const cc::SkottieResourceMetadataMap image_asset_map_;
  const std::vector<cc::SkottieMarker> markers_;
};

class AmbientAnimationPlayerTest : public AshTestBase {
 protected:
  AmbientAnimationPlayerTest()
      : clock_(base::TimeTicks::Now()),
        canvas_(kTestAnimationSize, /*image_scale=*/1.f, /*is_opaque=*/false) {}

  void SetUp() override {
    AshTestBase::SetUp();
    widget_ = CreateFramelessTestWidget();
    widget_->SetContentsView(std::make_unique<views::View>());
  }

  void TearDown() override {
    widget_.reset();
    AshTestBase::TearDown();
  }

  void AdvanceTimeAndPaint(lottie::Animation& animation,
                           float normalized_amount) {
    AdvanceTimeAndPaint({&animation}, normalized_amount);
  }

  void AdvanceTimeAndPaint(std::vector<lottie::Animation*> animations,
                           float normalized_amount) {
    CHECK(!animations.empty());
    CHECK_GE(normalized_amount, 0.f);
    CHECK_LE(normalized_amount, 1.f);
    clock_ += (normalized_amount * animations.front()->GetAnimationDuration());
    for (lottie::Animation* animation : animations) {
      animation->Paint(&canvas_, clock_, kTestAnimationSize);
    }
  }

  // The returned view is owned by the widget's contents view.
  views::AnimatedImageView* CreateView(std::vector<cc::SkottieMarker> markers) {
    auto view = std::make_unique<views::AnimatedImageView>();
    view->SetAnimatedImage(std::make_unique<lottie::Animation>(
        base::MakeRefCounted<TestSkottieWrapper>(base::Seconds(1),
                                                 std::move(markers))));
    return widget_->GetContentsView()->AddChildView(std::move(view));
  }

  std::unique_ptr<AmbientAnimationPlayer> CreatePlayer(
      views::AnimatedImageView* view) {
    return std::make_unique<AmbientAnimationPlayer>(view, &progress_tracker_);
  }

  base::TimeTicks clock_;
  gfx::Canvas canvas_;
  std::unique_ptr<views::Widget> widget_;
  AmbientAnimationProgressTracker progress_tracker_;
};

TEST_F(AmbientAnimationPlayerTest, BasicThrottling) {
  views::AnimatedImageView* view = CreateView({
      {"_CrOS_Marker_Throttled_20fps", 0.2f, 0.4f},
      {"_CrOS_Marker_Throttled_30fps", 0.8f, 0.9f},
  });
  lottie::Animation& animation = *view->animated_image();
  auto player = CreatePlayer(view);

  // T: 0
  AdvanceTimeAndPaint(animation, .0f);

  // T: .1
  AdvanceTimeAndPaint(animation, .1f);
  EXPECT_THAT(view->GetFrameInterval(), Eq(std::nullopt));

  // T: .21
  AdvanceTimeAndPaint(animation, .21f - .1f);
  EXPECT_THAT(view->GetFrameInterval(), Eq(base::Hertz(20)));

  // T: .39
  AdvanceTimeAndPaint(animation, .39f - .21f);
  EXPECT_THAT(view->GetFrameInterval(), Eq(base::Hertz(20)));

  // T: .41
  AdvanceTimeAndPaint(animation, .41f - .39f);
  EXPECT_THAT(view->GetFrameInterval(), Eq(std::nullopt));

  // T: .79
  AdvanceTimeAndPaint(animation, .79f - .41f);
  EXPECT_THAT(view->GetFrameInterval(), Eq(std::nullopt));

  // T: .81
  AdvanceTimeAndPaint(animation, .81f - .79f);
  EXPECT_THAT(view->GetFrameInterval(), Eq(base::Hertz(30)));

  // T: .89
  AdvanceTimeAndPaint(animation, .89f - .81f);
  EXPECT_THAT(view->GetFrameInterval(), Eq(base::Hertz(30)));

  // T: .91
  AdvanceTimeAndPaint(animation, .91f - .89f);
  EXPECT_THAT(view->GetFrameInterval(), Eq(std::nullopt));

  // T: .1
  AdvanceTimeAndPaint(animation, 1.1f - .9f);
  EXPECT_THAT(view->GetFrameInterval(), Eq(std::nullopt));

  // T: .21
  AdvanceTimeAndPaint(animation, .21f - .1f);
  EXPECT_THAT(view->GetFrameInterval(), Eq(base::Hertz(20)));
}

TEST_F(AmbientAnimationPlayerTest, MarkersAtBordersOfAnimationCycle) {
  views::AnimatedImageView* view = CreateView({
      {"_CrOS_Marker_Throttled_20fps", 0.f, 0.1f},
      {"_CrOS_Marker_Throttled_30fps", 0.9f, 1.f},
  });
  lottie::Animation& animation = *view->animated_image();
  auto player = CreatePlayer(view);

  // T: 0
  AdvanceTimeAndPaint(animation, .0f);
  EXPECT_THAT(view->GetFrameInterval(), Eq(base::Hertz(20)));

  // T: .91
  AdvanceTimeAndPaint(animation, .91f);
  EXPECT_THAT(view->GetFrameInterval(), Eq(base::Hertz(30)));

  // T: .01
  AdvanceTimeAndPaint(animation, 1.01f - .91f);
  EXPECT_THAT(view->GetFrameInterval(), Eq(base::Hertz(20)));
}

TEST_F(AmbientAnimationPlayerTest, NoMarkers) {
  views::AnimatedImageView* view = CreateView({});
  lottie::Animation& animation = *view->animated_image();
  auto player = CreatePlayer(view);

  constexpr int kNumTimeStepsToTest = 30;
  constexpr float kTimeStepSize = .1f;
  for (int i = 0; i < kNumTimeStepsToTest; ++i) {
    AdvanceTimeAndPaint(animation, kTimeStepSize);
    EXPECT_THAT(view->GetFrameInterval(), Eq(std::nullopt));
  }
}

// The time steps in this test skip multiple markers at a time.
TEST_F(AmbientAnimationPlayerTest, LargeTimesteps) {
  views::AnimatedImageView* view = CreateView({
      {"_CrOS_Marker_Throttled_10fps", 0.f, 0.1f},
      {"_CrOS_Marker_Throttled_20fps", 0.1f, 0.2f},
      {"_CrOS_Marker_Throttled_30fps", 0.2f, 0.3f},
      {"_CrOS_Marker_Throttled_40fps", 0.3f, 0.4f},
      {"_CrOS_Marker_Throttled_50fps", 0.4f, 0.5f},
  });
  lottie::Animation& animation = *view->animated_image();
  auto player = CreatePlayer(view);

  // T: 0
  AdvanceTimeAndPaint(animation, .0f);
  EXPECT_THAT(view->GetFrameInterval(), Eq(base::Hertz(10)));

  // T: .25
  AdvanceTimeAndPaint(animation, .25f);
  EXPECT_THAT(view->GetFrameInterval(), Eq(base::Hertz(30)));

  // T: .45
  AdvanceTimeAndPaint(animation, .45f - .25f);
  EXPECT_THAT(view->GetFrameInterval(), Eq(base::Hertz(50)));
}

TEST_F(AmbientAnimationPlayerTest, MultipleViews) {
  const std::vector<cc::SkottieMarker> markers = {
      {"_CrOS_Marker_Throttled_10fps", 0.4f, 0.6f},
  };
  views::AnimatedImageView* view_1 = CreateView(markers);
  views::AnimatedImageView* view_2 = CreateView(markers);
  lottie::Animation& animation_1 = *view_1->animated_image();
  lottie::Animation& animation_2 = *view_2->animated_image();
  auto player_1 = CreatePlayer(view_1);

  // T: 0
  AdvanceTimeAndPaint(animation_1, .0f);
  // T: .41
  AdvanceTimeAndPaint(animation_1, .41f);
  EXPECT_THAT(view_1->GetFrameInterval(), Eq(base::Hertz(10)));
  EXPECT_THAT(view_2->GetFrameInterval(), Eq(std::nullopt));

  // T: .59
  AdvanceTimeAndPaint(animation_1, .59f - .41f);
  // The second player starts from the existing animation's timestamp.
  auto player_2 = CreatePlayer(view_2);
  AdvanceTimeAndPaint(animation_2, .0f);
  EXPECT_THAT(view_1->GetFrameInterval(), Eq(base::Hertz(10)));
  EXPECT_THAT(view_2->GetFrameInterval(), Eq(base::Hertz(10)));

  // T: .7
  AdvanceTimeAndPaint({&animation_1, &animation_2}, .7f - .59f);
  EXPECT_THAT(view_1->GetFrameInterval(), Eq(std::nullopt));
  EXPECT_THAT(view_2->GetFrameInterval(), Eq(std::nullopt));
}

TEST_F(AmbientAnimationPlayerTest, HandlesViewAndPlayerDestroyed) {
  const std::vector<cc::SkottieMarker> markers = {
      {"_CrOS_Marker_Throttled_10fps", 0.f, 0.2f},
  };
  views::AnimatedImageView* view_1 = CreateView(markers);
  views::AnimatedImageView* view_2 = CreateView(markers);
  auto player_1 = CreatePlayer(view_1);
  auto player_2 = CreatePlayer(view_2);

  // T: 0
  AdvanceTimeAndPaint({view_1->animated_image(), view_2->animated_image()},
                      .0f);
  ASSERT_THAT(view_1->GetFrameInterval(), Eq(base::Hertz(10)));
  ASSERT_THAT(view_2->GetFrameInterval(), Eq(base::Hertz(10)));

  player_1.reset();
  // Destroys `view_1`.
  widget_->GetContentsView()->RemoveChildViewT(view_1);
  // T: 0.05f
  AdvanceTimeAndPaint(*view_2->animated_image(), .05f);
  EXPECT_THAT(view_2->GetFrameInterval(), Eq(base::Hertz(10)));

  // The frame rate should be restored to default when the player is
  // destroyed.
  player_2.reset();
  EXPECT_THAT(view_2->GetFrameInterval(), Eq(std::nullopt));
}

TEST_F(AmbientAnimationPlayerTest, CycleRestartWithThrottling) {
  views::AnimatedImageView* view = CreateView({
      {"_CrOS_Marker_CycleRestart", 0.5f, 0.5f},
      {"_CrOS_Marker_Throttled_10fps", 0.6f, 0.8f},
  });
  auto player = CreatePlayer(view);

  // T: 0
  AdvanceTimeAndPaint(*view->animated_image(), .0f);
  EXPECT_THAT(view->GetFrameInterval(), Eq(std::nullopt));

  // T: .65
  AdvanceTimeAndPaint(*view->animated_image(), .65f);
  EXPECT_THAT(view->GetFrameInterval(), Eq(base::Hertz(10)));
}

}  // namespace
}  // namespace ash
