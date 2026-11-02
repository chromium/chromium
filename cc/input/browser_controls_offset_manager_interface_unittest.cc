// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/browser_controls_offset_manager_interface.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "cc/input/browser_controls_offset_manager_client.h"
#include "cc/input/browser_controls_offset_tag_modifications.h"
#include "cc/input/browser_controls_offset_tags.h"
#include "cc/input/browser_controls_state.h"
#include "cc/metrics/begin_main_frame_metrics.h"
#include "components/viz/common/quads/offset_tag.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {
namespace {

using ::testing::FieldsAre;
using ::testing::NiceMock;

class MockBrowserControlsOffsetManagerClient
    : public BrowserControlsOffsetManagerClient {
 public:
  float TopControlsHeight() const override { return 0.0f; }
  float TopControlsMinHeight() const override { return 0.0f; }
  float BottomControlsHeight() const override { return 0.0f; }
  float BottomControlsMinHeight() const override { return 0.0f; }
  void SetCurrentBrowserControlsShownRatio(float top_ratio,
                                           float bottom_ratio) override {}
  float CurrentTopControlsShownRatio() const override { return 1.0f; }
  float CurrentBottomControlsShownRatio() const override { return 1.0f; }
  gfx::PointF ViewportScrollOffset() const override { return gfx::PointF(); }
  float MaxViewportScrollOffsetY() const override { return 0.0f; }
  void DidChangeBrowserControlsPosition() override {}
  bool OnlyExpandTopControlsAtPageTop() const override { return false; }
  bool HaveRootScrollNode() const override { return true; }
  MOCK_METHOD(void, SetNeedsCommit, (BeginMainFrameReason), (override));
  float DeviceScaleFactor() const override { return 1.0f; }
};

class TestBrowserControlsOffsetManager
    : public BrowserControlsOffsetManagerInterface {
 public:
  explicit TestBrowserControlsOffsetManager(
      BrowserControlsOffsetManagerClient* client)
      : BrowserControlsOffsetManagerInterface(client) {}

  // BrowserControlsOffsetManagerInterface:
  std::vector<viz::OffsetTagValue> GetOffsetTagValues() const override {
    return {};
  }
  std::pair<float, float> TopControlsShownRatioRange() override {
    return std::make_pair(0.0f, 1.0f);
  }
  std::pair<float, float> BottomControlsShownRatioRange() override {
    return std::make_pair(0.0f, 1.0f);
  }
  bool HasAnimation() override { return false; }
  void OnBrowserControlsParamsChanged(bool animate_changes) override {}
  void ScrollBegin() override {}
  gfx::Vector2dF ScrollBy(const gfx::Vector2dF& pending_delta,
                          bool is_inertial) override {
    return pending_delta;
  }
  void ScrollEnd(const gfx::Vector2dF& compensated_scroll_delta) override {}
  void PinchBegin() override {}
  void PinchEnd() override {}
  gfx::Vector2dF Animate(base::TimeTicks monotonic_time) override {
    return gfx::Vector2dF();
  }
  void ResetAnimations() override {}

 protected:
  void OnBrowserControlsStateUpdated(BrowserControlsState constraints,
                                     BrowserControlsState current,
                                     bool animate) override {}
};

class BrowserControlsOffsetManagerInterfaceTest : public testing::Test {
 protected:
  BrowserControlsState PullConstraint(bool* changed) {
    return manager_.PullConstraintForMainThread(changed);
  }

  NiceMock<MockBrowserControlsOffsetManagerClient> client_;
  TestBrowserControlsOffsetManager manager_{&client_};
};

TEST_F(BrowserControlsOffsetManagerInterfaceTest,
       ConstraintChangeIsReportedUntilSyncedToMainThread) {
  manager_.UpdateBrowserControlsState(BrowserControlsState::kShown,
                                      BrowserControlsState::kShown,
                                      /*animate=*/true, std::nullopt);

  bool changed = false;
  EXPECT_EQ(BrowserControlsState::kShown, PullConstraint(&changed));
  EXPECT_TRUE(changed);

  changed = false;
  EXPECT_EQ(BrowserControlsState::kShown, PullConstraint(&changed));
  EXPECT_TRUE(changed);

  manager_.NotifyConstraintSyncedToMainThread();
  EXPECT_EQ(BrowserControlsState::kShown, PullConstraint(&changed));
  EXPECT_FALSE(changed);
}

TEST_F(BrowserControlsOffsetManagerInterfaceTest,
       CommitIsRequestedOnlyWhenTheConstraintChanges) {
  // kBoth is the initial constraint, so the main thread has nothing to
  // relayout.
  EXPECT_CALL(client_, SetNeedsCommit).Times(0);
  manager_.UpdateBrowserControlsState(BrowserControlsState::kBoth,
                                      BrowserControlsState::kShown,
                                      /*animate=*/true, std::nullopt);
  bool changed = true;
  EXPECT_EQ(BrowserControlsState::kBoth, PullConstraint(&changed));
  EXPECT_FALSE(changed);

  EXPECT_CALL(client_, SetNeedsCommit);
  manager_.UpdateBrowserControlsState(BrowserControlsState::kHidden,
                                      BrowserControlsState::kHidden,
                                      /*animate=*/false, std::nullopt);
}

TEST_F(BrowserControlsOffsetManagerInterfaceTest,
       OffsetTagModificationsAreKeptWhenNotProvided) {
  const auto top_tag = viz::OffsetTag::CreateRandom();
  const auto content_tag = viz::OffsetTag::CreateRandom();
  const auto bottom_tag = viz::OffsetTag::CreateRandom();
  BrowserControlsOffsetTagModifications modifications;
  modifications.tags.top_controls_offset_tag = top_tag;
  modifications.tags.content_offset_tag = content_tag;
  modifications.tags.bottom_controls_offset_tag = bottom_tag;
  modifications.top_controls_additional_height = 10;
  modifications.bottom_controls_additional_height = 5;

  manager_.UpdateBrowserControlsState(BrowserControlsState::kBoth,
                                      BrowserControlsState::kBoth,
                                      /*animate=*/false, modifications);
  EXPECT_THAT(manager_.GetOffsetTagModifications(),
              FieldsAre(FieldsAre(top_tag, content_tag, bottom_tag), 10, 5));

  manager_.UpdateBrowserControlsState(BrowserControlsState::kBoth,
                                      BrowserControlsState::kBoth,
                                      /*animate=*/false, std::nullopt);
  EXPECT_THAT(manager_.GetOffsetTagModifications(),
              FieldsAre(FieldsAre(top_tag, content_tag, bottom_tag), 10, 5));
}

}  // namespace
}  // namespace cc
