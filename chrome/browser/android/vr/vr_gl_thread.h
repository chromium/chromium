// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_VR_GL_THREAD_H_
#define CHROME_BROWSER_ANDROID_VR_VR_GL_THREAD_H_

#include <memory>

#include "base/android/java_handler_thread.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/android/vr/browser_renderer_factory.h"
#include "chrome/browser/android/vr/gl_browser_interface.h"
#include "chrome/browser/vr/browser_renderer_browser_interface.h"
#include "chrome/browser/vr/browser_ui_interface.h"
#include "chrome/browser/vr/ui_browser_interface.h"
#include "chrome/browser/vr/ui_test_input.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"

namespace base {
class WaitableEvent;
}  // namespace base

namespace gvr {
class GvrApi;
}

namespace vr {

class VrShell;

class VrGLThread : public base::android::JavaHandlerThread,
                   public BrowserRendererBrowserInterface,
                   public GlBrowserInterface,
                   public UiBrowserInterface,
                   public BrowserUiInterface {
 public:
  VrGLThread(
      const base::WeakPtr<VrShell>& weak_vr_shell,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
      gvr_context* gvr_api,
      const UiInitialState& ui_initial_state,
      bool reprojected_rendering,
      bool daydream_support,
      base::WaitableEvent* gl_surface_created_event,
      base::OnceCallback<gfx::AcceleratedWidget()> surface_callback);

  VrGLThread(const VrGLThread&) = delete;
  VrGLThread& operator=(const VrGLThread&) = delete;

  ~VrGLThread() override;
  base::WeakPtr<BrowserRenderer> GetBrowserRenderer();

  // GlBrowserInterface implementation (GL calling to VrShell).
  void GvrDelegateReady() override;
  void SendRequestPresentReply(device::mojom::XRSessionPtr) override;
  void ToggleCardboardGamepad(bool enabled) override;

  // BrowserRendererBrowserInterface implementation (BrowserRenderer calling to
  // VrShell).
  void ForceExitVr() override;
  void ReportUiOperationResultForTesting(
      const UiTestOperationType& action_type,
      const UiTestOperationResult& result) override;

  // UiBrowserInterface implementation (UI calling to VrShell).
  void ExitPresent() override;

  // BrowserUiInterface implementation (Browser calling to UI).
  void SetCapturingState(
      const CapturingStateModel& active_capturing,
      const CapturingStateModel& background_capturing,
      const CapturingStateModel& potential_capturing) override;
  void SetVisibleExternalPromptNotification(
      ExternalPromptNotificationType prompt) override;

 protected:
  void Init() override;
  void CleanUp() override;

 private:
  bool OnMainThread() const;
  bool OnGlThread() const;

  base::WeakPtr<VrShell> weak_vr_shell_;
  base::WeakPtr<BrowserUiInterface> weak_browser_ui_;

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  // Created on GL thread.
  std::unique_ptr<UiFactory> ui_factory_;
  std::unique_ptr<BrowserRenderer> browser_renderer_;
  std::unique_ptr<gvr::GvrApi> gvr_api_;

  // This state is used for initializing the BrowserRenderer.
  std::unique_ptr<BrowserRendererFactory::Params> factory_params_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_VR_GL_THREAD_H_
