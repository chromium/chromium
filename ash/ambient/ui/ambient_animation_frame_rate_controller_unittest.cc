// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_animation_frame_rate_controller.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "ash/frame_throttler/frame_throttling_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "cc/paint/skottie_marker.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "cc/paint/skottie_wrapper.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkSize.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/lottie/animation.h"

namespace ash {
namespace {

using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

constexpr gfx::Size kTestAnimationSize = gfx::Size(100, 100);

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
  void Seek(float t);
  void Draw(SkCanvas* canvas,
            float t,
            const SkRect& rect,
            FrameDataCallback frame_data_cb,
            const cc::SkottieColorMap& color_map,
            const cc::SkottieTextPropertyValueMap& text_map) override {}
  float duration() const override { return duration_.InSecondsF(); }
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

class AmbientAnimationFrameRateControllerTest : public AshTestBase {
 protected:
  AmbientAnimationFrameRateControllerTest()
      : clock_(base::TimeTicks::Now()),
        canvas_(kTestAnimationSize, /*image_scale=*/1.f, /*is_opaque=*/false) {}

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

  base::TimeTicks clock_;
  gfx::Canvas canvas_;
};

TEST_F(AmbientAnimationFrameRateControllerTest, BasicThrottling) {
  constexpr viz::FrameSinkId kTestFrameSinkId = {99, 99};
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  window->SetEmbedFrameSinkId(kTestFrameSinkId);

  std::vector<cc::SkottieMarker> markers = {
      {"_CrOS_Marker_Throttled_20fps", 0.2f, 0.4f},
      {"_CrOS_Marker_Throttled_30fps", 0.8f, 0.9f},
  };
  lottie::Animation animation(
      base::MakeRefCounted<TestSkottieWrapper>(base::Seconds(1), markers));
  AmbientAnimationFrameRateController ambient_frame_rate_controller(
      Shell::Get()->frame_throttling_controller());
  ambient_frame_rate_controller.AddWindowToThrottle(window.get(), &animation);
  animation.Start(lottie::Animation::PlaybackConfig::CreateDefault(animation));

  // T: 0
  AdvanceTimeAndPaint(animation, .0f);

  // T: .1
  AdvanceTimeAndPaint(animation, .1f);
  EXPECT_THAT(
      Shell::Get()->frame_throttling_controller()->GetFrameSinkIdsToThrottle(),
      IsEmpty());

  // T: .21
  AdvanceTimeAndPaint(animation, .21f - .1f);
  EXPECT_THAT(
      Shell::Get()->frame_throttling_controller()->GetFrameSinkIdsToThrottle(),
      UnorderedElementsAre(kTestFrameSinkId));
  EXPECT_THAT(Shell::Get()
                  ->frame_throttling_controller()
                  ->GetCurrentThrottledFrameRate(),
              Eq(20));

  // T: .39
  AdvanceTimeAndPaint(animation, .39f - .21f);
  EXPECT_THAT(
      Shell::Get()->frame_throttling_controller()->GetFrameSinkIdsToThrottle(),
      UnorderedElementsAre(kTestFrameSinkId));
  EXPECT_THAT(Shell::Get()
                  ->frame_throttling_controller()
                  ->GetCurrentThrottledFrameRate(),
              Eq(20));

  // T: .41
  AdvanceTimeAndPaint(animation, .41f - .39f);
  EXPECT_THAT(
      Shell::Get()->frame_throttling_controller()->GetFrameSinkIdsToThrottle(),
      IsEmpty());

  // T: .79
  AdvanceTimeAndPaint(animation, .79f - .41f);
  EXPECT_THAT(
      Shell::Get()->frame_throttling_controller()->GetFrameSinkIdsToThrottle(),
      IsEmpty());

  // T: .81
  AdvanceTimeAndPaint(animation, .81f - .79f);
  EXPECT_THAT(
      Shell::Get()->frame_throttling_controller()->GetFrameSinkIdsToThrottle(),
      UnorderedElementsAre(kTestFrameSinkId));
  EXPECT_THAT(Shell::Get()
                  ->frame_throttling_controller()
                  ->GetCurrentThrottledFrameRate(),
              Eq(30));

  // T: .89
  AdvanceTimeAndPaint(animation, .89f - .81f);
  EXPECT_THAT(
      Shell::Get()->frame_throttling_controller()->GetFrameSinkIdsToThrottle(),
      UnorderedElementsAre(kTestFrameSinkId));
  EXPECT_THAT(Shell::Get()
                  ->frame_throttling_controller()
                  ->GetCurrentThrottledFrameRate(),
              Eq(30));

  // T: .91
  AdvanceTimeAndPaint(animation, .91f - .89f);
  EXPECT_THAT(
      Shell::Get()->frame_throttling_controller()->GetFrameSinkIdsToThrottle(),
      IsEmpty());

  // T: .1
  AdvanceTimeAndPaint(animation, 1.1f - .9f);
  EXPECT_THAT(
      Shell::Get()->frame_throttling_controller()->GetFrameSinkIdsToThrottle(),
      IsEmpty());

  // T: .21
  AdvanceTimeAndPaint(animation, .21f - .1f);
  EXPECT_THAT(
      Shell::Get()->frame_throttling_controller()->GetFrameSinkIdsToThrottle(),
      UnorderedElementsAre(kTestFrameSinkId));
  EXPECT_THAT(Shell::Get()
                  ->frame_throttling_controller()
                  ->GetCurrentThrottledFrameRate(),
              Eq(20));
}

TEST_F(AmbientAnimationFrameRateControllerTest,
       MarkersAtBordersOfAnimationCycle) {
  constexpr viz::FrameSinkId kTestFrameSinkId = {99, 99};
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  window->SetEmbedFrameSinkId(kTestFrameSinkId);

  std::vector<cc::SkottieMarker> markers = {
      {"_CrOS_Marker_Throttled_20fps", 0.f, 0.1f},
      {"_CrOS_Marker_Throttled_30fps", 0.9f, 1.f},
  };
  lottie::Animation animation(
      base::MakeRefCounted<TestSkottieWrapper>(base::Seconds(1), markers));
  AmbientAnimationFrameRateController ambient_frame_rate_controller(
      Shell::Get()->frame_throttling_controller());
  ambient_frame_rate_controller.AddWindowToThrottle(window.get(), &animation);
  animation.Start(lottie::Animation::PlaybackConfig::CreateDefault(animation));

  // T: 0
  AdvanceTimeAndPaint(animation, .0f);
  EXPECT_THAT(
      Shell::Get()->frame_throttling_controller()->GetFrameSinkIdsToThrottle(),
      UnorderedElementsAre(kTestFrameSinkId));
  EXPECT_THAT(Shell::Get()
                  ->frame_throttling_controller()
                  ->GetCurrentThrottledFrameRate(),
              Eq(20));

  // T: .91
  AdvanceTimeAndPaint(animation, .91f);
  EXPECT_THAT(
      Shell::Get()->frame_throttling_controller()->GetFrameSinkIdsToThrottle(),
      UnorderedElementsAre(kTestFrameSinkId));
  EXPECT_THAT(Shell::Get()
                  ->frame_throttling_controller()
                  ->GetCurrentThrottledFrameRate(),
              Eq(30));

  // T: .01
  AdvanceTimeAndPaint(animation, 1.01 - .91f);
  EXPECT_THAT(
      Shell::Get()->frame_throttling_controller()->GetFrameSinkIdsToThrottle(),
      UnorderedElementsAre(kTestFrameSinkId));
  EXPECT_THAT(Shell::Get()
                  ->frame_throttling_controller()
                  ->GetCurrentThrottledFrameRate(),
              Eq(20));
}

TEST_F(AmbientAnimationFrameRateControllerTest, NoMarkers) {
  constexpr viz::FrameSinkId kTestFrameSinkId = {99, 99};
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  window->SetEmbedFrameSinkId(kTestFrameSinkId);

  lottie::Animation animation(base::MakeRefCounted<TestSkottieWrapper>(
      base::Seconds(1), std::vector<cc::SkottieMarker>()));
  AmbientAnimationFrameRateController ambient_frame_rate_controller(
      Shell::Get()->frame_throttling_controller());
  ambient_frame_rate_controller.AddWindowToThrottle(window.get(), &animation);
  animation.Start(lottie::Animation::PlaybackConfig::CreateDefault(animation));

  constexpr int kNumTimeStepsToTest = 30;
  constexpr float kTimeStepSize = .1f;
  for (int i = 0; i < kNumTimeStepsToTest; ++i) {
    AdvanceTimeAndPaint(animation, kTimeStepSize);
    EXPECT_THAT(Shell::Get()
                    ->frame_throttling_controller()
                    ->GetFrameSinkIdsToThrottle(),
                IsEmpty());
  }
}

// The time steps in this test skip multiple markers at a time.
TEST_F(AmbientAnimationFrameRateControllerTest, LargeTimesteps) {
  constexpr viz::FrameSinkId kTestFrameSinkId = {99, 99};
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  window->SetEmbedFrameSinkId(kTestFrameSinkId);

  std::vector<cc::SkottieMarker> markers = {
      {"_CrOS_Marker_Throttled_10fps", 0.f, 0.1f},
      {"_CrOS_Marker_Throttled_20fps", 0.1f, 0.2f},
      {"_CrOS_Marker_Throttled_30fps", 0.2f, 0.3f},
      {"_CrOS_Marker_Throttled_40fps", 0.3f, 0.4f},
      {"_CrOS_Marker_Throttled_50fps", 0.4f, 0.5f},
  };
  lottie::Animation animation(
      base::MakeRefCounted<TestSkottieWrapper>(base::Seconds(1), markers));
  AmbientAnimationFrameRateController ambient_frame_rate_controller(
      Shell::Get()->frame_throttling_controller());
  ambient_frame_rate_controller.AddWindowToThrottle(window.get(), &animation);
  animation.Start(lottie::Animation::PlaybackConfig::CreateDefault(animation));

  // T: 0
  AdvanceTimeAndPaint(animation, .0f);
  EXPECT_THAT(
      Shell::Get()->frame_throttling_controller()->GetFrameSinkIdsToThrottle(),
      UnorderedElementsAre(kTestFrameSinkId));
  EXPECT_THAT(Shell::Get()
                  ->frame_throttling_controller()
                  ->GetCurrentThrottledFrameRate(),
              Eq(10));

  // T: .25
  AdvanceTimeAndPaint(animation, .25f);
  EXPECT_THAT(
      Shell::Get()->frame_throttling_controller()->GetFrameSinkIdsToThrottle(),
      UnorderedElementsAre(kTestFrameSinkId));
  EXPECT_THAT(Shell::Get()
                  ->frame_throttling_controller()
                  ->GetCurrentThrottledFrameRate(),
              Eq(30));

  // T: .45
  AdvanceTimeAndPaint(animation, .45f - .25f);
  EXPECT_THAT(
      Shell::Get()->frame_throttling_controller()->GetFrameSinkIdsToThrottle(),
      UnorderedElementsAre(kTestFrameSinkId));
  EXPECT_THAT(Shell::Get()
                  ->frame_throttling_controller()
                  ->GetCurrentThrottledFrameRate(),
              Eq(50));
}

TEST_F(AmbientAnimationFrameRateControllerTest, MultipleWindows) {
  constexpr viz::FrameSinkId kTestFrameSinkId1 = {99, 99};
  std::unique_ptr<aura::Window> window_1 = CreateTestWindow();
  window_1->SetEmbedFrameSinkId(kTestFrameSinkId1);

  constexpr viz::FrameSinkId kTestFrameSinkId2 = {999, 999};
  std::unique_ptr<aura::Window> window_2 = CreateTestWindow();
  window_2->SetEmbedFrameSinkId(kTestFrameSinkId2);

  std::vector<cc::SkottieMarker> markers = {
      {"_CrOS_Marker_Throttled_10fps", 0.4f, 0.6f},
  };
  lottie::Animation animation_1(
      base::MakeRefCounted<TestSkottieWrapper>(base::Seconds(1), markers));
  lottie::Animation animation_2(
      base::MakeRefCounted<TestSkottieWrapper>(base::Seconds(1), markers));
  AmbientAnimationFrameRateController ambient_frame_rate_controller(
      Shell::Get()->frame_throttling_controller());
  animation_1.Start(
      lottie::Animation::PlaybackConfig::CreateDefault(animation_1));
  animation_2.Start(
      lottie::Animation::PlaybackConfig::CreateDefault(animation_2));

  // T: 0
  AdvanceTimeAndPaint({&animation_1, &animation_2}, .0f);
  // T: .41
  AdvanceTimeAndPaint({&animation_1, &animation_2}, .41f);

  ambient_frame_rate_controller.AddWindowToThrottle(window_1.get(),
                                                    &animation_1);
  EXPECT_THAT(
      Shell::Get()->frame_throttling_controller()->GetFrameSinkIdsToThrottle(),
      UnorderedElementsAre(kTestFrameSinkId1));
  EXPECT_THAT(Shell::Get()
                  ->frame_throttling_controller()
                  ->GetCurrentThrottledFrameRate(),
              Eq(10));

  // T: .59
  AdvanceTimeAndPaint({&animation_1, &animation_2}, .59f - .41f);
  ambient_frame_rate_controller.AddWindowToThrottle(window_2.get(),
                                                    &animation_2);
  EXPECT_THAT(
      Shell::Get()->frame_throttling_controller()->GetFrameSinkIdsToThrottle(),
      UnorderedElementsAre(kTestFrameSinkId1, kTestFrameSinkId2));
  EXPECT_THAT(Shell::Get()
                  ->frame_throttling_controller()
                  ->GetCurrentThrottledFrameRate(),
              Eq(10));

  // T: .7
  AdvanceTimeAndPaint({&animation_1, &animation_2}, .7f - .59f);
  EXPECT_THAT(
      Shell::Get()->frame_throttling_controller()->GetFrameSinkIdsToThrottle(),
      IsEmpty());
}

TEST_F(AmbientAnimationFrameRateControllerTest,
       HandlesWindowAndAnimationDestroyedFirst) {
  constexpr viz::FrameSinkId kTestFrameSinkId1 = {99, 99};
  std::unique_ptr<aura::Window> window_1 = CreateTestWindow();
  window_1->SetEmbedFrameSinkId(kTestFrameSinkId1);

  constexpr viz::FrameSinkId kTestFrameSinkId2 = {999, 999};
  std::unique_ptr<aura::Window> window_2 = CreateTestWindow();
  window_2->SetEmbedFrameSinkId(kTestFrameSinkId2);

  std::vector<cc::SkottieMarker> markers = {
      {"_CrOS_Marker_Throttled_10fps", 0.f, 0.2f},
  };
  auto animation_1 = std::make_unique<lottie::Animation>(
      base::MakeRefCounted<TestSkottieWrapper>(base::Seconds(1), markers));
  auto animation_2 = std::make_unique<lottie::Animation>(
      base::MakeRefCounted<TestSkottieWrapper>(base::Seconds(1), markers));
  AmbientAnimationFrameRateController ambient_frame_rate_controller(
      Shell::Get()->frame_throttling_controller());
  animation_1->Start(
      lottie::Animation::PlaybackConfig::CreateDefault(*animation_1));
  animation_2->Start(
      lottie::Animation::PlaybackConfig::CreateDefault(*animation_2));
  ambient_frame_rate_controller.AddWindowToThrottle(window_1.get(),
                                                    animation_1.get());
  ambient_frame_rate_controller.AddWindowToThrottle(window_2.get(),
                                                    animation_2.get());

  // T: 0
  AdvanceTimeAndPaint({animation_1.get(), animation_2.get()}, .0f);
  ASSERT_THAT(
      Shell::Get()->frame_throttling_controller()->GetFrameSinkIdsToThrottle(),
      UnorderedElementsAre(kTestFrameSinkId1, kTestFrameSinkId2));

  window_1.reset();
  animation_1.reset();
  // T: 0.05f
  AdvanceTimeAndPaint({animation_2.get()}, .05f);
  EXPECT_THAT(
      Shell::Get()->frame_throttling_controller()->GetFrameSinkIdsToThrottle(),
      UnorderedElementsAre(kTestFrameSinkId2));

  // Try the reverse destruction order from before.
  animation_2.reset();
  window_2.reset();
  // The frame rate should be restored to default when the animation is
  // over.
  EXPECT_THAT(
      Shell::Get()->frame_throttling_controller()->GetFrameSinkIdsToThrottle(),
      IsEmpty());
}

}  // namespace
}  // namespace ash
