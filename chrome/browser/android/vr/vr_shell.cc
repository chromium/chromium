// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/vr_shell.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/android/features/vr/split_jni_headers/VrShell_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/vr/vr_gl_thread.h"
#include "chrome/browser/android/vr/vr_shell_delegate.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/page_info/chrome_page_info_delegate.h"
#include "chrome/browser/vr/assets_loader.h"
#include "chrome/browser/vr/browser_renderer.h"
#include "chrome/browser/vr/location_bar_helper.h"
#include "chrome/browser/vr/model/assets.h"
#include "chrome/browser/vr/model/omnibox_suggestions.h"
#include "chrome/browser/vr/model/text_input_info.h"
#include "chrome/browser/vr/ui_test_input.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/browser/vr/vr_web_contents_observer.h"
#include "chrome/common/url_constants.h"
#include "components/browser_ui/util/android/url_constants.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/common/url_constants.h"
#include "device/vr/android/gvr/gvr_device.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/android/window_android.h"
#include "ui/base/page_transition_types.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/surface_texture.h"
#include "url/gurl.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace vr {

namespace {
vr::VrShell* g_vr_shell_instance;

void SetIsInVR(content::WebContents* contents, bool is_in_vr) {
  if (contents && contents->GetRenderWidgetHostView()) {
    VrTabHelper* vr_tab_helper = VrTabHelper::FromWebContents(contents);
    DCHECK(vr_tab_helper);
    vr_tab_helper->SetIsInVr(is_in_vr);
  }
}

}  // namespace

VrShell::VrShell(JNIEnv* env,
                 const JavaParamRef<jobject>& obj,
                 const UiInitialState& ui_initial_state,
                 VrShellDelegate* delegate,
                 gvr_context* gvr_api,
                 bool reprojected_rendering,
                 const JavaParamRef<jobject>& j_web_contents)
    : delegate_provider_(delegate),
      main_thread_task_runner_(
          base::SingleThreadTaskRunner::GetCurrentDefault()),
      reprojected_rendering_(reprojected_rendering),
      gl_surface_created_event_(
          base::WaitableEvent::ResetPolicy::MANUAL,
          base::WaitableEvent::InitialState::NOT_SIGNALED) {
  DVLOG(1) << __FUNCTION__ << "=" << this;
  DCHECK(g_vr_shell_instance == nullptr);
  g_vr_shell_instance = this;
  j_vr_shell_.Reset(env, obj);

  base::OnceCallback<gfx::AcceleratedWidget()> surface_callback =
      base::BindOnce(&VrShell::GetRenderSurface, base::Unretained(this));

  gl_thread_ = std::make_unique<VrGLThread>(
      weak_ptr_factory_.GetWeakPtr(), main_thread_task_runner_, gvr_api,
      ui_initial_state, reprojected_rendering_, HasDaydreamSupport(env),
      &gl_surface_created_event_, std::move(surface_callback));
  ui_ = gl_thread_.get();

  gl_thread_->Start();

  web_contents_ = content::WebContents::FromJavaWebContents(j_web_contents);
  SetIsInVR(web_contents_, true);

  ui_->SetWebVrMode(true);

  vr_web_contents_observer_ = std::make_unique<VrWebContentsObserver>(
      web_contents_, base::BindOnce(&VrShell::ContentWebContentsDestroyed,
                                    base::Unretained(this)));
}

void VrShell::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

void VrShell::ContentWebContentsDestroyed() {
  web_contents_ = nullptr;
}

VrShell::~VrShell() {
  DVLOG(1) << __FUNCTION__ << "=" << this;
  SetIsInVR(web_contents_, false);
  delegate_provider_->RemoveDelegate();
  {
    // The GvrLayout is, and must always be, used only on the UI thread, and the
    // GvrApi used for rendering should only be used from the GL thread as it's
    // not thread safe. However, the GvrLayout owns the GvrApi instance, and
    // when it gets shut down it deletes the GvrApi instance with it. Therefore,
    // we need to block shutting down the GvrLayout on stopping our GL thread
    // from using the GvrApi instance.
    // base::Thread::Stop, which is called when destroying the thread, asserts
    // that sync primitives are allowed to prevent jank, but there shouldn't be
    // any concerns regarding jank in this case, because we're switching from 3D
    // to 2D, adding/removing a bunch of Java views, and probably changing
    // device orientation here.
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_thread_join;
    gl_thread_.reset();
  }
  g_vr_shell_instance = nullptr;
}

void VrShell::PostToGlThread(const base::Location& from_here,
                             base::OnceClosure task) {
  gl_thread_->task_runner()->PostTask(from_here, std::move(task));
}

void VrShell::ToggleCardboardGamepad(bool enabled) {
  // Enable/disable updating gamepad state.
  if (cardboard_gamepad_source_active_ && !enabled) {
    cardboard_gamepad_source_active_ = false;
  }

  if (!cardboard_gamepad_source_active_ && enabled) {
    device::GvrDevice* gvr_device = delegate_provider_->GetGvrDevice();
    if (!gvr_device)
      return;
    cardboard_gamepad_source_active_ = true;
    if (pending_cardboard_trigger_) {
      OnTriggerEvent(nullptr, JavaParamRef<jobject>(nullptr), true);
    }
    pending_cardboard_trigger_ = false;
  }
}

void VrShell::OnTriggerEvent(JNIEnv* env,
                             const JavaParamRef<jobject>& obj,
                             bool touched) {
  // If we are running cardboard, update gamepad state.
  if (cardboard_gamepad_source_active_) {
    cardboard_gamepad_timer_++;
  } else {
    pending_cardboard_trigger_ = touched;
  }

  PostToGlThread(FROM_HERE,
                 base::BindOnce(&BrowserRenderer::OnTriggerEvent,
                                gl_thread_->GetBrowserRenderer(), touched));
}

void VrShell::OnPause(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  PostToGlThread(FROM_HERE, base::BindOnce(&BrowserRenderer::OnPause,
                                           gl_thread_->GetBrowserRenderer()));
}

void VrShell::OnResume(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  PostToGlThread(FROM_HERE, base::BindOnce(&BrowserRenderer::OnResume,
                                           gl_thread_->GetBrowserRenderer()));
}

void VrShell::SetSurface(JNIEnv* env,
                         const JavaParamRef<jobject>& obj,
                         const JavaParamRef<jobject>& surface) {
  DCHECK(!reprojected_rendering_);
  DCHECK(!surface.is_null());
  surface_window_ = gl::ScopedANativeWindow(
      gl::ScopedJavaSurface(surface, /*auto_release=*/false));
  gl_surface_created_event_.Signal();
}

void VrShell::ConnectPresentingService(
    device::mojom::XRRuntimeSessionOptionsPtr options) {
  PostToGlThread(
      FROM_HERE,
      base::BindOnce(&BrowserRenderer::ConnectPresentingService,
                     gl_thread_->GetBrowserRenderer(), std::move(options)));
}

void VrShell::GvrDelegateReady() {
  delegate_provider_->SetDelegate(this);
}

void VrShell::SendRequestPresentReply(device::mojom::XRSessionPtr session) {
  delegate_provider_->SendRequestPresentReply(std::move(session));
}

void VrShell::ForceExitVr() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShell_forceExitVr(env, j_vr_shell_);
}

void VrShell::ExitPresent() {
  delegate_provider_->ExitWebVRPresent();
}

bool VrShell::HasDaydreamSupport(JNIEnv* env) {
  return Java_VrShell_hasDaydreamSupport(env, j_vr_shell_);
}

gfx::AcceleratedWidget VrShell::GetRenderSurface() {
  return surface_window_.a_native_window();
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong JNI_VrShell_Init(JNIEnv* env,
                       const JavaParamRef<jobject>& obj,
                       const JavaParamRef<jobject>& delegate,
                       jlong gvr_api,
                       jboolean reprojected_rendering,
                       const JavaParamRef<jobject>& j_web_contents) {
  UiInitialState ui_initial_state;
  return reinterpret_cast<intptr_t>(
      new VrShell(env, obj, ui_initial_state,
                  VrShellDelegate::GetNativeVrShellDelegate(env, delegate),
                  reinterpret_cast<gvr_context*>(gvr_api),
                  reprojected_rendering, j_web_contents));
}

}  // namespace vr
