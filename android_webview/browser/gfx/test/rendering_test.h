// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_TEST_RENDERING_TEST_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_TEST_RENDERING_TEST_H_

#include <memory>

#include "android_webview/browser/gfx/browser_view_renderer_client.h"
#include "android_webview/browser/gfx/test/fake_window.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "components/viz/common/resources/resource_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class MessageLoop;
}

namespace content {
class SynchronousCompositor;
class TestSynchronousCompositor;
}

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
  // BrowserViewRendererClient overrides.
  void OnNewPicture() override;
  void PostInvalidate() override;
  gfx::Point GetLocationOnScreen() override;
  void ScrollContainerViewTo(const gfx::Vector2d& new_value) override {}
  void UpdateScrollState(const gfx::Vector2d& max_scroll_offset,
                         const gfx::SizeF& contents_size_dip,
                         float page_scale_factor,
                         float min_page_scale_factor,
                         float max_page_scale_factor) override {}
  void DidOverscroll(const gfx::Vector2d& overscroll_delta,
                     const gfx::Vector2dF& overscroll_velocity) override {}
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
  const std::unique_ptr<base::MessageLoop> message_loop_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(RenderingTest);
};

#define RENDERING_TEST_F(TEST_FIXTURE_NAME)         \
  TEST_F(TEST_FIXTURE_NAME, RunTest) { RunTest(); } \
  class NeedsSemicolon##TEST_FIXTURE_NAME {}

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_TEST_RENDERING_TEST_H_
