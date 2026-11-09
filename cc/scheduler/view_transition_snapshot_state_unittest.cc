// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/view_transition_snapshot_state.h"

#include <memory>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "cc/base/features.h"
#include "cc/view_transition/view_transition_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace cc {
namespace {

TEST(ViewTransitionSnapshotStateTest,
     StaticPolicyChecksOnlyApplyToSnapshotSaveDirectives) {
  base::test::ScopedFeatureList enabled_feature_list;
  enabled_feature_list.InitAndEnableFeature(
      features::kViewTransitionEarlySnapshotRelease);

  std::vector<std::unique_ptr<ViewTransitionRequest>> same_doc_requests;
  same_doc_requests.push_back(ViewTransitionRequest::CreateCapture(
      blink::ViewTransitionToken(), /*maybe_cross_frame_sink=*/false, {},
      base::DoNothing(), /*delay_layer_tree_view_deletion=*/true));
  EXPECT_FALSE(same_doc_requests[0]->IsSaveDirectiveForSnapshot());
  EXPECT_FALSE(ViewTransitionSnapshotState::HasSaveDirectiveForSnapshot(
      same_doc_requests));
  EXPECT_TRUE(ViewTransitionSnapshotState::GetDirectivesToNotifyInPostCommit(
                  same_doc_requests)
                  .empty());
  EXPECT_FALSE(
      ViewTransitionSnapshotState::ShouldResetBrowserControlsAnimations(
          /*is_active_tree=*/false, *same_doc_requests[0]));
  EXPECT_TRUE(ViewTransitionSnapshotState::ShouldResetBrowserControlsAnimations(
      /*is_active_tree=*/true, *same_doc_requests[0]));

  std::vector<std::unique_ptr<ViewTransitionRequest>> cross_doc_save_requests;
  cross_doc_save_requests.push_back(ViewTransitionRequest::CreateCapture(
      blink::ViewTransitionToken(), /*maybe_cross_frame_sink=*/true, {},
      base::DoNothing(), /*delay_layer_tree_view_deletion=*/true));
  cross_doc_save_requests.push_back(ViewTransitionRequest::CreateCapture(
      blink::ViewTransitionToken(), /*maybe_cross_frame_sink=*/false, {},
      base::DoNothing(), /*delay_layer_tree_view_deletion=*/true));
  cross_doc_save_requests.push_back(ViewTransitionRequest::CreateCapture(
      blink::ViewTransitionToken(), /*maybe_cross_frame_sink=*/true, {},
      base::DoNothing(), /*delay_layer_tree_view_deletion=*/true));
  std::vector<uint32_t> expected_sequence_ids = {
      cross_doc_save_requests[0]->sequence_id(),
      cross_doc_save_requests[2]->sequence_id()};
  EXPECT_TRUE(cross_doc_save_requests[0]->IsSaveDirectiveForSnapshot());
  EXPECT_FALSE(cross_doc_save_requests[1]->IsSaveDirectiveForSnapshot());
  EXPECT_TRUE(cross_doc_save_requests[2]->IsSaveDirectiveForSnapshot());
  EXPECT_TRUE(ViewTransitionSnapshotState::HasSaveDirectiveForSnapshot(
      cross_doc_save_requests));
  EXPECT_EQ(ViewTransitionSnapshotState::GetDirectivesToNotifyInPostCommit(
                cross_doc_save_requests),
            expected_sequence_ids);
  EXPECT_TRUE(ViewTransitionSnapshotState::ShouldResetBrowserControlsAnimations(
      /*is_active_tree=*/false, *cross_doc_save_requests[0]));
  EXPECT_TRUE(ViewTransitionSnapshotState::ShouldResetBrowserControlsAnimations(
      /*is_active_tree=*/true, *cross_doc_save_requests[0]));

  std::vector<std::unique_ptr<ViewTransitionRequest>>
      cross_doc_animate_requests;
  cross_doc_animate_requests.push_back(
      ViewTransitionRequest::CreateAnimateRenderer(
          blink::ViewTransitionToken(), /*maybe_cross_frame_sink=*/true,
          /*delay_layer_tree_view_deletion=*/true));
  EXPECT_FALSE(cross_doc_animate_requests[0]->IsSaveDirectiveForSnapshot());
  EXPECT_FALSE(ViewTransitionSnapshotState::HasSaveDirectiveForSnapshot(
      cross_doc_animate_requests));
  EXPECT_TRUE(ViewTransitionSnapshotState::GetDirectivesToNotifyInPostCommit(
                  cross_doc_animate_requests)
                  .empty());
  EXPECT_FALSE(
      ViewTransitionSnapshotState::ShouldResetBrowserControlsAnimations(
          /*is_active_tree=*/true, *cross_doc_animate_requests[0]));

  // Disabling the feature flag disables snapshot early release behavior.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kViewTransitionEarlySnapshotRelease);
  EXPECT_FALSE(cross_doc_save_requests[0]->IsSaveDirectiveForSnapshot());
  EXPECT_FALSE(ViewTransitionSnapshotState::HasSaveDirectiveForSnapshot(
      cross_doc_save_requests));
  EXPECT_TRUE(ViewTransitionSnapshotState::GetDirectivesToNotifyInPostCommit(
                  cross_doc_save_requests)
                  .empty());
  EXPECT_FALSE(
      ViewTransitionSnapshotState::ShouldResetBrowserControlsAnimations(
          /*is_active_tree=*/false, *cross_doc_save_requests[0]));
}

TEST(ViewTransitionSnapshotStateTest,
     DefersHideAndUnpauseWhileCapturingAndReplaysWhenIdle) {
  ViewTransitionSnapshotState state;
  EXPECT_FALSE(state.IsCapturing());
  EXPECT_FALSE(state.ShouldDeferSetVisible(false));
  EXPECT_FALSE(state.ShouldDeferSetPauseRendering(
      /*pause_rendering=*/false, /*delay_until_visibility_change=*/true));
  EXPECT_FALSE(state.HasReadyDeferredActions());

  // 1. Enter capturing state when a snapshot `kSave` directive is present.
  state.SetCapturing(true);
  EXPECT_TRUE(state.IsCapturing());

  // 2. While capturing, `SetVisible(false)` and `SetPauseRendering(false, ...)`
  // are deferred.
  EXPECT_TRUE(state.ShouldDeferSetVisible(false));
  EXPECT_TRUE(state.ShouldDeferSetPauseRendering(
      /*pause_rendering=*/false, /*delay_until_visibility_change=*/true));
  // Deferred actions are not ready while `kSave` is still in flight.
  EXPECT_FALSE(state.HasReadyDeferredActions());

  // 3. Once the `kSave` frame is drawn (or dropped), `IsCapturing()` returns
  // false and the deferred hide and unpause become ready to replay.
  state.SetCapturing(false);
  EXPECT_FALSE(state.IsCapturing());
  EXPECT_TRUE(state.HasReadyDeferredActions());

  EXPECT_TRUE(state.TakeDeferredHide());
  EXPECT_FALSE(state.TakeDeferredHide());

  bool delay_until_visibility_change = false;
  EXPECT_TRUE(state.TakeDeferredUnpause(delay_until_visibility_change));
  EXPECT_TRUE(delay_until_visibility_change);
  EXPECT_FALSE(state.TakeDeferredUnpause(delay_until_visibility_change));
  EXPECT_FALSE(state.HasReadyDeferredActions());
}

TEST(ViewTransitionSnapshotStateTest,
     DeferredHideAndUnpauseCancelledBySubsequentVisibleOrPause) {
  ViewTransitionSnapshotState state;
  state.SetCapturing(true);

  EXPECT_TRUE(state.ShouldDeferSetVisible(false));
  EXPECT_TRUE(state.ShouldDeferSetPauseRendering(
      /*pause_rendering=*/false, /*delay_until_visibility_change=*/true));

  // Becoming visible or pausing rendering again cancels the deferred actions
  // and is applied immediately (`ShouldDefer...` returns false).
  EXPECT_FALSE(state.ShouldDeferSetVisible(true));
  EXPECT_FALSE(state.ShouldDeferSetPauseRendering(
      /*pause_rendering=*/true, /*delay_until_visibility_change=*/false));

  state.SetCapturing(false);
  EXPECT_FALSE(state.HasReadyDeferredActions());
}

}  // namespace
}  // namespace cc
