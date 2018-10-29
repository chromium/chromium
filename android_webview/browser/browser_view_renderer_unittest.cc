// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <queue>
#include <utility>

#include "android_webview/browser/browser_view_renderer.h"
#include "android_webview/browser/child_frame.h"
#include "android_webview/browser/compositor_frame_consumer.h"
#include "android_webview/browser/render_thread_manager.h"
#include "android_webview/browser/test/rendering_test.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "content/public/test/test_synchronous_compositor_android.h"

namespace android_webview {

class SmokeTest : public RenderingTest {
  void StartTest() override {
    browser_view_renderer_->PostInvalidate(ActiveCompositor());
  }

  void DidDrawOnRT() override { EndTest(); }
};

RENDERING_TEST_F(SmokeTest);

// Test the case where SynchronousCompositor is constructed after the RVH that
// owns it is switched to be active.
class ActiveCompositorSwitchBeforeConstructionTest : public RenderingTest {
 public:
  ActiveCompositorSwitchBeforeConstructionTest()
      : on_draw_count_(0), new_compositor_(nullptr) {}
  void StartTest() override {
    browser_view_renderer_->PostInvalidate(ActiveCompositor());
  }

  void DidOnDraw(bool success) override {
    on_draw_count_++;
    switch (on_draw_count_) {
      case 1:
        EXPECT_TRUE(success);
        // Change compositor here. And do another ondraw.
        // The previous active compositor id is 0, 0, now change it to 0, 1.
        browser_view_renderer_->SetActiveCompositorID(CompositorID(0, 1));
        browser_view_renderer_->PostInvalidate(ActiveCompositor());
        break;
      case 2:
        // The 2nd ondraw is skipped because there is no active compositor at
        // the moment.
        EXPECT_FALSE(success);
        new_compositor_.reset(new content::TestSynchronousCompositor(0, 1));
        new_compositor_->SetClient(browser_view_renderer_.get());
        EXPECT_EQ(ActiveCompositor(), new_compositor_.get());
        browser_view_renderer_->PostInvalidate(ActiveCompositor());
        break;
      case 3:
        EXPECT_TRUE(success);
        compositor_ = std::move(new_compositor_);

        EXPECT_EQ(ActiveCompositor(), compositor_.get());
        browser_view_renderer_->PostInvalidate(ActiveCompositor());
        break;
      case 4:
        EXPECT_TRUE(success);
        EndTest();
    }
  }

 private:
  int on_draw_count_;
  std::unique_ptr<content::TestSynchronousCompositor> new_compositor_;
};

RENDERING_TEST_F(ActiveCompositorSwitchBeforeConstructionTest);

// Test the case where SynchronousCompositor is constructed before the RVH that
// owns it is switched to be active.
class ActiveCompositorSwitchAfterConstructionTest : public RenderingTest {
 public:
  ActiveCompositorSwitchAfterConstructionTest()
      : on_draw_count_(0), new_compositor_(nullptr) {}
  void StartTest() override {
    browser_view_renderer_->PostInvalidate(ActiveCompositor());
  }

  void DidOnDraw(bool success) override {
    on_draw_count_++;
    switch (on_draw_count_) {
      case 1:
        EXPECT_TRUE(success);
        // Create a new compositor here. And switch it to be active.  And then
        // do another ondraw.
        new_compositor_.reset(new content::TestSynchronousCompositor(0, 1));
        new_compositor_->SetClient(browser_view_renderer_.get());
        browser_view_renderer_->SetActiveCompositorID(CompositorID(0, 1));

        EXPECT_EQ(ActiveCompositor(), new_compositor_.get());
        browser_view_renderer_->PostInvalidate(ActiveCompositor());
        break;
      case 2:
        EXPECT_TRUE(success);
        compositor_ = std::move(new_compositor_);

        EXPECT_EQ(ActiveCompositor(), compositor_.get());
        browser_view_renderer_->PostInvalidate(ActiveCompositor());
        break;
      case 3:
        EXPECT_TRUE(success);
        EndTest();
    }
  }

 private:
  int on_draw_count_;
  std::unique_ptr<content::TestSynchronousCompositor> new_compositor_;
};

RENDERING_TEST_F(ActiveCompositorSwitchAfterConstructionTest);

class ClearViewTest : public RenderingTest {
 public:
  ClearViewTest() : on_draw_count_(0) {}

  void StartTest() override {
    browser_view_renderer_->PostInvalidate(ActiveCompositor());
    browser_view_renderer_->ClearView();
  }

  void DidOnDraw(bool success) override {
    on_draw_count_++;
    if (on_draw_count_ == 1) {
      // First OnDraw should be skipped due to ClearView.
      EXPECT_FALSE(success);
      browser_view_renderer_->DidUpdateContent(
          ActiveCompositor());  // Unset ClearView.
      browser_view_renderer_->PostInvalidate(ActiveCompositor());
    } else {
      // Following OnDraws should succeed.
      EXPECT_TRUE(success);
    }
  }

  void DidDrawOnRT() override { EndTest(); }

 private:
  int on_draw_count_;
};

RENDERING_TEST_F(ClearViewTest);

class TestAnimateInAndOutOfScreen : public RenderingTest {
 public:
  TestAnimateInAndOutOfScreen() : on_draw_count_(0), draw_gl_count_on_rt_(0) {}

  void StartTest() override {
    new_constraints_ = ParentCompositorDrawConstraints(
        false, gfx::Transform(), window_->surface_size().IsEmpty());
    new_constraints_.transform.Scale(2.0, 2.0);
    browser_view_renderer_->PostInvalidate(ActiveCompositor());
  }

  void WillOnDraw() override {
    RenderingTest::WillOnDraw();
    // Step 0: A single onDraw on screen. The parent draw constraints
    // of the BVR will updated to be the initial constraints.
    // Step 1: A single onDrraw off screen. The parent draw constraints of the
    // BVR will be updated to the new constraints.
    // Step 2: This onDraw is to introduce the DrawGL that animates the
    // webview onto the screen on render thread. End the test when the parent
    // draw constraints of BVR is updated to initial constraints.
    if (on_draw_count_ == 1 || on_draw_count_ == 2)
      browser_view_renderer_->PrepareToDraw(gfx::Vector2d(), gfx::Rect());
  }

  void DidOnDraw(bool success) override {
    EXPECT_TRUE(success);
    on_draw_count_++;
  }

  bool WillDrawOnRT(AwDrawGLInfo* draw_info) override {
    if (draw_gl_count_on_rt_ == 1) {
      draw_gl_count_on_rt_++;
      ui_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(&RenderingTest::PostInvalidate,
                                               base::Unretained(this)));
      return false;
    }

    draw_info->width = window_->surface_size().width();
    draw_info->height = window_->surface_size().height();
    draw_info->is_layer = false;

    gfx::Transform transform;
    if (draw_gl_count_on_rt_ == 0)
      transform = new_constraints_.transform;

    transform.matrix().asColMajorf(draw_info->transform);
    return true;
  }

  void DidDrawOnRT() override { draw_gl_count_on_rt_++; }

  bool DrawConstraintsEquals(
      const ParentCompositorDrawConstraints& constraints1,
      const ParentCompositorDrawConstraints& constraints2) {
    if (constraints1.is_layer != constraints2.is_layer ||
        constraints1.transform != constraints2.transform)
      return false;

    return !constraints1.is_layer ||
           constraints1.surface_rect_empty == constraints2.surface_rect_empty;
  }

  void OnParentDrawConstraintsUpdated() override {
    ParentCompositorDrawConstraints constraints =
        GetCompositorFrameConsumer()->GetParentDrawConstraintsOnUI();
    switch (on_draw_count_) {
      case 0u:
        // This OnParentDrawConstraintsUpdated is generated by
        // connecting the compositor frame consumer to the producer.
        break;
      case 1u:
        EXPECT_TRUE(DrawConstraintsEquals(constraints, new_constraints_));
        break;
      case 3u:
        EXPECT_TRUE(DrawConstraintsEquals(constraints, initial_constraints_));
        EndTest();
        break;
      // There will be a following 4th onDraw. But the hardware renderer won't
      // post back the draw constraints in DrawGL because the constraints
      // don't change.
      default:
        FAIL();
    }
  }

 private:
  int on_draw_count_;
  int draw_gl_count_on_rt_;
  ParentCompositorDrawConstraints initial_constraints_;
  ParentCompositorDrawConstraints new_constraints_;
};

RENDERING_TEST_F(TestAnimateInAndOutOfScreen);

class CompositorNoFrameTest : public RenderingTest {
 public:
  CompositorNoFrameTest() : on_draw_count_(0) {}

  void StartTest() override {
    browser_view_renderer_->PostInvalidate(ActiveCompositor());
  }

  void WillOnDraw() override {
    if (0 == on_draw_count_) {
      // No frame from compositor.
    } else if (1 == on_draw_count_) {
      compositor_->SetHardwareFrame(0u, ConstructEmptyFrame());
    } else if (2 == on_draw_count_) {
      // No frame from compositor.
    }
    // There may be trailing invalidates.
  }

  void DidOnDraw(bool success) override {
    // OnDraw should succeed even when there are no frames from compositor.
    EXPECT_TRUE(success);
    if (0 == on_draw_count_) {
      browser_view_renderer_->PostInvalidate(ActiveCompositor());
    } else if (1 == on_draw_count_) {
      browser_view_renderer_->PostInvalidate(ActiveCompositor());
    } else if (2 == on_draw_count_) {
      EndTest();
    }
    on_draw_count_++;
  }

 private:
  int on_draw_count_;
};

RENDERING_TEST_F(CompositorNoFrameTest);

class ClientIsVisibleOnConstructionTest : public RenderingTest {
  void SetUpTestHarness() override {
    browser_view_renderer_.reset(
        new BrowserViewRenderer(this, base::ThreadTaskRunnerHandle::Get()));
  }

  void StartTest() override {
    EXPECT_FALSE(browser_view_renderer_->attached_to_window());
    EXPECT_FALSE(browser_view_renderer_->window_visible_for_tests());
    EXPECT_TRUE(browser_view_renderer_->IsClientVisible());
    EndTest();
  }
};

RENDERING_TEST_F(ClientIsVisibleOnConstructionTest);

class ClientIsVisibleAfterAttachTest : public RenderingTest {
  void StartTest() override {
    EXPECT_TRUE(browser_view_renderer_->attached_to_window());
    EXPECT_TRUE(browser_view_renderer_->window_visible_for_tests());

    EXPECT_TRUE(browser_view_renderer_->IsClientVisible());
    EndTest();
  }
};

RENDERING_TEST_F(ClientIsVisibleAfterAttachTest);

class ClientIsInvisibleAfterWindowGoneTest : public RenderingTest {
  void StartTest() override {
    browser_view_renderer_->SetWindowVisibility(false);
    EXPECT_FALSE(browser_view_renderer_->IsClientVisible());
    EndTest();
  }
};

RENDERING_TEST_F(ClientIsInvisibleAfterWindowGoneTest);

class ClientIsInvisibleAfterDetachTest : public RenderingTest {
  void StartTest() override {
    browser_view_renderer_->OnDetachedFromWindow();
    EXPECT_FALSE(browser_view_renderer_->IsClientVisible());
    EndTest();
  }
};

RENDERING_TEST_F(ClientIsInvisibleAfterDetachTest);

class ResourceRenderingTest : public RenderingTest {
 public:
  using ResourceCountMap = std::map<viz::ResourceId, int>;
  using LayerTreeFrameSinkResourceCountMap =
      std::map<uint32_t, ResourceCountMap>;

  virtual std::unique_ptr<content::SynchronousCompositor::Frame> GetFrame(
      int frame_number) = 0;

  void StartTest() override {
    frame_number_ = 0;
    AdvanceFrame();
  }

  void WillOnDraw() override {
    if (next_frame_) {
      compositor_->SetHardwareFrame(next_frame_->layer_tree_frame_sink_id,
                                    std::move(next_frame_->frame));
    }
  }

  void DidOnDraw(bool success) override {
    EXPECT_EQ(next_frame_ != nullptr, success);
    if (!AdvanceFrame()) {
      ui_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&ResourceRenderingTest::CheckResults,
                                    base::Unretained(this)));
    }
  }

  LayerTreeFrameSinkResourceCountMap GetReturnedResourceCounts() {
    LayerTreeFrameSinkResourceCountMap counts;
    content::TestSynchronousCompositor::FrameAckArray returned_resources_array;
    compositor_->SwapReturnedResources(&returned_resources_array);
    for (const auto& resources : returned_resources_array) {
      for (const auto& returned_resource : resources.resources) {
        counts[resources.layer_tree_frame_sink_id][returned_resource.id] +=
            returned_resource.count;
      }
    }
    return counts;
  }

  virtual void CheckResults() = 0;

 private:
  bool AdvanceFrame() {
    next_frame_ = GetFrame(frame_number_++);
    if (next_frame_) {
      browser_view_renderer_->PostInvalidate(ActiveCompositor());
      return true;
    }
    return false;
  }

  std::unique_ptr<content::SynchronousCompositor::Frame> next_frame_;
  int frame_number_;
};

class SwitchLayerTreeFrameSinkIdTest : public ResourceRenderingTest {
  struct FrameInfo {
    uint32_t layer_tree_frame_sink_id;
    viz::ResourceId resource_id;  // Each frame contains a single resource.
  };

  std::unique_ptr<content::SynchronousCompositor::Frame> GetFrame(
      int frame_number) override {
    static const FrameInfo infos[] = {
        // First output surface.
        {0u, 1u}, {0u, 1u}, {0u, 2u}, {0u, 2u}, {0u, 3u}, {0u, 3u}, {0u, 4u},
        // Second output surface.
        {1u, 1u}, {1u, 1u}, {1u, 2u}, {1u, 2u}, {1u, 3u}, {1u, 3u}, {1u, 4u},
    };
    if (frame_number >= static_cast<int>(base::size(infos))) {
      return nullptr;
    }

    std::unique_ptr<content::SynchronousCompositor::Frame> frame(
        new content::SynchronousCompositor::Frame);
    frame->layer_tree_frame_sink_id =
        infos[frame_number].layer_tree_frame_sink_id;
    frame->frame = ConstructFrame(infos[frame_number].resource_id);

    if (last_layer_tree_frame_sink_id_ !=
        infos[frame_number].layer_tree_frame_sink_id) {
      expected_return_count_.clear();
      last_layer_tree_frame_sink_id_ =
          infos[frame_number].layer_tree_frame_sink_id;
    }
    ++expected_return_count_[infos[frame_number].resource_id];
    return frame;
  }

  void StartTest() override {
    last_layer_tree_frame_sink_id_ = -1U;
    ResourceRenderingTest::StartTest();
  }

  void CheckResults() override {
    GetCompositorFrameConsumer()->DeleteHardwareRendererOnUI();
    window_->Detach();
    window_.reset();

    // Make sure resources for the last output surface are returned.
    EXPECT_EQ(expected_return_count_,
              GetReturnedResourceCounts()[last_layer_tree_frame_sink_id_]);
    EndTest();
  }

 private:
  uint32_t last_layer_tree_frame_sink_id_;
  ResourceCountMap expected_return_count_;
};

RENDERING_TEST_F(SwitchLayerTreeFrameSinkIdTest);

class RenderThreadManagerDeletionTest : public ResourceRenderingTest {
  std::unique_ptr<content::SynchronousCompositor::Frame> GetFrame(
      int frame_number) override {
    if (frame_number > 0) {
      return nullptr;
    }

    const uint32_t layer_tree_frame_sink_id = 0u;
    const viz::ResourceId resource_id =
        static_cast<viz::ResourceId>(frame_number);

    std::unique_ptr<content::SynchronousCompositor::Frame> frame(
        new content::SynchronousCompositor::Frame);
    frame->layer_tree_frame_sink_id = layer_tree_frame_sink_id;
    frame->frame = ConstructFrame(resource_id);
    ++expected_return_count_[layer_tree_frame_sink_id][resource_id];
    return frame;
  }

  void CheckResults() override {
    LayerTreeFrameSinkResourceCountMap resource_counts;
    functor_.reset();
    // Make sure resources for the last frame are returned.
    EXPECT_EQ(expected_return_count_, GetReturnedResourceCounts());
    EndTest();
  }

 private:
  LayerTreeFrameSinkResourceCountMap expected_return_count_;
};

RENDERING_TEST_F(RenderThreadManagerDeletionTest);

class RenderThreadManagerSwitchTest : public ResourceRenderingTest {
  std::unique_ptr<content::SynchronousCompositor::Frame> GetFrame(
      int frame_number) override {
    switch (frame_number) {
      case 0: {
        // Draw a frame with initial RTM.
        break;
      }
      case 1: {
        // Switch to new RTM.
        std::unique_ptr<FakeFunctor> functor(new FakeFunctor);
        functor->Init(window_.get(),
                      std::make_unique<RenderThreadManager>(
                          functor.get(), base::ThreadTaskRunnerHandle::Get()));
        browser_view_renderer_->SetCurrentCompositorFrameConsumer(
            functor->GetCompositorFrameConsumer());
        saved_functor_ = std::move(functor_);
        functor_ = std::move(functor);
        break;
      }
      case 2: {
        // Draw a frame with the new RTM, but also redraw the initial RTM.
        window_->RequestDrawGL(saved_functor_.get());
        break;
      }
      case 3: {
        // Switch back to the initial RTM, allowing the new RTM to be destroyed.
        functor_ = std::move(saved_functor_);
        browser_view_renderer_->SetCurrentCompositorFrameConsumer(
            functor_->GetCompositorFrameConsumer());
        break;
      }
      default:
        return nullptr;
    }

    const uint32_t layer_tree_frame_sink_id = 0u;
    const viz::ResourceId resource_id =
        static_cast<viz::ResourceId>(frame_number);

    std::unique_ptr<content::SynchronousCompositor::Frame> frame(
        new content::SynchronousCompositor::Frame);
    frame->layer_tree_frame_sink_id = layer_tree_frame_sink_id;
    frame->frame = ConstructFrame(resource_id);
    ++expected_return_count_[layer_tree_frame_sink_id][resource_id];
    return frame;
  }

  void CheckResults() override {
    LayerTreeFrameSinkResourceCountMap resource_counts;
    functor_.reset();
    // Make sure resources for all frames are returned.
    EXPECT_EQ(expected_return_count_, GetReturnedResourceCounts());
    EndTest();
  }

 private:
  std::unique_ptr<FakeFunctor> saved_functor_;
  LayerTreeFrameSinkResourceCountMap expected_return_count_;
};

RENDERING_TEST_F(RenderThreadManagerSwitchTest);

// Test for https://crbug.com/881458, this test is to make sure we will reach
// the maximal scroll offset.
class DidReachMaximalScrollOffsetTest : public RenderingTest {
 public:
  void StartTest() override {
    browser_view_renderer_->SetDipScale(kDipScale);
    gfx::Vector2dF total_scroll_offset = kTotalScrollOffset;
    gfx::Vector2dF total_max_scroll_offset = kTotalMaxScrollOffset;
    gfx::SizeF scrollable_size = kScrollableSize;
    // When --use-zoom-for-dsf is enabled, these value are in physical pixel.
    if (content::IsUseZoomForDSFEnabled()) {
      total_scroll_offset.Scale(kDipScale);
      total_max_scroll_offset.Scale(kDipScale);
      scrollable_size.Scale(kDipScale);
    }
    // |UpdateRootLayerState()| will call |SetTotalRootLayerScrollOffset()|.
    browser_view_renderer_->UpdateRootLayerState(
        ActiveCompositor(), total_scroll_offset, total_max_scroll_offset,
        scrollable_size, kPageScaleFactor, kMinPageScaleFactor,
        kMaxPageScaleFactor);
  }

  void ScrollContainerViewTo(const gfx::Vector2d& new_value) override {
    EXPECT_EQ(kExpectedScrollOffset.ToString(), new_value.ToString());
    EndTest();
  }

 private:
  static constexpr float kDipScale = 2.625f;
  static const gfx::Vector2dF kTotalScrollOffset;
  static const gfx::Vector2dF kTotalMaxScrollOffset;
  static const gfx::SizeF kScrollableSize;
  static constexpr float kPageScaleFactor = 1.f;
  // These two are not used in this test.
  static constexpr float kMinPageScaleFactor = 1.f;
  static constexpr float kMaxPageScaleFactor = 5.f;

  static const gfx::Vector2d kExpectedScrollOffset;
};

// The current scroll offset in logical pixel, which is at the end.
const gfx::Vector2dF DidReachMaximalScrollOffsetTest::kTotalScrollOffset =
    gfx::Vector2dF(0.f, 6132.f);
// The maximum possible scroll offset in logical pixel.
const gfx::Vector2dF DidReachMaximalScrollOffsetTest::kTotalMaxScrollOffset =
    gfx::Vector2dF(0.f, 6132.f);
// This is what passed to CTS test, not used for this test.
const gfx::SizeF DidReachMaximalScrollOffsetTest::kScrollableSize =
    gfx::SizeF(412.f, 6712.f);
// In max_scroll_offset() we are using ceiling rounding for scaled scroll
// offset. Therefore ceiling(2.625 * 6132 = 16096.5) = 16097.
const gfx::Vector2d DidReachMaximalScrollOffsetTest::kExpectedScrollOffset =
    gfx::Vector2d(0, 16097);

RENDERING_TEST_F(DidReachMaximalScrollOffsetTest);

}  // namespace android_webview
