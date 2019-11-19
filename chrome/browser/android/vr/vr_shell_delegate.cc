// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/vr_shell_delegate.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "chrome/android/features/vr/jni_headers/VrShellDelegate_jni.h"
#include "chrome/browser/android/vr/arcore_device/arcore_device_provider.h"
#include "chrome/browser/android/vr/metrics_util_android.h"
#include "chrome/browser/android/vr/vr_shell.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/vr_assets_component_installer.h"
#include "chrome/browser/vr/assets_loader.h"
#include "chrome/browser/vr/metrics/metrics_helper.h"
#include "chrome/browser/vr/service/browser_xr_runtime.h"
#include "chrome/browser/vr/service/vr_service_impl.h"
#include "chrome/browser/vr/service/xr_runtime_manager.h"
#include "content/public/browser/webvr_service_provider.h"
#include "device/vr/android/gvr/gvr_delegate_provider_factory.h"
#include "device/vr/android/gvr/gvr_device.h"
#include "device/vr/buildflags/buildflags.h"
#include "device/vr/public/cpp/session_mode.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace vr {

namespace {

void SetInlineVrEnabled(XRRuntimeManager& runtime_manager, bool enable) {
  runtime_manager.ForEachRuntime([enable](BrowserXRRuntime* runtime) {
    runtime->GetRuntime()->SetInlinePosesEnabled(enable);
  });
}

class VrShellDelegateProviderFactory
    : public device::GvrDelegateProviderFactory {
 public:
  VrShellDelegateProviderFactory() = default;
  ~VrShellDelegateProviderFactory() override = default;
  device::GvrDelegateProvider* CreateGvrDelegateProvider() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(VrShellDelegateProviderFactory);
};

device::GvrDelegateProvider*
VrShellDelegateProviderFactory::CreateGvrDelegateProvider() {
  return VrShellDelegate::CreateVrShellDelegate();
}

}  // namespace

VrShellDelegate::VrShellDelegate(JNIEnv* env, jobject obj)
    : task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  DVLOG(1) << __FUNCTION__ << "=" << this;
  j_vr_shell_delegate_.Reset(env, obj);
  XRRuntimeManager::AddObserver(this);
}

VrShellDelegate::~VrShellDelegate() {
  DVLOG(1) << __FUNCTION__ << "=" << this;
  XRRuntimeManager::RemoveObserver(this);
  device::GvrDevice* gvr_device = GetGvrDevice();
  if (gvr_device)
    gvr_device->OnExitPresent();
  if (!on_present_result_callback_.is_null())
    std::move(on_present_result_callback_).Run(false);
}

device::GvrDelegateProvider* VrShellDelegate::CreateVrShellDelegate() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jdelegate = Java_VrShellDelegate_getInstance(env);
  if (!jdelegate.is_null())
    return GetNativeVrShellDelegate(env, jdelegate);
  return nullptr;
}

VrShellDelegate* VrShellDelegate::GetNativeVrShellDelegate(
    JNIEnv* env,
    const JavaRef<jobject>& jdelegate) {
  return reinterpret_cast<VrShellDelegate*>(
      Java_VrShellDelegate_getNativePointer(env, jdelegate));
}

void VrShellDelegate::SetDelegate(VrShell* vr_shell,
                                  gvr::ViewerType viewer_type) {
  vr_shell_ = vr_shell;

  // When VrShell is created, we disable magic window mode as the user is inside
  // the headset. As currently implemented, orientation-based magic window
  // doesn't make sense when the window is fixed and the user is moving.
  auto* xr_runtime_manager = XRRuntimeManager::GetInstanceIfCreated();
  if (xr_runtime_manager) {
    // If the XRRuntimeManager singleton currently exists, this will disable
    // inline VR. Otherwise, the callback for 'XRRuntimeManagerObserver'
    // ('OnRuntimeAdded') will take care of it.
    SetInlineVrEnabled(*xr_runtime_manager, false);
  }

  if (pending_successful_present_request_) {
    CHECK(!on_present_result_callback_.is_null());
    pending_successful_present_request_ = false;
    std::move(on_present_result_callback_).Run(true);
  }

  if (pending_vr_start_action_) {
    vr_shell_->RecordVrStartAction(*pending_vr_start_action_);
    pending_vr_start_action_ = base::nullopt;
  }

  JNIEnv* env = AttachCurrentThread();
  std::unique_ptr<VrCoreInfo> vr_core_info = MakeVrCoreInfo(env);
  MetricsUtilAndroid::LogGvrVersionForVrViewerType(viewer_type, *vr_core_info);
}

void VrShellDelegate::RemoveDelegate() {
  vr_shell_ = nullptr;
  if (pending_successful_present_request_) {
    CHECK(!on_present_result_callback_.is_null());
    pending_successful_present_request_ = false;
    std::move(on_present_result_callback_).Run(false);
  }

  auto* xr_runtime_manager = XRRuntimeManager::GetInstanceIfCreated();
  if (xr_runtime_manager) {
    SetInlineVrEnabled(*xr_runtime_manager, true);
  }

  device::GvrDevice* gvr_device = GetGvrDevice();
  if (gvr_device)
    gvr_device->OnExitPresent();
}

void VrShellDelegate::SetPresentResult(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj,
                                       jboolean success) {
  CHECK(!on_present_result_callback_.is_null());
  std::move(on_present_result_callback_).Run(static_cast<bool>(success));
}

void VrShellDelegate::RecordVrStartAction(
    JNIEnv* env,
    jint start_action) {
  VrStartAction action = static_cast<VrStartAction>(start_action);

  if (!vr_shell_) {
    pending_vr_start_action_ = action;
    return;
  }

  vr_shell_->RecordVrStartAction(action);
}

void VrShellDelegate::OnPresentResult(
    device::mojom::VRDisplayInfoPtr display_info,
    device::mojom::XRRuntimeSessionOptionsPtr options,
    base::OnceCallback<void(device::mojom::XRSessionPtr)> callback,
    bool success) {
  DVLOG(1) << __FUNCTION__ << ": success=" << success;
  DCHECK(options);

  if (!success) {
    std::move(callback).Run(nullptr);
    possible_presentation_start_action_ = base::nullopt;
    return;
  }

  if (!vr_shell_) {
    // We have to wait until the GL thread is ready since we have to get the
    // XRPresentationClient.
    pending_successful_present_request_ = true;
    on_present_result_callback_ = base::BindOnce(
        &VrShellDelegate::OnPresentResult, base::Unretained(this),
        std::move(display_info), std::move(options), std::move(callback));
    return;
  }

  // If possible_presentation_start_action_ is not set at this point, then this
  // request present probably came from blink, and has already been reported
  // from there.
  if (possible_presentation_start_action_) {
    vr_shell_->RecordPresentationStartAction(
        *possible_presentation_start_action_, *options);
    possible_presentation_start_action_ = base::nullopt;
  }

  DVLOG(1) << __FUNCTION__ << ": connecting presenting service";
  request_present_response_callback_ = std::move(callback);
  vr_shell_->ConnectPresentingService(std::move(display_info),
                                      std::move(options));
}

void VrShellDelegate::SendRequestPresentReply(
    device::mojom::XRSessionPtr session) {
  DVLOG(1) << __FUNCTION__;
  if (!request_present_response_callback_) {
    DLOG(ERROR) << __FUNCTION__ << ": ERROR: no callback";
    return;
  }

  std::move(request_present_response_callback_).Run(std::move(session));
}

void VrShellDelegate::OnPause(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  if (vr_shell_)
    return;
  device::GvrDevice* gvr_device = GetGvrDevice();
  if (gvr_device)
    gvr_device->PauseTracking();
}

void VrShellDelegate::OnResume(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  if (vr_shell_)
    return;
  device::GvrDevice* gvr_device = GetGvrDevice();
  if (gvr_device)
    gvr_device->ResumeTracking();
}

void VrShellDelegate::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

bool VrShellDelegate::ShouldDisableGvrDevice() {
  int vr_support_level =
      Java_VrShellDelegate_getVrSupportLevel(AttachCurrentThread());
  return static_cast<VrSupportLevel>(vr_support_level) <=
         VrSupportLevel::kVrNeedsUpdate;
}

void VrShellDelegate::StartWebXRPresentation(
    device::mojom::VRDisplayInfoPtr display_info,
    device::mojom::XRRuntimeSessionOptionsPtr options,
    base::OnceCallback<void(device::mojom::XRSessionPtr)> callback) {
  if (!on_present_result_callback_.is_null() ||
      !request_present_response_callback_.is_null()) {
    // Can only handle one request at a time. This is also extremely unlikely to
    // happen in practice.
    std::move(callback).Run(nullptr);
    return;
  }

  on_present_result_callback_ = base::BindOnce(
      &VrShellDelegate::OnPresentResult, base::Unretained(this),
      std::move(display_info), std::move(options), std::move(callback));

  // If/When VRShell is ready for use it will call SetPresentResult.
  JNIEnv* env = AttachCurrentThread();
  Java_VrShellDelegate_presentRequested(env, j_vr_shell_delegate_);
}

void VrShellDelegate::ExitWebVRPresent() {
  JNIEnv* env = AttachCurrentThread();
  Java_VrShellDelegate_exitWebVRPresent(env, j_vr_shell_delegate_);
  device::GvrDevice* gvr_device = GetGvrDevice();
  if (gvr_device)
    gvr_device->OnExitPresent();
}

std::unique_ptr<VrCoreInfo> VrShellDelegate::MakeVrCoreInfo(JNIEnv* env) {
  return std::unique_ptr<VrCoreInfo>(reinterpret_cast<VrCoreInfo*>(
      Java_VrShellDelegate_getVrCoreInfo(env, j_vr_shell_delegate_)));
}

void VrShellDelegate::OnRuntimeAdded(vr::BrowserXRRuntime* runtime) {
  if (vr_shell_) {
    // See comment in VrShellDelegate::SetDelegate. This handles the case where
    // VrShell is created before the device code is initialized (like when
    // entering VR browsing on a non-webVR page).
    runtime->GetRuntime()->SetInlinePosesEnabled(false);
  }
}

device::GvrDevice* VrShellDelegate::GetGvrDevice() {
  return device::GvrDelegateProviderFactory::GetDevice();
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong JNI_VrShellDelegate_Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(new VrShellDelegate(env, obj));
}

static void JNI_VrShellDelegate_OnLibraryAvailable(JNIEnv* env) {
  device::GvrDelegateProviderFactory::Install(
      std::make_unique<VrShellDelegateProviderFactory>());
}

static void JNI_VrShellDelegate_RegisterVrAssetsComponent(JNIEnv* env) {
  component_updater::RegisterVrAssetsComponent(
      g_browser_process->component_updater());
}

}  // namespace vr
