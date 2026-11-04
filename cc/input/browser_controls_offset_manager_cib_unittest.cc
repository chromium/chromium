// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/browser_controls_offset_manager_cib.h"

#include "base/time/time.h"
#include "cc/input/browser_controls_offset_manager_client.h"
#include "cc/input/browser_controls_offset_tag_modifications.h"
#include "cc/input/browser_controls_offset_tags.h"
#include "cc/metrics/begin_main_frame_metrics.h"
#include "components/viz/common/quads/offset_tag.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {
namespace {

using ::testing::ElementsAre;
using ::testing::FieldsAre;
using ::testing::IsEmpty;
using ::testing::NiceMock;

class MockClient : public BrowserControlsOffsetManagerClient {
 public:
  float TopControlsHeight() const override { return 100.0f; }
  float TopControlsMinHeight() const override { return 0.0f; }
  float BottomControlsHeight() const override { return 50.0f; }
  float BottomControlsMinHeight() const override { return 0.0f; }
  void SetCurrentBrowserControlsShownRatio(float top_ratio,
                                           float bottom_ratio) override {
    if (top_ratio == top_ratio_ && bottom_ratio == bottom_ratio_) {
      return;
    }
    top_ratio_ = top_ratio;
    bottom_ratio_ = bottom_ratio;
    DidChangeBrowserControlsPosition();
  }
  float CurrentTopControlsShownRatio() const override { return top_ratio_; }
  float CurrentBottomControlsShownRatio() const override {
    return bottom_ratio_;
  }
  gfx::PointF ViewportScrollOffset() const override { return gfx::PointF(); }
  float MaxViewportScrollOffsetY() const override { return 1000.0f; }
  MOCK_METHOD(void, DidChangeBrowserControlsPosition, (), (override));
  bool OnlyExpandTopControlsAtPageTop() const override { return false; }
  bool HaveRootScrollNode() const override { return true; }
  MOCK_METHOD(void, SetNeedsCommit, (BeginMainFrameReason), (override));
  float DeviceScaleFactor() const override { return 1.0f; }

 private:
  float top_ratio_ = 1.0f;
  float bottom_ratio_ = 1.0f;
};

class BrowserControlsOffsetManagerCibTest : public ::testing::Test {
 protected:
  NiceMock<MockClient> client_;
  BrowserControlsOffsetManagerCib manager_{&client_};
};

TEST_F(BrowserControlsOffsetManagerCibTest,
       GetOffsetTagValuesReturnsOnlyContentOffsetTag) {
  EXPECT_THAT(manager_.GetOffsetTagValues(), IsEmpty());

  const viz::OffsetTag content_tag = viz::OffsetTag::CreateRandom();
  BrowserControlsOffsetTagModifications modifications;
  modifications.tags.top_controls_offset_tag = viz::OffsetTag::CreateRandom();
  modifications.tags.content_offset_tag = content_tag;
  modifications.tags.bottom_controls_offset_tag =
      viz::OffsetTag::CreateRandom();
  modifications.top_controls_additional_height = 5;
  modifications.bottom_controls_additional_height = 10;
  manager_.SetOffsetTagModifications(modifications);

  EXPECT_THAT(manager_.GetOffsetTagValues(),
              ElementsAre(FieldsAre(content_tag, gfx::Vector2dF(0.0f, 0.0f))));

  client_.SetCurrentBrowserControlsShownRatio(0.256f, 0.5f);
  EXPECT_THAT(
      manager_.GetOffsetTagValues(),
      ElementsAre(FieldsAre(content_tag, gfx::Vector2dF(0.0f, -74.0f))));

  client_.SetCurrentBrowserControlsShownRatio(0.0f, 0.0f);
  EXPECT_THAT(
      manager_.GetOffsetTagValues(),
      ElementsAre(FieldsAre(content_tag, gfx::Vector2dF(0.0f, -100.0f))));
}

TEST_F(BrowserControlsOffsetManagerCibTest, ScrollIsNotConsumed) {
  EXPECT_CALL(client_, DidChangeBrowserControlsPosition()).Times(0);
  constexpr gfx::Vector2dF kScrollDelta(0.0f, 30.0f);
  manager_.ScrollBegin();
  EXPECT_EQ(kScrollDelta,
            manager_.ScrollBy(kScrollDelta, /*is_inertial=*/false));
  EXPECT_EQ(kScrollDelta,
            manager_.ScrollBy(kScrollDelta, /*is_inertial=*/true));
  manager_.ScrollEnd(gfx::Vector2dF());
  EXPECT_FLOAT_EQ(1.0f, manager_.TopControlsShownRatio());
  EXPECT_FLOAT_EQ(1.0f, manager_.BottomControlsShownRatio());
  EXPECT_FALSE(manager_.HasAnimation());
  EXPECT_EQ(gfx::Vector2dF(), manager_.Animate(base::TimeTicks()));
}

}  // namespace
}  // namespace cc
