// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/arcore_consent_prompt.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "chrome/android/features/vr/jni_headers/ArConsentDialog_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/vr/ar_jni_headers/ArCoreInstallUtils_jni.h"
#include "chrome/browser/android/vr/arcore_device/arcore_device_provider.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/vr/android/arcore/arcore_device_provider_factory.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace vr {

namespace {

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

base::android::ScopedJavaLocalRef<jobject> GetTabFromRenderer(
    int render_process_id,
    int render_frame_id) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  DCHECK(render_frame_host);

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);

  TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents);
  DCHECK(tab_android);

  base::android::ScopedJavaLocalRef<jobject> j_tab_android =
      tab_android->GetJavaObject();
  DCHECK(!j_tab_android.is_null());

  return j_tab_android;
}

}  // namespace

ArCoreConsentPrompt::ArCoreConsentPrompt() : weak_ptr_factory_(this) {}

ArCoreConsentPrompt::~ArCoreConsentPrompt() = default;

void ArCoreConsentPrompt::OnUserConsentResult(
    JNIEnv* env,
    jboolean is_granted) {
  jdelegate_.Reset();

  if (!on_user_consent_callback_)
    return;

  if (!is_granted) {
    CallDeferredUserConsentCallback(false);
    return;
  }

  java_install_utils_ =
      Java_ArCoreInstallUtils_create(env, reinterpret_cast<jlong>(this));

  if (java_install_utils_.is_null()) {
    CallDeferredUserConsentCallback(false);
    return;
  }

  RequestArModule();
}

// static
void ArCoreConsentPrompt::ShowConsentPrompt(
    int render_process_id,
    int render_frame_id,
    base::OnceCallback<void(bool)> response_callback) {
  on_user_consent_callback_ = std::move(response_callback);
  render_process_id_ = render_process_id;
  render_frame_id_ = render_frame_id;

  JNIEnv* env = AttachCurrentThread();
  jdelegate_ = Java_ArConsentDialog_showDialog(
      env, reinterpret_cast<jlong>(this),
      GetTabFromRenderer(render_process_id_, render_frame_id_));
  if (jdelegate_.is_null()) {
    std::move(on_user_consent_callback_).Run(false);
  }
}

bool ArCoreConsentPrompt::CanRequestInstallArModule() {
  return Java_ArCoreInstallUtils_canRequestInstallArModule(
      AttachCurrentThread(), java_install_utils_);
}

bool ArCoreConsentPrompt::ShouldRequestInstallArModule() {
  return Java_ArCoreInstallUtils_shouldRequestInstallArModule(
      AttachCurrentThread(), java_install_utils_);
}

void ArCoreConsentPrompt::RequestInstallArModule() {
  Java_ArCoreInstallUtils_requestInstallArModule(
      AttachCurrentThread(), java_install_utils_,
      GetTabFromRenderer(render_process_id_, render_frame_id_));
}

bool ArCoreConsentPrompt::ShouldRequestInstallSupportedArCore() {
  JNIEnv* env = AttachCurrentThread();
  return Java_ArCoreInstallUtils_shouldRequestInstallSupportedArCore(
      env, java_install_utils_);
}

void ArCoreConsentPrompt::RequestInstallSupportedArCore() {
  DCHECK(ShouldRequestInstallSupportedArCore());

  JNIEnv* env = AttachCurrentThread();
  Java_ArCoreInstallUtils_requestInstallSupportedArCore(
      env, java_install_utils_,
      GetTabFromRenderer(render_process_id_, render_frame_id_));
}

void ArCoreConsentPrompt::OnRequestInstallArModuleResult(
    JNIEnv* env,
    bool success) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (on_request_ar_module_result_callback_) {
    std::move(on_request_ar_module_result_callback_).Run(success);
  }
}

void ArCoreConsentPrompt::OnRequestInstallSupportedArCoreResult(
    JNIEnv* env,
    bool success) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(on_request_arcore_install_or_update_result_callback_);

  std::move(on_request_arcore_install_or_update_result_callback_).Run(success);
}

void ArCoreConsentPrompt::RequestArModule() {
  DVLOG(1) << __func__;
  if (ShouldRequestInstallArModule()) {
    if (!CanRequestInstallArModule()) {
      OnRequestArModuleResult(false);
      return;
    }

    on_request_ar_module_result_callback_ = base::BindOnce(
        &ArCoreConsentPrompt::OnRequestArModuleResult, GetWeakPtr());
    RequestInstallArModule();
    return;
  }

  OnRequestArModuleResult(true);
}

void ArCoreConsentPrompt::OnRequestArModuleResult(bool success) {
  DVLOG(3) << __func__ << ": success=" << success;

  if (!success) {
    CallDeferredUserConsentCallback(false);
    return;
  }

  RequestArCoreInstallOrUpdate();
}

void ArCoreConsentPrompt::RequestArCoreInstallOrUpdate() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!on_request_arcore_install_or_update_result_callback_);

  if (ShouldRequestInstallSupportedArCore()) {
    // ARCore is not installed or requires an update. Store the callback to be
    // processed later once installation/update is complete or got cancelled.
    on_request_arcore_install_or_update_result_callback_ = base::BindOnce(
        &ArCoreConsentPrompt::OnRequestArCoreInstallOrUpdateResult,
        GetWeakPtr());

    RequestInstallSupportedArCore();
    return;
  }

  OnRequestArCoreInstallOrUpdateResult(true);
}

void ArCoreConsentPrompt::OnRequestArCoreInstallOrUpdateResult(bool success) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  CallDeferredUserConsentCallback(success);
}

void ArCoreConsentPrompt::CallDeferredUserConsentCallback(
    bool is_permission_granted) {
  if (on_user_consent_callback_)
    std::move(on_user_consent_callback_).Run(is_permission_granted);
}

static void JNI_ArCoreInstallUtils_InstallArCoreDeviceProviderFactory(
    JNIEnv* env) {
  device::ArCoreDeviceProviderFactory::Install(
      std::make_unique<ArCoreDeviceProviderFactoryImpl>());
}

}  // namespace vr
