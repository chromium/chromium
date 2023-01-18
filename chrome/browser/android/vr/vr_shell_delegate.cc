// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/vr_shell_delegate.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/android/features/vr/split_jni_headers/VrShellDelegate_jni.h"
#include "chrome/browser/android/vr/vr_shell.h"
#include "chrome/browser/android/vr/vrcore_install_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/vr/assets_loader.h"
#include "content/public/browser/browser_xr_runtime.h"
#include "content/public/browser/xr_runtime_manager.h"
#include "device/vr/android/gvr/gvr_delegate_provider_factory.h"
#include "device/vr/android/gvr/gvr_device.h"
#include "device/vr/buildflags/buildflags.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace vr {

namespace {

class VrShellDelegateProviderFactory
    : public device::GvrDelegateProviderFactory {
 public:
  VrShellDelegateProviderFactory() = default;

  VrShellDelegateProviderFactory(const VrShellDelegateProviderFactory&) =
      delete;
  VrShellDelegateProviderFactory& operator=(
      const VrShellDelegateProviderFactory&) = delete;

  ~VrShellDelegateProviderFactory() override = default;
  device::GvrDelegateProvider* CreateGvrDelegateProvider() override;
};

device::GvrDelegateProvider*
VrShellDelegateProviderFactory::CreateGvrDelegateProvider() {
  return VrShellDelegate::CreateVrShellDelegate();
}

}  // namespace

VrShellDelegate::VrShellDelegate(JNIEnv* env, jobject obj)
    : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  DVLOG(1) << __FUNCTION__ << "=" << this;
  j_vr_shell_delegate_.Reset(env, obj);
}

VrShellDelegate::~VrShellDelegate() {
  DVLOG(1) << __FUNCTION__ << "=" << this;
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

void VrShellDelegate::SetDelegate(VrShell* vr_shell) {
  vr_shell_ = vr_shell;

  if (pending_successful_present_request_) {
    CHECK(!on_present_result_callback_.is_null());
    pending_successful_present_request_ = false;
    std::move(on_present_result_callback_).Run(true);
  }
}

void VrShellDelegate::RemoveDelegate() {
  vr_shell_ = nullptr;
  if (pending_successful_present_request_) {
    CHECK(!on_present_result_callback_.is_null());
    pending_successful_present_request_ = false;
    std::move(on_present_result_callback_).Run(false);
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

void VrShellDelegate::OnPresentResult(
    device::mojom::XRRuntimeSessionOptionsPtr options,
    base::OnceCallback<void(device::mojom::XRSessionPtr)> callback,
    bool success) {
  DVLOG(1) << __FUNCTION__ << ": success=" << success;
  DCHECK(options);

  DVLOG(3) << __func__ << ": options->required_features.size()="
           << options->required_features.size()
           << ", options->optional_features.size()="
           << options->optional_features.size();

  if (!success) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (!vr_shell_) {
    // We have to wait until the GL thread is ready since we have to get the
    // XRPresentationClient.
    pending_successful_present_request_ = true;
    on_present_result_callback_ = base::BindOnce(
        &VrShellDelegate::OnPresentResult, base::Unretained(this),
        std::move(options), std::move(callback));
    return;
  }

  DVLOG(1) << __FUNCTION__ << ": connecting presenting service";
  request_present_response_callback_ = std::move(callback);
  vr_shell_->ConnectPresentingService(std::move(options));
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
  return VrCoreInstallHelper::VrSupportNeedsUpdate();
}

void VrShellDelegate::StartWebXRPresentation(
    device::mojom::XRRuntimeSessionOptionsPtr options,
    base::OnceCallback<void(device::mojom::XRSessionPtr)> callback) {
  if (!on_present_result_callback_.is_null() ||
      !request_present_response_callback_.is_null()) {
    // Can only handle one request at a time. This is also extremely unlikely to
    // happen in practice.
    std::move(callback).Run(nullptr);
    return;
  }

  on_present_result_callback_ =
      base::BindOnce(&VrShellDelegate::OnPresentResult, base::Unretained(this),
                     std::move(options), std::move(callback));

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

}  // namespace vr
