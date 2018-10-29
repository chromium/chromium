// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/vr_shell_delegate.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/callback_helpers.h"
#include "chrome/browser/android/vr/arcore_device/arcore_device_provider.h"
#include "chrome/browser/android/vr/metrics_util_android.h"
#include "chrome/browser/android/vr/vr_shell.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/vr_assets_component_installer.h"
#include "chrome/browser/vr/assets_loader.h"
#include "chrome/browser/vr/metrics/metrics_helper.h"
#include "chrome/browser/vr/service/vr_service_impl.h"
#include "content/public/browser/webvr_service_provider.h"
#include "device/vr/android/gvr/gvr_delegate_provider_factory.h"
#include "device/vr/android/gvr/gvr_device.h"
#include "device/vr/buildflags/buildflags.h"
#include "jni/VrShellDelegate_jni.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"

#if BUILDFLAG(ENABLE_ARCORE)
#include "device/vr/android/arcore/arcore_device_provider_factory.h"
#endif

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace vr {

namespace {

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

#if BUILDFLAG(ENABLE_ARCORE)
class ArCoreDeviceProviderFactoryImpl
    : public device::ArCoreDeviceProviderFactory {
 public:
  ArCoreDeviceProviderFactoryImpl() = default;
  ~ArCoreDeviceProviderFactoryImpl() override = default;
  std::unique_ptr<device::VRDeviceProvider> CreateDeviceProvider() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArCoreDeviceProviderFactoryImpl);
};

std::unique_ptr<device::VRDeviceProvider>
ArCoreDeviceProviderFactoryImpl::CreateDeviceProvider() {
  return std::make_unique<device::ArCoreDeviceProvider>();
}
#endif

}  // namespace

VrShellDelegate::VrShellDelegate(JNIEnv* env, jobject obj)
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_ptr_factory_(this) {
  DVLOG(1) << __FUNCTION__ << "=" << this;
  j_vr_shell_delegate_.Reset(env, obj);
}

VrShellDelegate::~VrShellDelegate() {
  DVLOG(1) << __FUNCTION__ << "=" << this;
  device::GvrDevice* device = GetDevice();
  if (device)
    device->OnExitPresent();
  if (!on_present_result_callback_.is_null())
    base::ResetAndReturn(&on_present_result_callback_).Run(false);
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
  device::GvrDevice* device = GetDevice();
  // When VrShell is created, we disable magic window mode as the user is inside
  // the headset. As currently implemented, orientation-based magic window
  // doesn't make sense when the window is fixed and the user is moving.
  if (device)
    device->SetMagicWindowEnabled(false);

  if (pending_successful_present_request_) {
    CHECK(!on_present_result_callback_.is_null());
    pending_successful_present_request_ = false;
    base::ResetAndReturn(&on_present_result_callback_).Run(true);
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
    base::ResetAndReturn(&on_present_result_callback_).Run(false);
  }
  device::GvrDevice* device = GetDevice();
  if (device) {
    device->SetMagicWindowEnabled(true);
    device->OnExitPresent();
  }
}

void VrShellDelegate::SetPresentResult(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj,
                                       jboolean success) {
  CHECK(!on_present_result_callback_.is_null());
  base::ResetAndReturn(&on_present_result_callback_)
      .Run(static_cast<bool>(success));
}

void VrShellDelegate::RecordVrStartAction(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint start_action) {
  VrStartAction action = static_cast<VrStartAction>(start_action);

  if (action == VrStartAction::kDeepLinkedApp) {
    // If this is a deep linked app we expect a DisplayActivate to be coming
    // down the pipeline shortly.
    possible_presentation_start_action_ =
        PresentationStartAction::kDeepLinkedApp;
  }

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
        *possible_presentation_start_action_);
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

  base::ResetAndReturn(&request_present_response_callback_)
      .Run(std::move(session));
}

void VrShellDelegate::DisplayActivate(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  device::GvrDevice* device = static_cast<device::GvrDevice*>(GetDevice());
  if (device) {
    if (!possible_presentation_start_action_ ||
        possible_presentation_start_action_ !=
            PresentationStartAction::kDeepLinkedApp) {
      // The only possible sources for DisplayActivate are at the moment DLAs
      // and HeadsetActivations. Therefore if it's not a DLA it must be a
      // HeadsetActivation.
      possible_presentation_start_action_ =
          PresentationStartAction::kHeadsetActivation;
    }

    device->Activate(
        device::mojom::VRDisplayEventReason::MOUNTED,
        base::BindRepeating(&VrShellDelegate::OnActivateDisplayHandled,
                            weak_ptr_factory_.GetWeakPtr()));
  } else {
    OnActivateDisplayHandled(true /* will_not_present */);
  }
}

void VrShellDelegate::OnPause(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  if (vr_shell_)
    return;
  device::GvrDevice* device = GetDevice();
  if (device)
    device->PauseTracking();
}

void VrShellDelegate::OnResume(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  if (vr_shell_)
    return;
  device::GvrDevice* device = GetDevice();
  if (device)
    device->ResumeTracking();
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

void VrShellDelegate::SetDeviceId(device::mojom::XRDeviceId device_id) {
  device_id_ = device_id;
  if (vr_shell_) {
    device::GvrDevice* device = GetDevice();
    // See comment in VrShellDelegate::SetDelegate. This handles the case where
    // VrShell is created before the device code is initialized (like when
    // entering VR browsing on a non-webVR page).
    if (device)
      device->SetMagicWindowEnabled(false);
  }
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
  device::GvrDevice* device = GetDevice();
  if (device)
    device->OnExitPresent();
}

std::unique_ptr<VrCoreInfo> VrShellDelegate::MakeVrCoreInfo(JNIEnv* env) {
  return std::unique_ptr<VrCoreInfo>(reinterpret_cast<VrCoreInfo*>(
      Java_VrShellDelegate_getVrCoreInfo(env, j_vr_shell_delegate_)));
}

void VrShellDelegate::OnActivateDisplayHandled(bool will_not_present) {
  if (will_not_present) {
    // WebVR page didn't request presentation in the vrdisplayactivate handler.
    // Tell VrShell that we are in VR Browsing Mode.
    ExitWebVRPresent();
    // Reset possible_presentation_start_action_ as it may have been set.
    possible_presentation_start_action_ = base::nullopt;
  }
}

void VrShellDelegate::OnListeningForActivateChanged(bool listening) {
  JNIEnv* env = AttachCurrentThread();
  Java_VrShellDelegate_setListeningForWebVrActivate(env, j_vr_shell_delegate_,
                                                    listening);
}

device::GvrDevice* VrShellDelegate::GetDevice() {
  return device::GvrDelegateProviderFactory::GetDevice();
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong JNI_VrShellDelegate_Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(new VrShellDelegate(env, obj));
}

static void JNI_VrShellDelegate_OnLibraryAvailable(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz) {
  device::GvrDelegateProviderFactory::Install(
      std::make_unique<VrShellDelegateProviderFactory>());

#if BUILDFLAG(ENABLE_ARCORE)
  // TODO(https://crbug.com/837965): Move this to an ARCore-specific location
  // with similar timing (occurs before XRRuntimeManager is initialized).
  device::ArCoreDeviceProviderFactory::Install(
      std::make_unique<ArCoreDeviceProviderFactoryImpl>());
#endif
}

static void JNI_VrShellDelegate_RegisterVrAssetsComponent(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz) {
  component_updater::RegisterVrAssetsComponent(
      g_browser_process->component_updater());
}

}  // namespace vr
