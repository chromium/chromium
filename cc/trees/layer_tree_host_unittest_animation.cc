// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include <stdint.h>
#include <climits>

#include "base/functional/bind.h"
#include "base/metrics/statistics_recorder.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/element_animations.h"
#include "cc/animation/keyframe_effect.h"
#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/animation/scroll_offset_animation_curve_factory.h"
#include "cc/animation/scroll_offset_animations.h"
#include "cc/base/completion_event.h"
#include "cc/base/features.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/target_property.h"
#include "cc/trees/transform_node.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"
#include "ui/gfx/animation/keyframe/timing_function.h"
#include "ui/gfx/geometry/test/geometry_util.h"
#include "ui/gfx/geometry/transform_operations.h"

namespace cc {
namespace {

#define SAMPLE(curve, time)                                                  \
  curve->GetTransformedValue(time,                                           \
                             time < base::TimeDelta()                        \
                                 ? gfx::TimingFunction::LimitDirection::LEFT \
                                 : gfx::TimingFunction::LimitDirection::RIGHT)

class LayerTreeHostAnimationTest : public LayerTreeTest {
 public:
  LayerTreeHostAnimationTest()
      : timeline_id_(AnimationIdProvider::NextTimelineId()),
        animation_id_(AnimationIdProvider::NextAnimationId()),
        animation_child_id_(AnimationIdProvider::NextAnimationId()) {
    timeline_ = AnimationTimeline::Create(timeline_id_);
    animation_ = Animation::Create(animation_id_);
    animation_child_ = Animation::Create(animation_child_id_);

    animation_->set_animation_delegate(this);
  }

  void AttachAnimationsToTimeline() {
    animation_host()->AddAnimationTimeline(timeline_.get());
    layer_tree_host()->SetElementIdsForTesting();
    timeline_->AttachAnimation(animation_.get());
    timeline_->AttachAnimation(animation_child_.get());
  }

  void DetachAnimationsFromTimeline() {
    if (animation_)
      timeline_->DetachAnimation(animation_.get());
    if (animation_child_)
      timeline_->DetachAnimation(animation_child_.get());
    animation_host()->RemoveAnimationTimeline(timeline_.get());
  }

  void GetImplTimelineAndAnimationByID(const LayerTreeHostImpl& host_impl) {
    AnimationHost* animation_host_impl = GetImplAnimationHost(&host_impl);
    timeline_impl_ = animation_host_impl->GetTimelineById(timeline_id_);
    EXPECT_TRUE(timeline_impl_);
    animation_impl_ = timeline_impl_->GetAnimationById(animation_id_);
    EXPECT_TRUE(animation_impl_);
    animation_child_impl_ =
        timeline_impl_->GetAnimationById(animation_child_id_);
    EXPECT_TRUE(animation_child_impl_);
  }

  void CleanupBeforeDestroy() override {
    // This needs to happen on the main thread (so can't happen in
    // EndTest()), and needs to happen before DestroyLayerTreeHost()
    // (which will trigger assertions if we don't do this), so it can't
    // happen in AfterTest().
    DetachAnimationsFromTimeline();
  }

  AnimationHost* GetImplAnimationHost(
      const LayerTreeHostImpl* host_impl) const {
    return static_cast<AnimationHost*>(host_impl->mutator_host());
  }

 protected:
  scoped_refptr<AnimationTimeline> timeline_;
  scoped_refptr<Animation> animation_;
  scoped_refptr<Animation> animation_child_;

  scoped_refptr<AnimationTimeline> timeline_impl_;
  scoped_refptr<Animation> animation_impl_;
  scoped_refptr<Animation> animation_child_impl_;

  const int timeline_id_;
  const int animation_id_;
  const int animation_child_id_;
};

// Makes sure that SetNeedsAnimate does not cause the CommitRequested() state to
// be set.
class LayerTreeHostAnimationTestSetNeedsAnimateShouldNotSetCommitRequested
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestSetNeedsAnimateShouldNotSetCommitRequested()
      : num_commits_(0) {}

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void BeginMainFrame(const viz::BeginFrameArgs& args) override {
    // We skip the first commit because its the commit that populates the
    // impl thread with a tree. After the second commit, the test is done.
    if (num_commits_ != 1)
      return;

    layer_tree_host()->SetNeedsAnimate();
    // Right now, CommitRequested is going to be true, because during
    // BeginFrame, we force CommitRequested to true to prevent requests from
    // hitting the impl thread. But, when the next DidCommit happens, we should
    // verify that CommitRequested has gone back to false.
  }

  void DidCommit() override {
    if (!num_commits_) {
      EXPECT_FALSE(layer_tree_host()->CommitRequested());
      layer_tree_host()->SetNeedsAnimate();
      EXPECT_FALSE(layer_tree_host()->CommitRequested());
    }

    // Verifies that the SetNeedsAnimate we made in ::Animate did not
    // trigger CommitRequested.
    EXPECT_FALSE(layer_tree_host()->CommitRequested());
    EndTest();
    num_commits_++;
  }

 private:
  int num_commits_;
};

MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestSetNeedsAnimateShouldNotSetCommitRequested);

// Trigger a frame with SetNeedsCommit. Then, inside the resulting animate
// callback, request another frame using SetNeedsAnimate. End the test when
// animate gets called yet-again, indicating that the proxy is correctly
// handling the case where SetNeedsAnimate() is called inside the BeginFrame
// flow.
class LayerTreeHostAnimationTestSetNeedsAnimateInsideAnimationCallback
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestSetNeedsAnimateInsideAnimationCallback()
      : num_begin_frames_(0) {}

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void BeginMainFrame(const viz::BeginFrameArgs& args) override {
    if (!num_begin_frames_) {
      layer_tree_host()->SetNeedsAnimate();
      num_begin_frames_++;
      return;
    }
    EndTest();
  }

 private:
  int num_begin_frames_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestSetNeedsAnimateInsideAnimationCallback);

// Add a layer animation and confirm that
// LayerTreeHostImpl::UpdateAnimationState does get called.
class LayerTreeHostAnimationTestAddKeyframeModel
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestAddKeyframeModel()
      : update_animation_state_was_called_(false) {}

  void BeginTest() override {
    AttachAnimationsToTimeline();
    animation_->AttachElement(layer_tree_host()->root_layer()->element_id());
    PostAddOpacityAnimationToMainThreadInstantly(animation_.get());
  }

  void UpdateAnimationState(LayerTreeHostImpl* host_impl,
                            bool has_unfinished_animation) override {
    EXPECT_FALSE(has_unfinished_animation);
    update_animation_state_was_called_ = true;
  }

  void NotifyAnimationStarted(base::TimeTicks monotonic_time,
                              int target_property,
                              int group) override {
    EXPECT_LT(base::TimeTicks(), monotonic_time);

    gfx::KeyframeModel* keyframe_model =
        animation_->GetKeyframeModel(TargetProperty::OPACITY);
    if (keyframe_model)
      animation_->RemoveKeyframeModel(keyframe_model->id());

    EndTest();
  }

  void AfterTest() override { EXPECT_TRUE(update_animation_state_was_called_); }

 private:
  bool update_animation_state_was_called_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestAddKeyframeModel);

// Add an animation that does not cause damage, followed by an animation that
// does. Confirm that the first animation finishes even though
// enable_early_damage_check should abort draws.
class LayerTreeHostAnimationTestNoDamageAnimation
    : public LayerTreeHostAnimationTest {
  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->using_synchronous_renderer_compositor = true;
    settings->enable_early_damage_check = true;
  }

 public:
  LayerTreeHostAnimationTestNoDamageAnimation()
      : started_animating_(false), finished_animating_(false) {}

  void BeginTest() override {
    AttachAnimationsToTimeline();
    animation_->AttachElement(layer_tree_host()->root_layer()->element_id());
    PostAddNoDamageAnimationToMainThread(animation_.get());
    PostAddOpacityAnimationToMainThread(animation_.get());
  }

  void AnimateLayers(LayerTreeHostImpl* host_impl,
                     base::TimeTicks monotonic_time) override {
    base::AutoLock auto_lock(lock_);
    started_animating_ = true;
  }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    base::AutoLock auto_lock(lock_);
    if (started_animating_ && finished_animating_)
      EndTest();
  }

  void NotifyAnimationFinished(base::TimeTicks monotonic_time,
                               int target_property,
                               int group) override {
    base::AutoLock auto_lock(lock_);
    finished_animating_ = true;
  }

 private:
  bool started_animating_;
  bool finished_animating_;
  base::Lock lock_;
};

// This behavior is specific to Android WebView, which only uses
// multi-threaded compositor.
MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestNoDamageAnimation);

// Add a layer animation to a layer, but continually fail to draw. Confirm that
// after a while, we do eventually force a draw.
class LayerTreeHostAnimationTestCheckerboardDoesNotStarveDraws
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestCheckerboardDoesNotStarveDraws()
      : started_animating_(false) {}

  void BeginTest() override {
    AttachAnimationsToTimeline();
    animation_->AttachElement(layer_tree_host()->root_layer()->element_id());
    PostAddOpacityAnimationToMainThread(animation_.get());
  }

  void AnimateLayers(LayerTreeHostImpl* host_impl,
                     base::TimeTicks monotonic_time) override {
    started_animating_ = true;
  }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    if (started_animating_)
      EndTest();
  }

  DrawResult PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                   LayerTreeHostImpl::FrameData* frame,
                                   DrawResult draw_result) override {
    return DrawResult::kAbortedCheckerboardAnimations;
  }

 private:
  bool started_animating_;
};

// Starvation can only be an issue with the MT compositor.
MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestCheckerboardDoesNotStarveDraws);

// Ensures that animations eventually get deleted.
class LayerTreeHostAnimationTestAnimationsGetDeleted
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestAnimationsGetDeleted()
      : started_animating_(false) {}

  void BeginTest() override {
    AttachAnimationsToTimeline();
    animation_->AttachElement(layer_tree_host()->root_layer()->element_id());
    PostAddOpacityAnimationToMainThread(animation_.get());
  }

  void AnimateLayers(LayerTreeHostImpl* host_impl,
                     base::TimeTicks monotonic_time) override {
    bool have_animations = !GetImplAnimationHost(host_impl)
                                ->ticking_animations_for_testing()
                                .empty();
    if (!started_animating_ && have_animations) {
      started_animating_ = true;
      return;
    }

    if (started_animating_ && !have_animations)
      EndTest();
  }

  void NotifyAnimationFinished(base::TimeTicks monotonic_time,
                               int target_property,
                               int group) override {
    // Animations on the impl-side ElementAnimations only get deleted during
    // a commit, so we need to schedule a commit.
    layer_tree_host()->SetNeedsCommit();
  }

 private:
  bool started_animating_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestAnimationsGetDeleted);

// Ensure that an animation's timing function is respected.
class LayerTreeHostAnimationTestAddKeyframeModelWithTimingFunction
    : public LayerTreeHostAnimationTest {
 public:
  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    picture_ = FakePictureLayer::Create(&client_);
    picture_->SetBounds(gfx::Size(4, 4));
    client_.set_bounds(picture_->bounds());
    layer_tree_host()->root_layer()->AddChild(picture_);

    AttachAnimationsToTimeline();
    animation_child_->AttachElement(picture_->element_id());
  }

  void BeginTest() override {
    PostAddOpacityAnimationToMainThread(animation_child_.get());
  }

  void AnimateLayers(LayerTreeHostImpl* host_impl,
                     base::TimeTicks monotonic_time) override {
    // TODO(ajuma): This test only checks the active tree. Add checks for
    // pending tree too.
    if (!host_impl->active_tree()->root_layer())
      return;

    // Wait for the commit with the animation to happen.
    if (host_impl->active_tree()->source_frame_number() != 0)
      return;

    // The impl thread may start a second frame before the test finishes. This
    // can lead to a race as if the main thread has committed a new sync tree
    // the impl thread can now delete the KeyframeModel as the animation only
    // lasts a single frame and no longer affects either tree. Only test the
    // first frame the animation shows up for.
    if (!first_animation_frame_)
      return;
    first_animation_frame_ = false;

    scoped_refptr<AnimationTimeline> timeline_impl =
        GetImplAnimationHost(host_impl)->GetTimelineById(timeline_id_);
    scoped_refptr<Animation> animation_child_impl =
        timeline_impl->GetAnimationById(animation_child_id_);

    KeyframeModel* keyframe_model =
        animation_child_impl->GetKeyframeModel(TargetProperty::OPACITY);

    const gfx::FloatAnimationCurve* curve =
        gfx::FloatAnimationCurve::ToFloatAnimationCurve(
            keyframe_model->curve());
    float start_opacity = SAMPLE(curve, base::TimeDelta());
    float end_opacity = SAMPLE(curve, curve->Duration());
    float linearly_interpolated_opacity =
        0.25f * end_opacity + 0.75f * start_opacity;
    base::TimeDelta time = curve->Duration() * 0.25f;
    // If the linear timing function associated with this animation was not
    // picked up, then the linearly interpolated opacity would be different
    // because of the default ease timing function.
    EXPECT_FLOAT_EQ(linearly_interpolated_opacity, SAMPLE(curve, time));

    EndTest();
  }

  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> picture_;
  bool first_animation_frame_ = true;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestAddKeyframeModelWithTimingFunction);

// Ensures that main thread animations have their start times synchronized with
// impl thread animations.
class LayerTreeHostAnimationTestSynchronizeAnimationStartTimes
    : public LayerTreeHostAnimationTest {
 public:
  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    picture_ = FakePictureLayer::Create(&client_);
    picture_->SetBounds(gfx::Size(4, 4));
    client_.set_bounds(picture_->bounds());

    layer_tree_host()->root_layer()->AddChild(picture_);

    AttachAnimationsToTimeline();
    animation_child_->set_animation_delegate(this);
    animation_child_->AttachElement(picture_->element_id());
  }

  void BeginTest() override {
    PostAddOpacityAnimationToMainThread(animation_child_.get());
  }

  void NotifyAnimationStarted(base::TimeTicks monotonic_time,
                              int target_property,
                              int group) override {
    KeyframeModel* keyframe_model =
        animation_child_->GetKeyframeModel(TargetProperty::OPACITY);
    main_start_time_ = keyframe_model->start_time();
    animation_child_->RemoveKeyframeModel(keyframe_model->id());
    EndTest();
  }

  void UpdateAnimationState(LayerTreeHostImpl* impl_host,
                            bool has_unfinished_animation) override {
    scoped_refptr<const AnimationTimeline> timeline_impl =
        GetImplAnimationHost(impl_host)->GetTimelineById(timeline_id_);
    scoped_refptr<Animation> animation_child_impl =
        timeline_impl->GetAnimationById(animation_child_id_);

    KeyframeModel* keyframe_model =
        animation_child_impl->GetKeyframeModel(TargetProperty::OPACITY);
    if (!keyframe_model)
      return;

    impl_start_time_ = keyframe_model->start_time();
  }

  void AfterTest() override {
    EXPECT_EQ(impl_start_time_, main_start_time_);
    EXPECT_LT(base::TimeTicks(), impl_start_time_);
  }

 private:
  base::TimeTicks main_start_time_;
  base::TimeTicks impl_start_time_;
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> picture_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestSynchronizeAnimationStartTimes);

// Ensures that notify animation finished is called.
class LayerTreeHostAnimationTestAnimationFinishedEvents
    : public LayerTreeHostAnimationTest {
 public:
  void BeginTest() override {
    AttachAnimationsToTimeline();
    animation_->AttachElement(layer_tree_host()->root_layer()->element_id());
    PostAddOpacityAnimationToMainThreadInstantly(animation_.get());
  }

  void NotifyAnimationFinished(base::TimeTicks monotonic_time,
                               int target_property,
                               int group) override {
    KeyframeModel* keyframe_model =
        animation_->GetKeyframeModel(TargetProperty::OPACITY);
    if (keyframe_model)
      animation_->RemoveKeyframeModel(keyframe_model->id());
    EndTest();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestAnimationFinishedEvents);

// Ensures that when opacity is being animated, this value does not cause the
// subtree to be skipped.
class LayerTreeHostAnimationTestDoNotSkipLayersWithAnimatedOpacity
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestDoNotSkipLayersWithAnimatedOpacity()
      : update_check_layer_() {}

  void SetupTree() override {
    update_check_layer_ = FakePictureLayer::Create(&client_);
    update_check_layer_->SetOpacity(0.f);
    layer_tree_host()->SetRootLayer(update_check_layer_);
    client_.set_bounds(update_check_layer_->bounds());
    LayerTreeHostAnimationTest::SetupTree();

    AttachAnimationsToTimeline();
    animation_->AttachElement(update_check_layer_->element_id());
  }

  void BeginTest() override {
    PostAddOpacityAnimationToMainThread(animation_.get());
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) override {
    scoped_refptr<const AnimationTimeline> timeline_impl =
        GetImplAnimationHost(host_impl)->GetTimelineById(timeline_id_);
    scoped_refptr<Animation> animation_impl =
        timeline_impl->GetAnimationById(animation_id_);

    KeyframeModel* keyframe_model_impl =
        animation_impl->GetKeyframeModel(TargetProperty::OPACITY);
    animation_impl->RemoveKeyframeModel(keyframe_model_impl->id());
    EndTest();
  }

  void AfterTest() override {
    // Update() should have been called once, proving that the layer was not
    // skipped.
    EXPECT_EQ(1, update_check_layer_->update_count());

    // clear update_check_layer_ so LayerTreeHost dies.
    update_check_layer_ = nullptr;
  }

 private:
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> update_check_layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestDoNotSkipLayersWithAnimatedOpacity);

// Layers added to tree with existing active animations should have the
// animation correctly recognized.
class LayerTreeHostAnimationTestLayerAddedWithAnimation
    : public LayerTreeHostAnimationTest {
 public:
  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override {
    if (layer_tree_host()->SourceFrameNumber() == 1) {
      AttachAnimationsToTimeline();

      scoped_refptr<Layer> layer = Layer::Create();
      layer->SetElementId(ElementId(42));
      animation_->AttachElement(layer->element_id());
      animation_->set_animation_delegate(this);

      // Any valid AnimationCurve will do here.
      std::unique_ptr<gfx::AnimationCurve> curve(new FakeFloatAnimationCurve());
      std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
          std::move(curve), 1, 1,
          KeyframeModel::TargetPropertyId(TargetProperty::OPACITY)));
      animation_->AddKeyframeModel(std::move(keyframe_model));

      // We add the animation *before* attaching the layer to the tree.
      layer_tree_host()->root_layer()->AddChild(layer);
    }
  }

  void AnimateLayers(LayerTreeHostImpl* impl_host,
                     base::TimeTicks monotonic_time) override {
    EndTest();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestLayerAddedWithAnimation);

class LayerTreeHostAnimationTestCancelAnimateCommit
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestCancelAnimateCommit()
      : num_begin_frames_(0), num_commit_calls_(0), num_draw_calls_(0) {}

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void BeginMainFrame(const viz::BeginFrameArgs& args) override {
    num_begin_frames_++;
    // No-op animate will cancel the commit.
    if (layer_tree_host()->SourceFrameNumber() == 1) {
      EndTest();
      return;
    }
    layer_tree_host()->SetNeedsAnimate();
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* impl) override {
    num_commit_calls_++;
    if (impl->active_tree()->source_frame_number() > 1)
      FAIL() << "Commit should have been canceled.";
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    num_draw_calls_++;
    if (impl->active_tree()->source_frame_number() > 1)
      FAIL() << "Draw should have been canceled.";
  }

  void AfterTest() override {
    EXPECT_EQ(2, num_begin_frames_);
    EXPECT_EQ(1, num_commit_calls_);
    EXPECT_EQ(1, num_draw_calls_);
  }

 private:
  int num_begin_frames_;
  int num_commit_calls_;
  int num_draw_calls_;
};

MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestCancelAnimateCommit);

class LayerTreeHostAnimationTestForceRedraw
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestForceRedraw()
      : num_animate_(0), num_draw_layers_(0) {}

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void BeginMainFrame(const viz::BeginFrameArgs& args) override {
    if (++num_animate_ < 2)
      layer_tree_host()->SetNeedsAnimate();
  }

  void UpdateLayerTreeHost() override {
    layer_tree_host()->SetNeedsCommitWithForcedRedraw();
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    if (++num_draw_layers_ == 2)
      EndTest();
  }

  void AfterTest() override {
    // The first commit will always draw; make sure the second draw triggered
    // by the animation was not cancelled.
    EXPECT_EQ(2, num_draw_layers_);
    EXPECT_EQ(2, num_animate_);
  }

 private:
  int num_animate_;
  int num_draw_layers_;
};

MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestForceRedraw);

class LayerTreeHostAnimationTestAnimateAfterSetNeedsCommit
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestAnimateAfterSetNeedsCommit()
      : num_animate_(0), num_draw_layers_(0) {}

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void BeginMainFrame(const viz::BeginFrameArgs& args) override {
    if (++num_animate_ <= 2) {
      layer_tree_host()->SetNeedsCommit();
      layer_tree_host()->SetNeedsAnimate();
    }
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    if (++num_draw_layers_ == 2)
      EndTest();
  }

  void AfterTest() override {
    // The first commit will always draw; make sure the second draw triggered
    // by the SetNeedsCommit was not cancelled.
    EXPECT_EQ(2, num_draw_layers_);
    EXPECT_GE(num_animate_, 2);
  }

 private:
  int num_animate_;
  int num_draw_layers_;
};

MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestAnimateAfterSetNeedsCommit);

// Animations should not be started when frames are being skipped due to
// checkerboard.
class LayerTreeHostAnimationTestCheckerboardDoesntStartAnimations
    : public LayerTreeHostAnimationTest {
  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    picture_ = FakePictureLayer::Create(&client_);
    picture_->SetBounds(gfx::Size(4, 4));
    client_.set_bounds(picture_->bounds());
    layer_tree_host()->root_layer()->AddChild(picture_);

    AttachAnimationsToTimeline();
    animation_child_->AttachElement(picture_->element_id());
    animation_child_->set_animation_delegate(this);
  }

  void BeginTest() override {
    prevented_draw_ = 0;
    started_times_ = 0;

    PostSetNeedsCommitToMainThread();
  }

  DrawResult PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                   LayerTreeHostImpl::FrameData* frame_data,
                                   DrawResult draw_result) override {
    // Don't checkerboard when the first animation wants to start.
    if (host_impl->active_tree()->source_frame_number() < 2)
      return draw_result;
    if (TestEnded())
      return draw_result;
    // Act like there is checkerboard when the second animation wants to draw.
    ++prevented_draw_;
    if (prevented_draw_ > 2)
      EndTest();
    return DrawResult::kAbortedCheckerboardAnimations;
  }

  void DidCommitAndDrawFrame() override {
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 1:
        // The animation is longer than 1 BeginFrame interval.
        AddOpacityTransitionToAnimation(animation_child_.get(), 0.1, 0.2f, 0.8f,
                                        false);
        break;
      case 2:
        // This second animation will not be drawn so it should not start.
        AddAnimatedTransformToAnimation(animation_child_.get(), 0.1, 5, 5);
        break;
    }
  }

  void NotifyAnimationStarted(base::TimeTicks monotonic_time,
                              int target_property,
                              int group) override {
    if (TestEnded())
      return;
    started_times_++;
  }

  void AfterTest() override {
    // Make sure we tried to draw the second animation but failed.
    EXPECT_LT(0, prevented_draw_);
    // The first animation should be started, but the second should not because
    // of checkerboard.
    EXPECT_EQ(1, started_times_);
  }

  int prevented_draw_;
  int started_times_;
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> picture_;
};

MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestCheckerboardDoesntStartAnimations);

// Verifies that a scroll offset animation sends scroll offset updates back to
// the main thread.
class LayerTreeHostAnimationTestScrollOffsetChangesArePropagated
    : public LayerTreeHostAnimationTest {
 public:
  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();

    scroll_layer_ = FakePictureLayer::Create(&client_);
    scroll_layer_->SetScrollable(gfx::Size(100, 100));
    scroll_layer_->SetBounds(gfx::Size(1000, 1000));
    client_.set_bounds(scroll_layer_->bounds());
    scroll_layer_->SetScrollOffset(gfx::PointF(10, 20));
    layer_tree_host()->root_layer()->AddChild(scroll_layer_);

    AttachAnimationsToTimeline();
    animation_child_->AttachElement(scroll_layer_->element_id());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override {
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 1: {
        std::unique_ptr<ScrollOffsetAnimationCurve> curve(
            ScrollOffsetAnimationCurveFactory::
                CreateEaseInOutAnimationForTesting(gfx::PointF(500.f, 550.f)));
        std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
            std::move(curve), 1, 0,
            KeyframeModel::TargetPropertyId(TargetProperty::SCROLL_OFFSET)));
        keyframe_model->set_needs_synchronized_start_time(true);
        animation_child_->AddKeyframeModel(std::move(keyframe_model));
        break;
      }
      default:
        EXPECT_GE(scroll_layer_->scroll_offset().x(), 10);
        EXPECT_GE(scroll_layer_->scroll_offset().y(), 20);
        if (scroll_layer_->scroll_offset().x() > 10 &&
            scroll_layer_->scroll_offset().y() > 20)
          EndTest();
    }
  }

 private:
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> scroll_layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestScrollOffsetChangesArePropagated);

// Verifies that when a main thread scrolling reason gets added, the
// notification to takover the animation on the main thread gets sent.
class LayerTreeHostAnimationTestScrollOffsetAnimationTakeover
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestScrollOffsetAnimationTakeover() = default;

  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();

    scroll_layer_ = FakePictureLayer::Create(&client_);
    scroll_layer_->SetBounds(gfx::Size(10000, 10000));
    client_.set_bounds(scroll_layer_->bounds());
    scroll_layer_->SetScrollOffset(gfx::PointF(10, 20));
    scroll_layer_->SetScrollable(gfx::Size(10, 10));
    layer_tree_host()->root_layer()->AddChild(scroll_layer_);

    AttachAnimationsToTimeline();
    animation_child_->AttachElement(scroll_layer_->element_id());
    // Allows NotifyAnimationTakeover to get called.
    animation_child_->set_animation_delegate(this);
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override {
    if (layer_tree_host()->SourceFrameNumber() == 1) {
      // Add an update after the first commit to trigger the animation takeover
      // path.
      animation_host()->scroll_offset_animations().AddTakeoverUpdate(
          scroll_layer_->element_id());
      EXPECT_TRUE(
          animation_host()->scroll_offset_animations().HasUpdatesForTesting());
    }
  }

  void WillCommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    if (host_impl->sync_tree()->source_frame_number() == 0) {
      GetImplAnimationHost(host_impl)->ImplOnlyScrollAnimationCreate(
          scroll_layer_->element_id(), gfx::PointF(650.f, 750.f),
          gfx::PointF(10, 20), base::TimeDelta(), base::TimeDelta());
    }
  }

  void NotifyAnimationTakeover(
      base::TimeTicks monotonic_time,
      int target_property,
      base::TimeTicks animation_start_time,
      std::unique_ptr<gfx::AnimationCurve> curve) override {
    EndTest();
  }

 private:
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> scroll_layer_;
};

// TODO(crbug.com/40655283):  [BlinkGenPropertyTrees] Scroll Animation should be
// taken over from cc when scroll is unpromoted.
// MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestScrollOffsetAnimationTakeover);

// Verifies that an impl-only scroll offset animation gets updated when the
// scroll offset is adjusted on the main thread.
class LayerTreeHostAnimationTestScrollOffsetAnimationAdjusted
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestScrollOffsetAnimationAdjusted() = default;

  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();

    scroll_layer_ = FakePictureLayer::Create(&client_);
    scroll_layer_->SetBounds(gfx::Size(10000, 10000));
    client_.set_bounds(scroll_layer_->bounds());
    scroll_layer_->SetScrollOffset(gfx::PointF(10, 20));
    scroll_layer_->SetScrollable(gfx::Size(10, 10));
    layer_tree_host()->root_layer()->AddChild(scroll_layer_);

    AttachAnimationsToTimeline();
    scroll_layer_element_id_ = scroll_layer_->element_id();
  }

  const KeyframeEffect& ScrollOffsetKeyframeEffect(
      const LayerTreeHostImpl& host_impl,
      ElementId element_id) const {
    scoped_refptr<const ElementAnimations> element_animations =
        GetImplAnimationHost(&host_impl)
            ->GetElementAnimationsForElementIdForTesting(element_id);
    DCHECK(element_animations);
    const KeyframeEffect* keyframe_effect =
        &*element_animations->FirstKeyframeEffectForTesting();
    DCHECK(keyframe_effect);
    return *keyframe_effect;
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override {
    if (layer_tree_host()->SourceFrameNumber() == 2) {
      // Add an update after the first commit to trigger the animation update
      // path.
      animation_host()->scroll_offset_animations().AddAdjustmentUpdate(
          scroll_layer_element_id_, gfx::Vector2dF(100.f, 100.f));
      EXPECT_TRUE(
          animation_host()->scroll_offset_animations().HasUpdatesForTesting());
    } else if (layer_tree_host()->SourceFrameNumber() == 3) {
      // Verify that the update queue is cleared after the update is applied.
      EXPECT_FALSE(
          animation_host()->scroll_offset_animations().HasUpdatesForTesting());
    }
  }

  void BeginCommitOnThread(LayerTreeHostImpl* host_impl) override {
    // Note that the frame number gets incremented after BeginCommitOnThread but
    // before WillCommitCompleteOnThread and CommitCompleteOnThread.
    if (host_impl->sync_tree()->source_frame_number() == 2) {
      GetImplTimelineAndAnimationByID(*host_impl);
      // This happens after the impl-only animation is added in
      // WillCommitCompleteOnThread.
      gfx::KeyframeModel* keyframe_model =
          ScrollOffsetKeyframeEffect(*host_impl, scroll_layer_element_id_)
              .GetKeyframeModel(TargetProperty::SCROLL_OFFSET);
      DCHECK(keyframe_model);
      const ScrollOffsetAnimationCurve* curve =
          ScrollOffsetAnimationCurve::ToScrollOffsetAnimationCurve(
              keyframe_model->curve());

      // Verifiy the initial and target position before the scroll offset
      // update from MT.
      EXPECT_EQ(gfx::PointF(10.f, 20.f), curve->GetValue(base::TimeDelta()));
      EXPECT_EQ(gfx::PointF(650.f, 750.f), curve->target_value());
    }
  }

  void WillActivateTreeOnThread(LayerTreeHostImpl* host_impl) override {
    if (host_impl->sync_tree()->source_frame_number() == 0) {
      // Once we activate frame 0 we can start the impl scroll animation as
      // the referenced node will be available on the active tree.
      GetImplAnimationHost(host_impl)->ImplOnlyScrollAnimationCreate(
          scroll_layer_element_id_, gfx::PointF(650.f, 750.f),
          gfx::PointF(10, 20), base::TimeDelta(), base::TimeDelta());
    }
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    if (host_impl->sync_tree()->source_frame_number() == 2) {
      gfx::KeyframeModel* keyframe_model =
          ScrollOffsetKeyframeEffect(*host_impl, scroll_layer_element_id_)
              .GetKeyframeModel(TargetProperty::SCROLL_OFFSET);
      DCHECK(keyframe_model);
      const ScrollOffsetAnimationCurve* curve =
          ScrollOffsetAnimationCurve::ToScrollOffsetAnimationCurve(
              keyframe_model->curve());
      // Verifiy the initial and target position after the scroll offset
      // update from MT
      EXPECT_EQ(KeyframeModel::RunState::STARTING, keyframe_model->run_state());
      EXPECT_EQ(gfx::PointF(110.f, 120.f), curve->GetValue(base::TimeDelta()));
      EXPECT_EQ(gfx::PointF(750.f, 850.f), curve->target_value());

      EndTest();
    }
  }

 private:
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> scroll_layer_;
  ElementId scroll_layer_element_id_;
};

MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestScrollOffsetAnimationAdjusted);

// Tests that presentation-time requested from the main-thread is attached to
// the correct frame (i.e. activation needs to happen before the
// presentation-request is attached to the frame).
class LayerTreeHostPresentationDuringAnimation
    : public LayerTreeHostAnimationTest {
 public:
  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();

    scroll_layer_ = FakePictureLayer::Create(&client_);
    scroll_layer_->SetScrollable(gfx::Size(100, 100));
    scroll_layer_->SetBounds(gfx::Size(10000, 10000));
    client_.set_bounds(scroll_layer_->bounds());
    scroll_layer_->SetScrollOffset(gfx::PointF(100.0, 200.0));
    layer_tree_host()->root_layer()->AddChild(scroll_layer_);

    std::unique_ptr<ScrollOffsetAnimationCurve> curve(
        ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimationForTesting(
            gfx::PointF(6500.f, 7500.f)));
    std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
        std::move(curve), 1, 0,
        KeyframeModel::TargetPropertyId(TargetProperty::SCROLL_OFFSET)));
    keyframe_model->set_needs_synchronized_start_time(true);

    AttachAnimationsToTimeline();
    animation_child_->AttachElement(scroll_layer_->element_id());
    animation_child_->AddKeyframeModel(std::move(keyframe_model));
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void BeginMainFrame(const viz::BeginFrameArgs& args) override {
    PostSetNeedsCommitToMainThread();
    if (const_cast<const LayerTreeHost*>(layer_tree_host())
            ->pending_commit_state()
            ->source_frame_number == 2) {
      layer_tree_host()->RequestSuccessfulPresentationTimeForNextFrame(
          base::BindOnce(
              &LayerTreeHostPresentationDuringAnimation::OnPresentation,
              base::Unretained(this)));
    }
  }

  void BeginCommitOnThread(LayerTreeHostImpl* host_impl) override {
    if (host_impl->active_tree()->source_frame_number() == 1) {
      request_token_ = host_impl->next_frame_token();
      host_impl->BlockNotifyReadyToActivateForTesting(true);
    }
  }

  void WillBeginImplFrameOnThread(LayerTreeHostImpl* host_impl,
                                  const viz::BeginFrameArgs& args,
                                  bool has_damage) override {
    if (host_impl->next_frame_token() >= 5)
      host_impl->BlockNotifyReadyToActivateForTesting(false);
  }

  void DisplayReceivedCompositorFrameOnThread(
      const viz::CompositorFrame& frame) override {
    received_token_ = frame.metadata.frame_token;
  }

  void AfterTest() override {
    EXPECT_GT(request_token_, 0u);
    EXPECT_GT(received_token_, request_token_);
    EXPECT_GE(received_token_, 5u);
    EXPECT_TRUE(base::StatisticsRecorder::FindHistogram(
        "CompositorLatency.TotalLatency"));
    EXPECT_FALSE(base::StatisticsRecorder::FindHistogram(
        "CompositorLatency.Universal.TotalLatency"));
  }

 private:
  void OnPresentation(const viz::FrameTimingDetails& details) { EndTest(); }

  // Disable sub-sampling to deterministically record histograms under test.
  base::MetricsSubSampler::ScopedAlwaysSampleForTesting no_subsampling_;

  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> scroll_layer_;
  uint32_t request_token_ = 0;
  uint32_t received_token_ = 0;
};

// Disabled on ChromeOS due to test flakiness. See https://crbug.com/1246422
#if !BUILDFLAG(IS_CHROMEOS)
MULTI_THREAD_TEST_F(LayerTreeHostPresentationDuringAnimation);
#endif

// Verifies that when the main thread removes a scroll animation and sets a new
// scroll position, the active tree takes on exactly this new scroll position
// after activation, and the main thread doesn't receive a spurious scroll
// delta.
class LayerTreeHostAnimationTestScrollOffsetAnimationRemoval
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestScrollOffsetAnimationRemoval()
      : final_postion_(50.0, 100.0) {}

  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();

    scroll_layer_ = FakePictureLayer::Create(&client_);
    scroll_layer_->SetScrollable(gfx::Size(100, 100));
    scroll_layer_->SetBounds(gfx::Size(10000, 10000));
    client_.set_bounds(scroll_layer_->bounds());
    scroll_layer_->SetScrollOffset(gfx::PointF(100.0, 200.0));
    layer_tree_host()->root_layer()->AddChild(scroll_layer_);

    std::unique_ptr<ScrollOffsetAnimationCurve> curve(
        ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimationForTesting(
            gfx::PointF(6500.f, 7500.f)));
    std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
        std::move(curve), 1, 0,
        KeyframeModel::TargetPropertyId(TargetProperty::SCROLL_OFFSET)));
    keyframe_model->set_needs_synchronized_start_time(true);

    AttachAnimationsToTimeline();
    animation_child_->AttachElement(scroll_layer_->element_id());
    animation_child_->AddKeyframeModel(std::move(keyframe_model));
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void BeginMainFrame(const viz::BeginFrameArgs& args) override {
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 0:
        EXPECT_EQ(scroll_layer_->scroll_offset().x(), 100);
        EXPECT_EQ(scroll_layer_->scroll_offset().y(), 200);
        break;
      case 1: {
        EXPECT_GE(scroll_layer_->scroll_offset().x(), 100);
        EXPECT_GE(scroll_layer_->scroll_offset().y(), 200);
        KeyframeModel* keyframe_model =
            animation_child_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET);
        animation_child_->RemoveKeyframeModel(keyframe_model->id());
        scroll_layer_->SetScrollOffset(final_postion_);
        break;
      }
      default:
        EXPECT_EQ(final_postion_, scroll_layer_->scroll_offset());
    }
  }

  void BeginCommitOnThread(LayerTreeHostImpl* host_impl) override {
    host_impl->BlockNotifyReadyToActivateForTesting(
        ShouldBlockActivation(host_impl));
  }

  void WillBeginImplFrameOnThread(LayerTreeHostImpl* host_impl,
                                  const viz::BeginFrameArgs& args,
                                  bool has_damage) override {
    host_impl->BlockNotifyReadyToActivateForTesting(
        ShouldBlockActivation(host_impl));
  }

  void WillActivateTreeOnThread(LayerTreeHostImpl* host_impl) override {
    if (host_impl->pending_tree()->source_frame_number() != 1)
      return;
    LayerImpl* scroll_layer_impl =
        host_impl->pending_tree()->LayerById(scroll_layer_->id());
    EXPECT_EQ(final_postion_, CurrentScrollOffset(scroll_layer_impl));
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) override {
    if (host_impl->active_tree()->source_frame_number() != 1)
      return;
    LayerImpl* scroll_layer_impl =
        host_impl->active_tree()->LayerById(scroll_layer_->id());
    EXPECT_EQ(final_postion_, CurrentScrollOffset(scroll_layer_impl));
    EndTest();
  }

  void AfterTest() override {
    EXPECT_EQ(final_postion_, scroll_layer_->scroll_offset());
  }

 private:
  bool ShouldBlockActivation(LayerTreeHostImpl* host_impl) {
    if (!host_impl->pending_tree())
      return false;

    if (!host_impl->active_tree()->root_layer())
      return false;

    scoped_refptr<const AnimationTimeline> timeline_impl =
        GetImplAnimationHost(host_impl)->GetTimelineById(timeline_id_);
    scoped_refptr<Animation> animation_impl =
        timeline_impl->GetAnimationById(animation_child_id_);

    LayerImpl* scroll_layer_impl =
        host_impl->active_tree()->LayerById(scroll_layer_->id());
    KeyframeModel* keyframe_model =
        animation_impl->GetKeyframeModel(TargetProperty::SCROLL_OFFSET);

    if (!keyframe_model ||
        keyframe_model->run_state() != KeyframeModel::RUNNING)
      return false;

    // Block activation until the running animation has a chance to produce a
    // scroll delta.
    gfx::Vector2dF scroll_delta = ScrollDelta(scroll_layer_impl);
    if (scroll_delta.x() > 0.f || scroll_delta.y() > 0.f)
      return false;

    return true;
  }

  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> scroll_layer_;
  const gfx::PointF final_postion_;
};

MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestScrollOffsetAnimationRemoval);

// Verifies that the state of a scroll animation is tracked correctly on the
// main and compositor thread and that the KeyframeModel is removed when
// the scroll animation completes. This is a regression test for
// https://crbug.com/962346 and demonstrates the necessity of
// LayerTreeHost::AnimateLayers for https://crbug.com/762717.
class LayerTreeHostAnimationTestScrollOffsetAnimationCompletion
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestScrollOffsetAnimationCompletion()
      : final_position_(80.0, 180.0) {}

  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();

    scroll_layer_ = FakePictureLayer::Create(&client_);
    scroll_layer_->SetScrollable(gfx::Size(100, 100));
    scroll_layer_->SetBounds(gfx::Size(10000, 10000));
    client_.set_bounds(scroll_layer_->bounds());
    scroll_layer_->SetScrollOffset(gfx::PointF(100.0, 200.0));
    layer_tree_host()->root_layer()->AddChild(scroll_layer_);

    std::unique_ptr<ScrollOffsetAnimationCurve> curve(
        ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimationForTesting(
            final_position_));
    std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
        std::move(curve), 1, 0,
        KeyframeModel::TargetPropertyId(TargetProperty::SCROLL_OFFSET)));
    keyframe_model->set_needs_synchronized_start_time(true);

    AttachAnimationsToTimeline();
    animation_child_->AttachElement(scroll_layer_->element_id());
    animation_child_->AddKeyframeModel(std::move(keyframe_model));
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void BeginMainFrame(const viz::BeginFrameArgs& args) override {
    KeyframeModel* keyframe_model =
        animation_child_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET);
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 0:
        EXPECT_EQ(scroll_layer_->scroll_offset().x(), 100);
        EXPECT_EQ(scroll_layer_->scroll_offset().y(), 200);
        EXPECT_EQ(KeyframeModel::RunState::WAITING_FOR_TARGET_AVAILABILITY,
                  keyframe_model->run_state());
        break;
      case 1:
        EXPECT_EQ(KeyframeModel::RunState::RUNNING,
                  keyframe_model->run_state());
        break;
      default:
        break;
    }
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    if (host_impl->sync_tree()->source_frame_number() == 0) {
      GetImplTimelineAndAnimationByID(*host_impl);
      return;
    }
    KeyframeModel* keyframe_model =
        animation_child_impl_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET);
    if (!keyframe_model || keyframe_model->run_state() ==
                               KeyframeModel::RunState::WAITING_FOR_DELETION)
      EndTest();
  }

  void DidFinishImplFrameOnThread(LayerTreeHostImpl* host_impl) override {
    if (!animation_child_impl_)
      return;
    if (KeyframeModel* keyframe_model = animation_child_impl_->GetKeyframeModel(
            TargetProperty::SCROLL_OFFSET)) {
      if (keyframe_model->run_state() == KeyframeModel::RunState::RUNNING) {
        ran_animation_ = true;
      }
    }
  }

  void AfterTest() override {
    // The animation should have run for some frames.
    EXPECT_TRUE(ran_animation_);

    // The finished KeyframeModel should have been removed from both the
    // main and impl side animations.
    EXPECT_EQ(nullptr, animation_child_->GetKeyframeModel(
                           TargetProperty::SCROLL_OFFSET));
    EXPECT_EQ(nullptr, animation_child_impl_->GetKeyframeModel(
                           TargetProperty::SCROLL_OFFSET));

    // The scroll should have been completed.
    EXPECT_EQ(final_position_, scroll_layer_->scroll_offset());
  }

 private:
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> scroll_layer_;
  const gfx::PointF final_position_;
  bool ran_animation_ = false;
};

MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestScrollOffsetAnimationCompletion);

// When animations are simultaneously added to an existing layer and to a new
// layer, they should start at the same time, even when there's already a
// running animation on the existing layer.
class LayerTreeHostAnimationTestAnimationsAddedToNewAndExistingLayers
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestAnimationsAddedToNewAndExistingLayers()
      : frame_count_with_pending_tree_(0) {}

  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    layer_ = Layer::Create();
    layer_->SetBounds(gfx::Size(4, 4));
    layer_tree_host()->root_layer()->AddChild(layer_);
    layer_tree_host()->SetElementIdsForTesting();
  }

  void BeginTest() override {
    AttachAnimationsToTimeline();
    PostSetNeedsCommitToMainThread();
  }

  void DidCommit() override {
    if (layer_tree_host()->SourceFrameNumber() == 1) {
      animation_->AttachElement(layer_->element_id());
      AddAnimatedTransformToAnimation(animation_.get(), 4, 1, 1);
    } else if (layer_tree_host()->SourceFrameNumber() == 2) {
      AddOpacityTransitionToAnimation(animation_.get(), 1, 0.f, 0.5f, true);

      scoped_refptr<Layer> child_layer = Layer::Create();
      layer_->AddChild(child_layer);

      layer_tree_host()->SetElementIdsForTesting();
      child_layer->SetBounds(gfx::Size(4, 4));

      animation_child_->AttachElement(child_layer->element_id());
      animation_child_->set_animation_delegate(this);
      AddOpacityTransitionToAnimation(animation_child_.get(), 1, 0.f, 0.5f,
                                      true);
    }
  }

  void BeginCommitOnThread(LayerTreeHostImpl* host_impl) override {
    host_impl->BlockNotifyReadyToActivateForTesting(true);
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    // For the commit that added animations to new and existing layers, keep
    // blocking activation. We want to verify that even with activation blocked,
    // the animation on the layer that's already in the active tree won't get a
    // head start.
    if (host_impl->pending_tree()->source_frame_number() != 2) {
      host_impl->BlockNotifyReadyToActivateForTesting(false);
    }
  }

  void WillBeginImplFrameOnThread(LayerTreeHostImpl* host_impl,
                                  const viz::BeginFrameArgs& args,
                                  bool has_damage) override {
    if (!host_impl->pending_tree() ||
        host_impl->pending_tree()->source_frame_number() != 2)
      return;

    frame_count_with_pending_tree_++;
    if (frame_count_with_pending_tree_ == 2) {
      host_impl->BlockNotifyReadyToActivateForTesting(false);
    }
  }

  void UpdateAnimationState(LayerTreeHostImpl* host_impl,
                            bool has_unfinished_animation) override {
    scoped_refptr<AnimationTimeline> timeline_impl =
        GetImplAnimationHost(host_impl)->GetTimelineById(timeline_id_);
    scoped_refptr<Animation> animation_impl =
        timeline_impl->GetAnimationById(animation_id_);
    scoped_refptr<Animation> animation_child_impl =
        timeline_impl->GetAnimationById(animation_child_id_);

    // wait for tree activation.
    if (!animation_impl->element_animations())
      return;

    KeyframeModel* root_keyframe_model =
        animation_impl->GetKeyframeModel(TargetProperty::OPACITY);
    if (!root_keyframe_model ||
        root_keyframe_model->run_state() != KeyframeModel::RUNNING)
      return;

    KeyframeModel* child_keyframe_model =
        animation_child_impl->GetKeyframeModel(TargetProperty::OPACITY);
    EXPECT_EQ(KeyframeModel::RUNNING, child_keyframe_model->run_state());
    EXPECT_EQ(root_keyframe_model->start_time(),
              child_keyframe_model->start_time());
    animation_impl->AbortKeyframeModelsWithProperty(TargetProperty::OPACITY,
                                                    false);
    animation_impl->AbortKeyframeModelsWithProperty(TargetProperty::TRANSFORM,
                                                    false);
    animation_child_impl->AbortKeyframeModelsWithProperty(
        TargetProperty::OPACITY, false);
    EndTest();
  }

 private:
  scoped_refptr<Layer> layer_;
  int frame_count_with_pending_tree_;
};

// This test blocks activation which is not supported for single thread mode.
MULTI_THREAD_BLOCKNOTIFY_TEST_F(
    LayerTreeHostAnimationTestAnimationsAddedToNewAndExistingLayers);

class LayerTreeHostAnimationTestAddKeyframeModelAfterAnimating
    : public LayerTreeHostAnimationTest {
 public:
  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    layer_ = Layer::Create();
    layer_->SetBounds(gfx::Size(4, 4));
    layer_tree_host()->root_layer()->AddChild(layer_);
    child_layer_ = Layer::Create();
    child_layer_->SetBounds(gfx::Size(4, 4));
    layer_->AddChild(child_layer_);

    AttachAnimationsToTimeline();

    animation_->AttachElement(layer_->element_id());
    animation_child_->AttachElement(child_layer_->element_id());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override {
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 1:
        // First frame: add an animation to the root layer.
        AddAnimatedTransformToAnimation(animation_.get(), 0.1, 5, 5);
        break;
      case 2:
        // Second frame: add an animation to the content layer. The root layer
        // animation has caused us to animate already during this frame.
        AddOpacityTransitionToAnimation(animation_child_.get(), 0.1, 5, 5,
                                        false);
        break;
    }
  }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    // After both animations have started, verify that they have valid
    // start times.
    if (host_impl->active_tree()->source_frame_number() < 2)
      return;

    // Animation state is updated after drawing. Only do this once.
    if (!TestEnded()) {
      ImplThreadTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &LayerTreeHostAnimationTestAddKeyframeModelAfterAnimating::
                  CheckAnimations,
              base::Unretained(this), host_impl));
    }
  }

  void CheckAnimations(LayerTreeHostImpl* host_impl) {
    GetImplTimelineAndAnimationByID(*host_impl);

    EXPECT_EQ(2u, GetImplAnimationHost(host_impl)
                      ->ticking_animations_for_testing()
                      .size());

    KeyframeModel* root_anim =
        animation_impl_->GetKeyframeModel(TargetProperty::TRANSFORM);
    EXPECT_LT(base::TimeTicks(), root_anim->start_time());

    KeyframeModel* anim =
        animation_child_impl_->GetKeyframeModel(TargetProperty::OPACITY);
    EXPECT_LT(base::TimeTicks(), anim->start_time());

    EndTest();
  }

 private:
  scoped_refptr<Layer> layer_;
  scoped_refptr<Layer> child_layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestAddKeyframeModelAfterAnimating);

class LayerTreeHostAnimationTestRemoveKeyframeModel
    : public LayerTreeHostAnimationTest {
 public:
  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    layer_ = FakePictureLayer::Create(&client_);
    layer_->SetBounds(gfx::Size(4, 4));
    client_.set_bounds(layer_->bounds());
    layer_tree_host()->root_layer()->AddChild(layer_);

    AttachAnimationsToTimeline();

    animation_->AttachElement(layer_tree_host()->root_layer()->element_id());
    animation_child_->AttachElement(layer_->element_id());
  }

  void BeginTest() override {
    animation_stopped_ = false;
    last_frame_number_ = INT_MAX;
    PostSetNeedsCommitToMainThread();
  }

  void DidCommit() override {
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 1:
        AddAnimatedTransformToAnimation(animation_child_.get(), 1.0, 5, 5);
        break;
      case 2:
        KeyframeModel* keyframe_model =
            animation_child_->GetKeyframeModel(TargetProperty::TRANSFORM);
        animation_child_->RemoveKeyframeModel(keyframe_model->id());
        gfx::Transform transform;
        transform.Translate(10.f, 10.f);
        layer_->SetTransform(transform);

        // Do something that causes property trees to get rebuilt. This is
        // intended to simulate the conditions that caused the bug whose fix
        // this is testing (the test will pass without it but won't test what
        // we want it to). We were updating the wrong transform node at the end
        // of an animation (we were assuming the layer with the finished
        // animation still had its own transform node). But nodes can only get
        // added/deleted when something triggers a rebuild. Adding a layer
        // triggers a rebuild, and since the layer that had an animation before
        // no longer has one, it doesn't get a transform node in the rebuild.
        layer_->AddChild(Layer::Create());
        break;
    }
  }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    GetImplTimelineAndAnimationByID(*host_impl);
    LayerImpl* child = host_impl->active_tree()->LayerById(layer_->id());
    switch (host_impl->active_tree()->source_frame_number()) {
      case 0:
        // No animation yet.
        break;
      case 1:
        // Animation is started.
        EXPECT_TRUE(child->screen_space_transform_is_animating());
        break;
      case 2: {
        // The animation is stopped, the transform that was set afterward is
        // applied.
        gfx::Transform expected_transform;
        expected_transform.Translate(10.f, 10.f);
        EXPECT_TRANSFORM_EQ(expected_transform, child->DrawTransform());
        EXPECT_FALSE(child->screen_space_transform_is_animating());
        animation_stopped_ = true;
        PostSetNeedsCommitToMainThread();
        break;
      }
    }
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    if (host_impl->sync_tree()->source_frame_number() >= last_frame_number_) {
      // Check that eventually the animation is removed.
      EXPECT_FALSE(
          animation_child_impl_->keyframe_effect()->has_any_keyframe_model());
      EndTest();
    }
  }

  void UpdateAnimationState(LayerTreeHostImpl* host_impl,
                            bool has_unfinished_animation) override {
    // Non impl only animations are removed during commit. After the animation
    // is fully stopped on compositor thread, make sure another commit happens.
    if (animation_stopped_ && !has_unfinished_animation) {
      last_frame_number_ =
          std::min(last_frame_number_,
                   host_impl->active_tree()->source_frame_number() + 1);
      PostSetNeedsCommitToMainThread();
    }
  }

 private:
  scoped_refptr<Layer> layer_;
  FakeContentLayerClient client_;

  int last_frame_number_;
  bool animation_stopped_;
};

// Disabled on ChromeOS due to test flakiness. See https://crbug.com/1246422
#if !BUILDFLAG(IS_CHROMEOS)
SINGLE_THREAD_TEST_F(LayerTreeHostAnimationTestRemoveKeyframeModel);
#endif

MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestRemoveKeyframeModel);

class LayerTreeHostAnimationTestIsAnimating
    : public LayerTreeHostAnimationTest {
 public:
  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    layer_ = FakePictureLayer::Create(&client_);
    layer_->SetBounds(gfx::Size(4, 4));
    client_.set_bounds(layer_->bounds());
    layer_tree_host()->root_layer()->AddChild(layer_);

    AttachAnimationsToTimeline();
    animation_->AttachElement(layer_->element_id());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override {
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 1:
        AddAnimatedTransformToAnimation(animation_.get(), 1.0, 5, 5);
        break;
      case 2:
        KeyframeModel* keyframe_model =
            animation_->GetKeyframeModel(TargetProperty::TRANSFORM);
        EXPECT_EQ(KeyframeModel::RunState::RUNNING,
                  keyframe_model->run_state());
        animation_->RemoveKeyframeModel(keyframe_model->id());
        break;
    }
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    LayerImpl* child = host_impl->sync_tree()->LayerById(layer_->id());
    switch (host_impl->sync_tree()->source_frame_number()) {
      case 0:
        // No animation yet.
        break;
      case 1:
        // Animation is started.
        EXPECT_TRUE(child->screen_space_transform_is_animating());
        break;
      case 2:
        // The animation is removed/stopped.
        EXPECT_FALSE(child->screen_space_transform_is_animating());
        break;
      case 3:
        break;
      default:
        NOTREACHED();
    }
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) override {
    GetImplTimelineAndAnimationByID(*host_impl);
    switch (host_impl->active_tree()->source_frame_number()) {
      case 0:
        // No animation yet.
        break;
      case 1:
        // Animation is starting.
        EXPECT_EQ(KeyframeModel::RunState::STARTING,
                  animation_impl_->GetKeyframeModel(TargetProperty::TRANSFORM)
                      ->run_state());
        break;
      case 2:
        // After activation, the KeyframeModel should be waiting for deletion.
        EXPECT_EQ(KeyframeModel::RunState::WAITING_FOR_DELETION,
                  animation_impl_->GetKeyframeModel(TargetProperty::TRANSFORM)
                      ->run_state());
        break;
      case 3:
        // The animation KeyframeModel is cleaned up.
        EXPECT_EQ(nullptr,
                  animation_impl_->GetKeyframeModel(TargetProperty::TRANSFORM));
        EndTest();
        break;
      default:
        NOTREACHED();
    }
  }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    LayerImpl* child = host_impl->active_tree()->LayerById(layer_->id());
    switch (host_impl->active_tree()->source_frame_number()) {
      case 0:
        // No animation yet.
        break;
      case 1:
        // Animation is started.
        EXPECT_TRUE(child->screen_space_transform_is_animating());
        break;
      case 2:
      case 3:
        // The animation is removed/stopped.
        EXPECT_FALSE(child->screen_space_transform_is_animating());
        break;
      default:
        NOTREACHED();
    }
  }

 private:
  scoped_refptr<Layer> layer_;
  FakeContentLayerClient client_;
};

// TODO(https://issues.chromium.org/41490442): Flaky on Linux/ASAN/debug.
// TODO(crbug.com/364634743): Flaky on Android.
#if BUILDFLAG(IS_LINUX) || defined(ADDRESS_SANITIZER) || !defined(NDEBUG) || \
    BUILDFLAG(IS_ANDROID)
SINGLE_THREAD_TEST_F(LayerTreeHostAnimationTestIsAnimating);
#else
SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestIsAnimating);
#endif

class LayerTreeHostAnimationTestAnimationFinishesDuringCommit
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestAnimationFinishesDuringCommit() = default;

  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    layer_ = FakePictureLayer::Create(&client_);
    layer_->SetBounds(gfx::Size(4, 4));
    client_.set_bounds(layer_->bounds());
    layer_tree_host()->root_layer()->AddChild(layer_);

    AttachAnimationsToTimeline();

    animation_->AttachElement(layer_tree_host()->root_layer()->element_id());
    animation_child_->AttachElement(layer_->element_id());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override {
    if (layer_tree_host()->SourceFrameNumber() == 1) {
      AddAnimatedTransformToAnimation(animation_child_.get(), 0.04, 5, 5);
      // Blink animations will implicitly fill to ensure they remain active
      // until a subsequent commit.
      KeyframeModel* keyframe_model =
          animation_child_->GetKeyframeModel(TargetProperty::TRANSFORM);
      keyframe_model->set_fill_mode(KeyframeModel::FillMode::FORWARDS);
    }
  }

  void WillCommit(const CommitState& commit_state) override {
    if (commit_state.source_frame_number == 2) {
      // Block until the animation finishes on the compositor thread. Since
      // animations have already been ticked on the main thread, when the commit
      // happens the state on the main thread will be consistent with having a
      // running animation but the state on the compositor thread will be
      // consistent with having only a finished animation.
      completion_.Wait();
    }
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    switch (host_impl->sync_tree()->source_frame_number()) {
      case 1:
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        gfx::Transform expected_transform;
        expected_transform.Translate(5.f, 5.f);
        LayerImpl* layer_impl = host_impl->sync_tree()->LayerById(layer_->id());
        EXPECT_TRANSFORM_EQ(expected_transform, layer_impl->DrawTransform());
        EndTest();
        break;
    }
  }

  void UpdateAnimationState(LayerTreeHostImpl* host_impl,
                            bool has_unfinished_animation) override {
    if (host_impl->active_tree()->source_frame_number() == 1 &&
        !has_unfinished_animation && !signalled_) {
      // The animation has finished, so allow the main thread to commit.
      completion_.Signal();
      signalled_ = true;
    }
  }

 private:
  scoped_refptr<Layer> layer_;
  FakeContentLayerClient client_;
  CompletionEvent completion_;
  bool signalled_ = false;
};

// An animation finishing during commit can only happen when we have a separate
// compositor thread.
MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestAnimationFinishesDuringCommit);

class LayerTreeHostAnimationTestImplSideInvalidation
    : public LayerTreeHostAnimationTest {
 public:
  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    layer_ = FakePictureLayer::Create(&client_);
    layer_->SetBounds(gfx::Size(4, 4));
    client_.set_bounds(layer_->bounds());
    layer_tree_host()->root_layer()->AddChild(layer_);

    AttachAnimationsToTimeline();

    animation_->AttachElement(layer_tree_host()->root_layer()->element_id());
    animation_child_->AttachElement(layer_->element_id());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override {
    if (layer_tree_host()->SourceFrameNumber() == 1) {
      AddAnimatedTransformToAnimation(animation_child_.get(), 0.04, 5, 5);
      KeyframeModel* keyframe_model =
          animation_child_->GetKeyframeModel(TargetProperty::TRANSFORM);
      keyframe_model->set_fill_mode(KeyframeModel::FillMode::BOTH);
    }
  }

  void WillCommit(const CommitState& commit_state) override {
    if (commit_state.source_frame_number == 2) {
      // Block until the animation finishes on the compositor thread. Since
      // animations have already been ticked on the main thread, when the commit
      // happens the state on the main thread will be consistent with having a
      // running animation but the state on the compositor thread will be
      // consistent with having only a finished animation.
      completion_.Wait();
    }
  }

  void DidInvalidateContentOnImplSide(LayerTreeHostImpl* host_impl) override {
    DCHECK(did_request_impl_side_invalidation_);
    completion_.Signal();
  }

  void UpdateAnimationState(LayerTreeHostImpl* host_impl,
                            bool has_unfinished_animation) override {
    if (host_impl->active_tree()->source_frame_number() == 1 &&
        !has_unfinished_animation && !did_request_impl_side_invalidation_) {
      // The animation on the active tree has finished, now request an impl-side
      // invalidation and make sure it finishes before the main thread is
      // released.
      did_request_impl_side_invalidation_ = true;
      host_impl->RequestImplSideInvalidationForCheckerImagedTiles();
    }
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    switch (host_impl->sync_tree()->source_frame_number()) {
      case 1:
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        gfx::Transform expected_transform;
        expected_transform.Translate(5.f, 5.f);
        LayerImpl* layer_impl = host_impl->sync_tree()->LayerById(layer_->id());
        EXPECT_TRANSFORM_EQ(expected_transform, layer_impl->DrawTransform());
        EndTest();
        break;
    }
  }

 private:
  scoped_refptr<Layer> layer_;
  FakeContentLayerClient client_;
  CompletionEvent completion_;
  bool did_request_impl_side_invalidation_ = false;
};

MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestImplSideInvalidation);

class LayerTreeHostAnimationTestImplSideInvalidationWithoutCommit
    : public LayerTreeHostAnimationTest {
 public:
  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    layer_ = FakePictureLayer::Create(&client_);
    layer_->SetBounds(gfx::Size(4, 4));
    client_.set_bounds(layer_->bounds());
    layer_tree_host()->root_layer()->AddChild(layer_);
    AttachAnimationsToTimeline();
    layer_element_id_ = layer_->element_id();
    animation_child_->AttachElement(layer_element_id_);
    num_draws_ = 0;
  }

  void UpdateAnimationState(LayerTreeHostImpl* host_impl,
                            bool has_unfinished_animation) override {
    if (!has_unfinished_animation && !did_request_impl_side_invalidation_) {
      // The animation on the active tree has finished, now request an impl-side
      // invalidation and verify that the final value on an impl-side pending
      // tree is correct.
      did_request_impl_side_invalidation_ = true;
      host_impl->RequestImplSideInvalidationForCheckerImagedTiles();
    }
  }
  void DidInvalidateContentOnImplSide(LayerTreeHostImpl* host_impl) override {
    completion_.Signal();
    EndTest();
  }

  void WillCommit(const CommitState& commit_state) override {
    if (commit_state.source_frame_number == 1) {
      // Block until the invalidation is done after animation finishes on the
      // compositor thread. We need to make sure the pending tree has valid
      // information based on invalidation only.
      completion_.Wait();
    }
  }

 protected:
  scoped_refptr<Layer> layer_;
  FakeContentLayerClient client_;
  bool did_request_impl_side_invalidation_ = false;
  int num_draws_;
  ElementId layer_element_id_;

 private:
  CompletionEvent completion_;
};

class ImplSideInvalidationWithoutCommitTestOpacity
    : public LayerTreeHostAnimationTestImplSideInvalidationWithoutCommit {
 public:
  void BeginTest() override {
    AddOpacityTransitionToAnimation(animation_child_.get(), 0.04, 0.2f, 0.8f,
                                    false);
    PostSetNeedsCommitToMainThread();
  }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    if (num_draws_++ > 0)
      return;
    EXPECT_EQ(0, host_impl->active_tree()->source_frame_number());
    LayerImpl* layer_impl = host_impl->active_tree()->LayerById(layer_->id());
    EXPECT_FLOAT_EQ(0.2f, layer_impl->Opacity());
  }

  void DidInvalidateContentOnImplSide(LayerTreeHostImpl* host_impl) override {
    ASSERT_TRUE(did_request_impl_side_invalidation_);
    EXPECT_EQ(0, host_impl->sync_tree()->source_frame_number());
    const float expected_opacity = 0.8f;
    LayerImpl* layer_impl = host_impl->sync_tree()->LayerById(layer_->id());
    EXPECT_FLOAT_EQ(expected_opacity, layer_impl->Opacity());
    LayerTreeHostAnimationTestImplSideInvalidationWithoutCommit::
        DidInvalidateContentOnImplSide(host_impl);
  }
};

MULTI_THREAD_TEST_F(ImplSideInvalidationWithoutCommitTestOpacity);

class ImplSideInvalidationWithoutCommitTestTransform
    : public LayerTreeHostAnimationTestImplSideInvalidationWithoutCommit {
 public:
  void BeginTest() override {
    AddAnimatedTransformToAnimation(animation_child_.get(), 0.04, 5, 5);
    PostSetNeedsCommitToMainThread();
  }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    if (num_draws_++ > 0)
      return;
    EXPECT_EQ(0, host_impl->active_tree()->source_frame_number());
    LayerImpl* layer_impl = host_impl->active_tree()->LayerById(layer_->id());
    EXPECT_TRANSFORM_EQ(gfx::Transform(), layer_impl->DrawTransform());
  }

  void DidInvalidateContentOnImplSide(LayerTreeHostImpl* host_impl) override {
    ASSERT_TRUE(did_request_impl_side_invalidation_);
    EXPECT_EQ(0, host_impl->sync_tree()->source_frame_number());
    gfx::Transform expected_transform;
    expected_transform.Translate(5.f, 5.f);
    LayerImpl* layer_impl = host_impl->sync_tree()->LayerById(layer_->id());
    EXPECT_TRANSFORM_EQ(expected_transform, layer_impl->DrawTransform());
    LayerTreeHostAnimationTestImplSideInvalidationWithoutCommit::
        DidInvalidateContentOnImplSide(host_impl);
  }
};

MULTI_THREAD_TEST_F(ImplSideInvalidationWithoutCommitTestTransform);

class ImplSideInvalidationWithoutCommitTestFilter
    : public LayerTreeHostAnimationTestImplSideInvalidationWithoutCommit {
 public:
  void BeginTest() override {
    AddAnimatedFilterToAnimation(animation_child_.get(), 0.04, 0.f, 1.f);
    PostSetNeedsCommitToMainThread();
  }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    if (num_draws_++ > 0)
      return;
    EXPECT_EQ(0, host_impl->active_tree()->source_frame_number());
    EXPECT_EQ(
        std::string("{\"FilterOperations\":[{\"type\":5,\"amount\":0.0}]}"),
        host_impl->active_tree()
            ->property_trees()
            ->effect_tree()
            .FindNodeFromElementId(layer_element_id_)
            ->filters.ToString());
  }

  void DidInvalidateContentOnImplSide(LayerTreeHostImpl* host_impl) override {
    ASSERT_TRUE(did_request_impl_side_invalidation_);
    EXPECT_EQ(0, host_impl->sync_tree()->source_frame_number());
    // AddAnimatedFilterToAnimation adds brightness filter which is type 5.
    EXPECT_EQ(
        std::string("{\"FilterOperations\":[{\"type\":5,\"amount\":1.0}]}"),
        host_impl->sync_tree()
            ->property_trees()
            ->effect_tree()
            .FindNodeFromElementId(layer_element_id_)
            ->filters.ToString());
    LayerTreeHostAnimationTestImplSideInvalidationWithoutCommit::
        DidInvalidateContentOnImplSide(host_impl);
  }
};

MULTI_THREAD_TEST_F(ImplSideInvalidationWithoutCommitTestFilter);

class ImplSideInvalidationWithoutCommitTestScroll
    : public LayerTreeHostAnimationTestImplSideInvalidationWithoutCommit {
 public:
  void SetupTree() override {
    LayerTreeHostAnimationTestImplSideInvalidationWithoutCommit::SetupTree();

    layer_->SetScrollable(gfx::Size(100, 100));
    layer_->SetBounds(gfx::Size(1000, 1000));
    client_.set_bounds(layer_->bounds());
    layer_->SetScrollOffset(gfx::PointF(10.f, 20.f));
  }

  void BeginTest() override {
    std::unique_ptr<ScrollOffsetAnimationCurve> curve(
        ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimationForTesting(
            gfx::PointF(500.f, 550.f)));
    std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
        std::move(curve), 1, 0,
        KeyframeModel::TargetPropertyId(TargetProperty::SCROLL_OFFSET)));
    keyframe_model->set_needs_synchronized_start_time(true);
    animation_child_->AddKeyframeModel(std::move(keyframe_model));
    PostSetNeedsCommitToMainThread();
  }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    if (num_draws_++ > 0)
      return;
    EXPECT_EQ(0, host_impl->active_tree()->source_frame_number());
    LayerImpl* layer_impl = host_impl->active_tree()->LayerById(layer_->id());
    EXPECT_EQ(gfx::PointF(10.f, 20.f), CurrentScrollOffset(layer_impl));
  }

  void DidInvalidateContentOnImplSide(LayerTreeHostImpl* host_impl) override {
    ASSERT_TRUE(did_request_impl_side_invalidation_);
    EXPECT_EQ(0, host_impl->sync_tree()->source_frame_number());
    LayerImpl* layer_impl = host_impl->pending_tree()->LayerById(layer_->id());
    EXPECT_EQ(gfx::PointF(500.f, 550.f), CurrentScrollOffset(layer_impl));
    LayerTreeHostAnimationTestImplSideInvalidationWithoutCommit::
        DidInvalidateContentOnImplSide(host_impl);
  }
};

MULTI_THREAD_TEST_F(ImplSideInvalidationWithoutCommitTestScroll);

class LayerTreeHostAnimationTestNotifyAnimationFinished
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestNotifyAnimationFinished()
      : called_animation_started_(false), called_animation_finished_(false) {}

  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    picture_ = FakePictureLayer::Create(&client_);
    picture_->SetBounds(gfx::Size(4, 4));
    client_.set_bounds(picture_->bounds());
    layer_tree_host()->root_layer()->AddChild(picture_);

    AttachAnimationsToTimeline();
    animation_->AttachElement(picture_->element_id());
    animation_->set_animation_delegate(this);
  }

  void BeginTest() override {
    PostAddOpacityAnimationToMainThreadDelayed(animation_.get());
  }

  void NotifyAnimationStarted(base::TimeTicks monotonic_time,
                              int target_property,
                              int group) override {
    called_animation_started_ = true;
    layer_tree_host()->AnimateLayers(base::TimeTicks::Max());
    PostSetNeedsCommitToMainThread();
  }

  void NotifyAnimationFinished(base::TimeTicks monotonic_time,
                               int target_property,
                               int group) override {
    called_animation_finished_ = true;
    EndTest();
  }

  void AfterTest() override {
    EXPECT_TRUE(called_animation_started_);
    EXPECT_TRUE(called_animation_finished_);
  }

 private:
  bool called_animation_started_;
  bool called_animation_finished_;
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> picture_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestNotifyAnimationFinished);

// Check that transform sync happens correctly at commit when we remove and add
// a different animation animation to an element.
class LayerTreeHostAnimationTestChangeAnimation
    : public LayerTreeHostAnimationTest {
 public:
  void SetUp() override {
    scoped_feature_list.InitAndDisableFeature(
        features::kNoPreserveLastMutation);
    LayerTreeHostAnimationTest::SetUp();
  }

  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    layer_ = Layer::Create();
    layer_->SetBounds(gfx::Size(4, 4));
    layer_tree_host()->root_layer()->AddChild(layer_);

    AttachAnimationsToTimeline();
    layer_element_id_ = layer_->element_id();

    timeline_->DetachAnimation(animation_child_.get());
    animation_->AttachElement(layer_element_id_);

    gfx::TransformOperations start;
    start.AppendTranslate(5.f, 5.f, 0.f);
    gfx::TransformOperations end;
    end.AppendTranslate(5.f, 5.f, 0.f);
    AddAnimatedTransformToAnimation(animation_.get(), 1.0, start, end);
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    PropertyTrees* property_trees = host_impl->sync_tree()->property_trees();
    const TransformNode* node =
        property_trees->transform_tree().Node(host_impl->sync_tree()
                                                  ->LayerById(layer_->id())
                                                  ->transform_tree_index());
    gfx::Transform translate;
    translate.Translate(5, 5);
    switch (host_impl->sync_tree()->source_frame_number()) {
      case 2:
        EXPECT_TRANSFORM_EQ(node->local, translate);
        EndTest();
        break;
      default:
        break;
    }
  }

  void DidCommit() override { PostSetNeedsCommitToMainThread(); }

  void WillBeginMainFrame() override {
    if (layer_tree_host()->SourceFrameNumber() == 2) {
      // Destroy animation.
      timeline_->DetachAnimation(animation_.get());
      animation_ = nullptr;
      timeline_->AttachAnimation(animation_child_.get());
      animation_child_->AttachElement(layer_element_id_);
      AddAnimatedTransformToAnimation(animation_child_.get(), 1.0, 10, 10);
      KeyframeModel* keyframe_model =
          animation_child_->GetKeyframeModel(TargetProperty::TRANSFORM);
      keyframe_model->set_start_time(base::TimeTicks::Now() +
                                     base::Seconds(1000));
      keyframe_model->set_fill_mode(KeyframeModel::FillMode::NONE);
    }
  }

 private:
  scoped_refptr<Layer> layer_;
  ElementId layer_element_id_;
  base::test::ScopedFeatureList scoped_feature_list;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestChangeAnimation);

// Check that SetTransformIsPotentiallyAnimatingChanged is called
// if we destroy ElementAnimations.
class LayerTreeHostAnimationTestSetPotentiallyAnimatingOnLacDestruction
    : public LayerTreeHostAnimationTest {
 public:
  void SetupTree() override {
    prev_screen_space_transform_is_animating_ = true;
    screen_space_transform_animation_stopped_ = false;

    LayerTreeHostAnimationTest::SetupTree();
    AttachAnimationsToTimeline();
    animation_->AttachElement(layer_tree_host()->root_layer()->element_id());
    AddAnimatedTransformToAnimation(animation_.get(), 1.0, 5, 5);
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    if (host_impl->pending_tree()->source_frame_number() <= 1) {
      EXPECT_TRUE(host_impl->pending_tree()
                      ->root_layer()
                      ->screen_space_transform_is_animating());
    } else {
      EXPECT_FALSE(host_impl->pending_tree()
                       ->root_layer()
                       ->screen_space_transform_is_animating());
    }
  }

  void DidCommit() override { PostSetNeedsCommitToMainThread(); }

  void UpdateLayerTreeHost() override {
    if (layer_tree_host()->SourceFrameNumber() == 2) {
      // Destroy animation.
      timeline_->DetachAnimation(animation_.get());
      animation_ = nullptr;
    }
  }

  DrawResult PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                   LayerTreeHostImpl::FrameData* frame_data,
                                   DrawResult draw_result) override {
    const bool screen_space_transform_is_animating =
        host_impl->active_tree()
            ->root_layer()
            ->screen_space_transform_is_animating();

    // Check that screen_space_transform_is_animating changes only once.
    if (screen_space_transform_is_animating &&
        prev_screen_space_transform_is_animating_)
      EXPECT_FALSE(screen_space_transform_animation_stopped_);
    if (!screen_space_transform_is_animating &&
        prev_screen_space_transform_is_animating_) {
      EXPECT_FALSE(screen_space_transform_animation_stopped_);
      screen_space_transform_animation_stopped_ = true;
    }
    if (!screen_space_transform_is_animating &&
        !prev_screen_space_transform_is_animating_)
      EXPECT_TRUE(screen_space_transform_animation_stopped_);

    prev_screen_space_transform_is_animating_ =
        screen_space_transform_is_animating;

    return draw_result;
  }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    if (host_impl->active_tree()->source_frame_number() >= 2)
      EndTest();
  }

  void AfterTest() override {
    EXPECT_TRUE(screen_space_transform_animation_stopped_);
  }

  bool prev_screen_space_transform_is_animating_;
  bool screen_space_transform_animation_stopped_;
};

MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestSetPotentiallyAnimatingOnLacDestruction);

// Check that we invalidate property trees on Animation::SetNeedsCommit.
class LayerTreeHostAnimationTestRebuildPropertyTreesOnAnimationSetNeedsCommit
    : public LayerTreeHostAnimationTest {
 public:
  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    layer_ = FakePictureLayer::Create(&client_);
    layer_->SetBounds(gfx::Size(4, 4));
    client_.set_bounds(layer_->bounds());
    layer_tree_host()->root_layer()->AddChild(layer_);

    AttachAnimationsToTimeline();

    animation_->AttachElement(layer_tree_host()->root_layer()->element_id());
    animation_child_->AttachElement(layer_->element_id());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override {
    if (layer_tree_host()->SourceFrameNumber() == 1 ||
        layer_tree_host()->SourceFrameNumber() == 2)
      PostSetNeedsCommitToMainThread();
  }

  void UpdateLayerTreeHost() override {
    if (layer_tree_host()->SourceFrameNumber() == 1)
      AddAnimatedTransformToAnimation(animation_child_.get(), 1.0, 5, 5);
    EXPECT_TRUE(layer_tree_host()->proxy()->CommitRequested());
  }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    if (host_impl->active_tree()->source_frame_number() >= 2)
      EndTest();
  }

 private:
  scoped_refptr<Layer> layer_;
  FakeContentLayerClient client_;
};

MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestRebuildPropertyTreesOnAnimationSetNeedsCommit);

class LayerTreeHostTestPauseRendering : public LayerTreeHostAnimationTest {
 public:
  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    layer_ = Layer::Create();
    layer_->SetBounds(gfx::Size(4, 4));
    layer_tree_host()->root_layer()->AddChild(layer_);
    layer_tree_host()->SetElementIdsForTesting();
  }

  void BeginTest() override {
    AttachAnimationsToTimeline();

    // Set up an animation which is committed to the impl thread in the first
    // frame.
    animation_->AttachElement(layer_->element_id());
    AddAnimatedTransformToAnimation(animation_.get(), 4, 1, 1);

    PostSetNeedsCommitToMainThread();
  }

  void WillCommit(const CommitState& state) override {
    // First frame pauses rendering.
    if (layer_tree_host()->SourceFrameNumber() == 0) {
      EXPECT_FALSE(rendering_paused_);
      rendering_paused_ = layer_tree_host()->PauseRendering();
    }
  }

  void DidCommitAndDrawFrame() override {
    if (layer_tree_host()->SourceFrameNumber() == 1) {
      rendering_paused_.reset();
    }
  }

  void WillCommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    // If this is the pending tree which resumes rendering, delay its
    // activation so we can ensure draws don't resume until this is activated.
    if (host_impl->pending_tree()->source_frame_number() == 1) {
      host_impl->BlockNotifyReadyToActivateForTesting(true, true);
      has_pending_tree_which_resumes_draws_ = true;
    }
  }

  void WillActivateTreeOnThread(LayerTreeHostImpl* host_impl) override {
    EXPECT_FALSE(has_pending_tree_which_resumes_draws_);
  }

  void WillBeginImplFrameOnThread(LayerTreeHostImpl* host_impl,
                                  const viz::BeginFrameArgs& args,
                                  bool has_damage) override {
    if (!has_pending_tree_which_resumes_draws_) {
      return;
    }

    EXPECT_EQ(host_impl->pending_tree()->source_frame_number(), 1);

    constexpr size_t kNumOfFramesToDelayActivation = 5;
    if (++impl_frames_while_activation_delayed_ ==
        kNumOfFramesToDelayActivation) {
      has_pending_tree_which_resumes_draws_ = false;
      waiting_for_draw_after_rendering_resumes_ = true;
      host_impl->BlockNotifyReadyToActivateForTesting(false, true);
    }
  }

  void WillPrepareToDrawOnThread(LayerTreeHostImpl* host_impl) override {
    EXPECT_FALSE(has_pending_tree_which_resumes_draws_);

    if (waiting_for_draw_after_rendering_resumes_) {
      EXPECT_EQ(host_impl->active_tree()->source_frame_number(), 1);
      EndTest();
    }
  }

 private:
  // State accessed only on main thread.
  scoped_refptr<Layer> layer_;
  std::unique_ptr<ScopedPauseRendering> rendering_paused_;

  // State accessed only on impl thread.
  bool has_pending_tree_which_resumes_draws_ = false;
  bool waiting_for_draw_after_rendering_resumes_ = false;
  size_t impl_frames_while_activation_delayed_ = 0;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestPauseRendering);

}  // namespace
}  // namespace cc
