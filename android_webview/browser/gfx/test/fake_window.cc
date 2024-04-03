// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/test/fake_window.h"
#include "base/memory/raw_ptr.h"

#include "android_webview/browser/gfx/browser_view_renderer.h"
#include "android_webview/browser/gfx/child_frame.h"
#include "android_webview/browser/gfx/render_thread_manager.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

namespace android_webview {
namespace {
std::unique_ptr<base::Thread> g_render_thread;
}

class FakeWindow::ScopedMakeCurrent {
 public:
  ScopedMakeCurrent(FakeWindow* view_root) : view_root_(view_root) {
    DCHECK(!view_root_->context_current_);
    view_root_->context_current_ = true;
    bool result = view_root_->context_->MakeCurrent(view_root_->surface_.get());
    DCHECK(result);
  }

  ~ScopedMakeCurrent() {
    DCHECK(view_root_->context_current_);
    view_root_->context_current_ = false;

    // Release the underlying EGLContext. This is required because the real
    // GLContextEGL may no longer be current here and to satisfy DCHECK in
    // GLContextEGL::IsCurrent.
    eglMakeCurrent(view_root_->surface_->GetGLDisplay()->GetDisplay(),
                   EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    view_root_->context_->ReleaseCurrent(view_root_->surface_.get());
  }

 private:
  raw_ptr<FakeWindow> view_root_;
};

FakeWindow::FakeWindow(BrowserViewRenderer* view,
                       WindowHooks* hooks,
                       gfx::Rect location)
    : view_(view),
      hooks_(hooks),
      surface_size_(100, 100),
      location_(location),
      on_draw_hardware_pending_(false),
      context_current_(false) {
  CheckCurrentlyOnUIThread();
  DCHECK(view_);
  view_->OnAttachedToWindow(location_.width(), location_.height());
  view_->SetWindowVisibility(true);
  view_->SetViewVisibility(true);
}

FakeWindow::~FakeWindow() {
  CheckCurrentlyOnUIThread();
  if (render_thread_loop_) {
    base::WaitableEvent completion(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    render_thread_loop_->PostTask(
        FROM_HERE, base::BindOnce(&FakeWindow::DestroyOnRT,
                                  base::Unretained(this), &completion));
    completion.Wait();
  }
}

void FakeWindow::Detach() {
  CheckCurrentlyOnUIThread();
  view_->SetCurrentCompositorFrameConsumer(nullptr);
  view_->OnDetachedFromWindow();
}

void FakeWindow::RequestInvokeGL(FakeFunctor* functor,
                                 bool wait_for_completion) {
  CreateRenderThreadIfNeeded();
  CheckCurrentlyOnUIThread();
  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  render_thread_loop_->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeWindow::InvokeFunctorOnRT, base::Unretained(this),
                     functor, wait_for_completion ? &completion : nullptr));
  if (wait_for_completion)
    completion.Wait();
}

void FakeWindow::InvokeFunctorOnRT(FakeFunctor* functor,
                                   base::WaitableEvent* sync) {
  CheckCurrentlyOnRT();
  ScopedMakeCurrent make_current(this);
  functor->Invoke(hooks_);
  if (sync)
    sync->Signal();
}

void FakeWindow::RequestDrawGL(FakeFunctor* functor) {
  CheckCurrentlyOnUIThread();
  render_thread_loop_->PostTask(
      FROM_HERE, base::BindOnce(&FakeWindow::ProcessDrawOnRT,
                                base::Unretained(this), functor));
}

void FakeWindow::PostInvalidate() {
  CheckCurrentlyOnUIThread();
  if (on_draw_hardware_pending_)
    return;
  on_draw_hardware_pending_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FakeWindow::OnDrawHardware,
                                weak_ptr_factory_.GetWeakPtr()));
}

void FakeWindow::OnDrawHardware() {
  CheckCurrentlyOnUIThread();
  DCHECK(on_draw_hardware_pending_);
  on_draw_hardware_pending_ = false;

  view_->PrepareToDraw(gfx::Point(), location_);
  hooks_->WillOnDraw();
  bool success = view_->OnDrawHardware();
  hooks_->DidOnDraw(success);
  FakeFunctor* functor = hooks_->GetFunctor();
  if (success && functor) {
    CreateRenderThreadIfNeeded();

    base::WaitableEvent completion(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    render_thread_loop_->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeWindow::DrawFunctorOnRT, base::Unretained(this),
                       functor, &completion));
    completion.Wait();
  }
}

void FakeWindow::ProcessSyncOnRT(FakeFunctor* functor,
                                 base::WaitableEvent* sync) {
  CheckCurrentlyOnRT();
  functor->Sync(location_, hooks_);
  sync->Signal();
}

void FakeWindow::ProcessDrawOnRT(FakeFunctor* functor) {
  CheckCurrentlyOnRT();
  ScopedMakeCurrent make_current(this);
  functor->Draw(hooks_);
}

void FakeWindow::DrawFunctorOnRT(FakeFunctor* functor,
                                 base::WaitableEvent* sync) {
  ProcessSyncOnRT(functor, sync);
  ProcessDrawOnRT(functor);
}

void FakeWindow::CheckCurrentlyOnUIThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_checker_);
}

void FakeWindow::CreateRenderThreadIfNeeded() {
  CheckCurrentlyOnUIThread();
  if (render_thread_loop_) {
    DCHECK(g_render_thread);
    return;
  }

  if (!g_render_thread) {
    g_render_thread = std::make_unique<base::Thread>("TestRenderThread");
    g_render_thread->Start();
  }

  render_thread_loop_ = g_render_thread->task_runner();
  DETACH_FROM_SEQUENCE(rt_checker_);

  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  render_thread_loop_->PostTask(
      FROM_HERE, base::BindOnce(&FakeWindow::InitializeOnRT,
                                base::Unretained(this), &completion));
  completion.Wait();
}

void FakeWindow::InitializeOnRT(base::WaitableEvent* sync) {
  CheckCurrentlyOnRT();
  surface_ = gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplayEGL(),
                                                surface_size_);
  DCHECK(surface_);
  DCHECK(surface_->GetHandle());
  context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                       gl::GLContextAttribs());
  DCHECK(context_);
  sync->Signal();
}

void FakeWindow::DestroyOnRT(base::WaitableEvent* sync) {
  CheckCurrentlyOnRT();
  if (context_) {
    DCHECK(!context_->IsCurrent(surface_.get()));
    context_ = nullptr;
    surface_ = nullptr;
  }
  sync->Signal();
}

void FakeWindow::CheckCurrentlyOnRT() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(rt_checker_);
}

FakeFunctor::FakeFunctor() : window_(nullptr) {}

FakeFunctor::~FakeFunctor() {
  // Older tests delete functor without bothering to
  // call either release code path. Release thiings here.
  ReleaseOnUIWithInvoke();
}

void FakeFunctor::Init(
    FakeWindow* window,
    std::unique_ptr<RenderThreadManager> render_thread_manager) {
  window_ = window;
  render_thread_manager_ = std::move(render_thread_manager);
}

void FakeFunctor::Sync(const gfx::Rect& location,
                       WindowHooks* hooks) {
  DCHECK(render_thread_manager_);
  committed_location_ = location;
  hooks->WillSyncOnRT();
  render_thread_manager_->CommitFrameOnRT();
  hooks->DidSyncOnRT();
}

void FakeFunctor::Draw(WindowHooks* hooks) {
  DCHECK(render_thread_manager_);
  HardwareRendererDrawParams params{};
  params.clip_left = committed_location_.x();
  params.clip_top = committed_location_.y();
  params.clip_right = committed_location_.x() + committed_location_.width();
  params.clip_bottom = committed_location_.y() + committed_location_.height();
  params.width = committed_location_.width();
  params.height = committed_location_.height();
  if (!hooks->WillDrawOnRT(&params))
    return;
  render_thread_manager_->DrawOnRT(/*save_restore=*/false, params,
                                   OverlaysParams(),
                                   ReportRenderingThreadsCallback());
  hooks->DidDrawOnRT();
}

CompositorFrameConsumer* FakeFunctor::GetCompositorFrameConsumer() {
  return render_thread_manager_.get();
}

void FakeFunctor::ReleaseOnUIWithoutInvoke(base::OnceClosure callback) {
  DCHECK(render_thread_manager_);
  render_thread_manager_->RemoveFromCompositorFrameProducerOnUI();
  window_->render_thread_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FakeFunctor::ReleaseOnRT, base::Unretained(this),
          base::BindOnce(
              base::IgnoreResult(&base::SingleThreadTaskRunner::PostTask),
              base::SingleThreadTaskRunner::GetCurrentDefault(), FROM_HERE,
              std::move(callback))));
}

void FakeFunctor::ReleaseOnRT(base::OnceClosure callback) {
  DCHECK(render_thread_manager_);
  {
    RenderThreadManager::InsideHardwareReleaseReset release_reset(
        render_thread_manager_.get());
    render_thread_manager_->DestroyHardwareRendererOnRT(
        false /* save_restore */, false /* abandon_context */);
  }
  render_thread_manager_.reset();
  std::move(callback).Run();
}

void FakeFunctor::ReleaseOnUIWithInvoke() {
  if (!render_thread_manager_)
    return;
  render_thread_manager_->RemoveFromCompositorFrameProducerOnUI();
  {
    RenderThreadManager::InsideHardwareReleaseReset release_reset(
        render_thread_manager_.get());
    RequestInvokeGL(true);
  }
  render_thread_manager_.reset();
}

void FakeFunctor::Invoke(WindowHooks* hooks) {
  DCHECK(render_thread_manager_);
  hooks->WillProcessOnRT();
  bool abandon_context = true;  // For test coverage.
  render_thread_manager_->DestroyHardwareRendererOnRT(false /* save_restore */,
                                                      abandon_context);
  hooks->DidProcessOnRT();
}

bool FakeFunctor::RequestInvokeGL(bool wait_for_completion) {
  DCHECK(window_);
  window_->RequestInvokeGL(this, wait_for_completion);
  return true;
}

}  // namespace android_webview
