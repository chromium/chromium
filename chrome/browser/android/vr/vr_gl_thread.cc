// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/vr_gl_thread.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/android/vr/metrics_util_android.h"
#include "chrome/browser/android/vr/ui_factory.h"
#include "chrome/browser/android/vr/vr_shell.h"
#include "chrome/browser/vr/browser_renderer.h"
#include "chrome/browser/vr/browser_ui_interface.h"
#include "chrome/browser/vr/ui_test_input.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gl/android/surface_texture.h"

namespace vr {

VrGLThread::VrGLThread(
    const base::WeakPtr<VrShell>& weak_vr_shell,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    gvr_context* gvr_api,
    const UiInitialState& ui_initial_state,
    bool reprojected_rendering,
    bool daydream_support,
    base::WaitableEvent* gl_surface_created_event,
    base::OnceCallback<gfx::AcceleratedWidget()> surface_callback)
    : base::android::JavaHandlerThread("VrShellGL"),
      weak_vr_shell_(weak_vr_shell),
      main_thread_task_runner_(std::move(main_thread_task_runner)),
      gvr_api_(gvr::GvrApi::WrapNonOwned(gvr_api)),
      factory_params_(std::make_unique<BrowserRendererFactory::Params>(
          gvr_api_.get(),
          ui_initial_state,
          reprojected_rendering,
          gvr_api_->GetViewerType() ==
              gvr::ViewerType::GVR_VIEWER_TYPE_CARDBOARD,
          gl_surface_created_event,
          std::move(surface_callback))) {
  MetricsUtilAndroid::LogVrViewerType(gvr_api_->GetViewerType());
}

VrGLThread::~VrGLThread() {
  Stop();
}

base::WeakPtr<BrowserRenderer> VrGLThread::GetBrowserRenderer() {
  return browser_renderer_->GetWeakPtr();
}

void VrGLThread::Init() {
  ui_factory_ = CreateUiFactory();
  browser_renderer_ = BrowserRendererFactory::Create(
      this, ui_factory_.get(), std::move(factory_params_));
  weak_browser_ui_ = browser_renderer_->GetBrowserUiWeakPtr();
}

void VrGLThread::CleanUp() {
  browser_renderer_.reset();
}

void VrGLThread::GvrDelegateReady() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::GvrDelegateReady, weak_vr_shell_));
}

void VrGLThread::SendRequestPresentReply(device::mojom::XRSessionPtr session) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::SendRequestPresentReply,
                                weak_vr_shell_, std::move(session)));
}

void VrGLThread::ForceExitVr() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::ForceExitVr, weak_vr_shell_));
}

void VrGLThread::ExitPresent() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::ExitPresent, weak_vr_shell_));
  browser_renderer_->OnExitPresent();
}

void VrGLThread::ToggleCardboardGamepad(bool enabled) {}

void VrGLThread::SetCapturingState(
    const CapturingStateModel& active_capturing,
    const CapturingStateModel& background_capturing,
    const CapturingStateModel& potential_capturing) {}

void VrGLThread::SetVisibleExternalPromptNotification(
    ExternalPromptNotificationType prompt) {
  // Not reached on Android.
  NOTREACHED();
}

void VrGLThread::ReportUiOperationResultForTesting(
    const UiTestOperationType& action_type,
    const UiTestOperationResult& result) {}

bool VrGLThread::OnMainThread() const {
  return main_thread_task_runner_->BelongsToCurrentThread();
}

bool VrGLThread::OnGlThread() const {
  return task_runner()->BelongsToCurrentThread();
}

}  // namespace vr
