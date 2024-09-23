// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_TEST_RENDERING_TEST_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_TEST_RENDERING_TEST_H_

#include <memory>

#include "android_webview/browser/gfx/browser_view_renderer_client.h"
#include "android_webview/browser/gfx/test/fake_window.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "components/viz/common/resources/resource_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace test {
class TaskEnvironment;
}
}  // namespace base

namespace content {
class SynchronousCompositor;
class TestSynchronousCompositor;
}  // namespace content

namespace ui {
class TouchHandleDrawable;
}

namespace viz {
class CompositorFrame;
}

namespace android_webview {

class BrowserViewRenderer;
class CompositorFrameConsumer;
class CompositorFrameProducer;
class FakeWindow;

class RenderingTest : public testing::Test,
                      public BrowserViewRendererClient,
                      public WindowHooks {
 public:
  RenderingTest(const RenderingTest&) = delete;
  RenderingTest& operator=(const RenderingTest&) = delete;

  // BrowserViewRendererClient overrides.
  void OnNewPicture() override;
  void PostInvalidate(bool inside_vsync) override;
  gfx::Point GetLocationOnScreen() override;
  void ScrollContainerViewTo(const gfx::Point& new_value) override {}
  void UpdateScrollState(const gfx::Point& max_scroll_offset,
                         const gfx::SizeF& contents_size_dip,
                         float page_scale_factor,
                         float min_page_scale_factor,
                         float max_page_scale_factor) override {}
  void DidOverscroll(const gfx::Vector2d& overscroll_delta,
                     const gfx::Vector2dF& overscroll_velocity,
                     bool inside_vsync) override {}
  ui::TouchHandleDrawable* CreateDrawable() override;

  // WindowHooks overrides.
  void WillOnDraw() override;
  void DidOnDraw(bool success) override {}
  FakeFunctor* GetFunctor() override;
  void WillSyncOnRT() override {}
  void DidSyncOnRT() override {}
  void WillProcessOnRT() override {}
  void DidProcessOnRT() override {}
  bool WillDrawOnRT(HardwareRendererDrawParams* params) override;
  void DidDrawOnRT() override {}

  virtual void OnParentDrawDataUpdated() {}
  void OnViewTreeForceDarkStateChanged(
      bool view_tree_force_dark_state) override {}
  void SetPreferredFrameInterval(
      base::TimeDelta preferred_frame_interval) override {}

 protected:
  RenderingTest();
  ~RenderingTest() override;

  CompositorFrameConsumer* GetCompositorFrameConsumer();
  CompositorFrameProducer* GetCompositorFrameProducer();

  virtual void SetUpTestHarness();
  virtual void StartTest();

  void RunTest();
  void InitializeCompositor();
  void EndTest();
  content::SynchronousCompositor* ActiveCompositor() const;
  std::unique_ptr<viz::CompositorFrame> ConstructEmptyFrame();
  std::unique_ptr<viz::CompositorFrame> ConstructFrame(
      viz::ResourceId resource_id);
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  std::unique_ptr<FakeWindow> window_;
  std::unique_ptr<FakeFunctor> functor_;
  std::unique_ptr<BrowserViewRenderer> browser_view_renderer_;
  std::unique_ptr<content::TestSynchronousCompositor> compositor_;

 private:
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
  base::RunLoop run_loop_;
};

#define RENDERING_TEST_F(TEST_FIXTURE_NAME)         \
  TEST_F(TEST_FIXTURE_NAME, RunTest) { RunTest(); } \
  class NeedsSemicolon##TEST_FIXTURE_NAME {}

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_TEST_RENDERING_TEST_H_
