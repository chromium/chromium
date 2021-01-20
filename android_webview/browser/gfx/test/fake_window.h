// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_TEST_FAKE_WINDOW_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_TEST_FAKE_WINDOW_H_

#include <map>

#include "android_webview/browser/gfx/hardware_renderer.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

namespace base {
class Thread;
class WaitableEvent;
}

namespace android_webview {

class BrowserViewRenderer;
class CompositorFrameConsumer;
class FakeFunctor;
class RenderThreadManager;

class WindowHooks {
 public:
  virtual ~WindowHooks() {}

  virtual void WillOnDraw() = 0;
  virtual void DidOnDraw(bool success) = 0;
  virtual FakeFunctor* GetFunctor() = 0;

  virtual void WillSyncOnRT() = 0;
  virtual void DidSyncOnRT() = 0;
  virtual void WillProcessOnRT() = 0;
  virtual void DidProcessOnRT() = 0;
  virtual bool WillDrawOnRT(HardwareRendererDrawParams* params) = 0;
  virtual void DidDrawOnRT() = 0;
};

class FakeWindow {
 public:
  FakeWindow(BrowserViewRenderer* view, WindowHooks* hooks, gfx::Rect location);
  ~FakeWindow();

  void Detach();

  void RequestInvokeGL(FakeFunctor* functor, bool wait_for_completion);
  void PostInvalidate();
  const gfx::Size& surface_size() { return surface_size_; }

  void RequestDrawGL(FakeFunctor* functor);

  bool on_draw_hardware_pending() const { return on_draw_hardware_pending_; }
  scoped_refptr<base::SingleThreadTaskRunner> render_thread_task_runner() {
    return render_thread_loop_;
  }

 private:
  class ScopedMakeCurrent;

  void OnDrawHardware();
  void CheckCurrentlyOnUIThread();
  void CreateRenderThreadIfNeeded();

  void InitializeOnRT(base::WaitableEvent* sync);
  void DestroyOnRT(base::WaitableEvent* sync);
  void InvokeFunctorOnRT(FakeFunctor* functor, base::WaitableEvent* sync);
  void ProcessSyncOnRT(FakeFunctor* functor, base::WaitableEvent* sync);
  void ProcessDrawOnRT(FakeFunctor* functor);
  void DrawFunctorOnRT(FakeFunctor* functor, base::WaitableEvent* sync);
  void CheckCurrentlyOnRT();

  // const so can be used on both threads.
  BrowserViewRenderer* const view_;
  WindowHooks* const hooks_;
  const gfx::Size surface_size_;

  // UI thread members.
  gfx::Rect location_;
  bool on_draw_hardware_pending_;
  base::SequenceChecker ui_checker_;

  // Render thread members.
  std::unique_ptr<base::Thread> render_thread_;
  base::SequenceChecker rt_checker_;
  scoped_refptr<base::SingleThreadTaskRunner> render_thread_loop_;
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  bool context_current_;

  base::WeakPtrFactory<FakeWindow> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeWindow);
};

class FakeFunctor {
 public:
  FakeFunctor();
  ~FakeFunctor();

  void Init(FakeWindow* window,
            std::unique_ptr<RenderThreadManager> render_thread_manager);
  void Sync(const gfx::Rect& location, WindowHooks* hooks);
  void Draw(WindowHooks* hooks);
  void Invoke(WindowHooks* hooks);

  CompositorFrameConsumer* GetCompositorFrameConsumer();
  void ReleaseOnUIWithInvoke();

  void ReleaseOnUIWithoutInvoke(base::OnceClosure callback);

 private:
  bool RequestInvokeGL(bool wait_for_completion);
  void ReleaseOnRT(base::OnceClosure callback);

  FakeWindow* window_;
  std::unique_ptr<RenderThreadManager> render_thread_manager_;
  gfx::Rect committed_location_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_TEST_FAKE_WINDOW_H_
