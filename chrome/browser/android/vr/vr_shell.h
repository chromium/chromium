// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_VR_SHELL_H_
#define CHROME_BROWSER_ANDROID_VR_VR_SHELL_H_

#include <jni.h>

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/cancelable_callback.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/toolbar/chrome_location_bar_model_delegate.h"
#include "chrome/browser/vr/assets_load_status.h"
#include "chrome/browser/vr/exit_vr_prompt_choice.h"
#include "chrome/browser/vr/model/capturing_state_model.h"
#include "chrome/browser/vr/platform_ui_input_delegate.h"
#include "chrome/browser/vr/speech_recognizer.h"
#include "chrome/browser/vr/ui_browser_interface.h"
#include "chrome/browser/vr/ui_initial_state.h"
#include "chrome/browser/vr/ui_unsupported_mode.h"
#include "components/page_info/page_info_ui.h"
#include "content/public/browser/web_contents_observer.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom-forward.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/geolocation_config.mojom.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"
#include "ui/gl/android/scoped_a_native_window.h"

namespace content {
class WebContents;
}  // namespace content

namespace vr {

class BrowserUiInterface;
class VrGLThread;
class VrShellDelegate;
class VrWebContentsObserver;
enum class UiTestOperationType;
enum class UiTestOperationResult;

// The native instance of the Java VrShell. This class is not threadsafe and
// must only be used on the UI thread.
class VrShell {
 public:
  VrShell(JNIEnv* env,
          const base::android::JavaParamRef<jobject>& obj,
          const UiInitialState& ui_initial_state,
          VrShellDelegate* delegate,
          gvr_context* gvr_api,
          bool reprojected_rendering,
          const base::android::JavaParamRef<jobject>& j_web_contents);

  VrShell(const VrShell&) = delete;
  VrShell& operator=(const VrShell&) = delete;

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void OnTriggerEvent(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      bool touched);
  void OnPause(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void OnResume(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void SetSurface(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& obj,
                  const base::android::JavaParamRef<jobject>& surface);
  void ToggleCardboardGamepad(bool enabled);

  void ContentWebContentsDestroyed();

  void GvrDelegateReady();
  void SendRequestPresentReply(device::mojom::XRSessionPtr);

  void ForceExitVr();
  void ExitPresent();

  void ConnectPresentingService(
      device::mojom::XRRuntimeSessionOptionsPtr options);

  gfx::AcceleratedWidget GetRenderSurface();

 private:
  ~VrShell();
  void PostToGlThread(const base::Location& from_here, base::OnceClosure task);

  bool HasDaydreamSupport(JNIEnv* env);

  raw_ptr<content::WebContents> web_contents_ = nullptr;

  std::unique_ptr<VrWebContentsObserver> vr_web_contents_observer_;

  raw_ptr<VrShellDelegate> delegate_provider_ = nullptr;
  base::android::ScopedJavaGlobalRef<jobject> j_vr_shell_;

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  std::unique_ptr<VrGLThread> gl_thread_;
  raw_ptr<BrowserUiInterface> ui_;

  bool reprojected_rendering_;

  // Are we currently providing a gamepad factory to the gamepad manager?
  bool cardboard_gamepad_source_active_ = false;
  bool pending_cardboard_trigger_ = false;

  int64_t cardboard_gamepad_timer_ = 0;

  base::WaitableEvent gl_surface_created_event_;
  gl::ScopedANativeWindow surface_window_;

  base::WeakPtrFactory<VrShell> weak_ptr_factory_{this};
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_VR_SHELL_H_
