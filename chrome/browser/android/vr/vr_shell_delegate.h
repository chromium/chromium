// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_VR_SHELL_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_VR_VR_SHELL_DELEGATE_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "device/vr/android/gvr/gvr_delegate_provider.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device.h"

namespace device {
class GvrDevice;
}

namespace vr {

class VrShell;

class VrShellDelegate : public device::GvrDelegateProvider {
 public:
  VrShellDelegate(JNIEnv* env, jobject obj);

  VrShellDelegate(const VrShellDelegate&) = delete;
  VrShellDelegate& operator=(const VrShellDelegate&) = delete;

  ~VrShellDelegate() override;

  static device::GvrDelegateProvider* CreateVrShellDelegate();

  static VrShellDelegate* GetNativeVrShellDelegate(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jdelegate);

  void SetDelegate(VrShell* vr_shell);
  void RemoveDelegate();

  void SetPresentResult(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        jboolean success);
  void OnPause(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void OnResume(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  bool IsClearActivatePending(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& obj);
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  device::GvrDevice* GetGvrDevice();

  void SendRequestPresentReply(device::mojom::XRSessionPtr session);

  // device::GvrDelegateProvider implementation.
  void ExitWebVRPresent() override;

 private:
  // device::GvrDelegateProvider implementation.
  bool ShouldDisableGvrDevice() override;
  void StartWebXRPresentation(
      device::mojom::XRRuntimeSessionOptionsPtr options,
      base::OnceCallback<void(device::mojom::XRSessionPtr)> callback) override;

  void OnPresentResult(
      device::mojom::XRRuntimeSessionOptionsPtr options,
      base::OnceCallback<void(device::mojom::XRSessionPtr)> callback,
      bool success);

  base::android::ScopedJavaGlobalRef<jobject> j_vr_shell_delegate_;
  raw_ptr<VrShell> vr_shell_ = nullptr;

  // Deferred callback stored for later use in cases where vr_shell
  // wasn't ready yet. Used once SetDelegate is called.
  base::OnceCallback<void(bool)> on_present_result_callback_;

  // Mojo callback waiting for request present response. This is temporarily
  // stored here from OnPresentResult's outgoing ConnectPresentingService call
  // until the reply arguments are received by SendRequestPresentReply.
  base::OnceCallback<void(device::mojom::XRSessionPtr)>
      request_present_response_callback_;

  bool pending_successful_present_request_ = false;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtrFactory<VrShellDelegate> weak_ptr_factory_{this};
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_VR_SHELL_DELEGATE_H_
