// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/vr_module_provider.h"

#include <memory>
#include <utility>

#include "chrome/android/features/vr/jni_headers/VrModuleProvider_jni.h"
#include "chrome/browser/android/vr/gvr_consent_helper_impl.h"
#include "chrome/browser/android/vr/register_jni.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/vr/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_ARCORE)
#include "chrome/browser/android/vr/arcore_device/arcore_consent_prompt.h"
#endif

namespace vr {

VrModuleProvider::VrModuleProvider(TabAndroid* tab)
    : j_vr_module_provider_(
          Java_VrModuleProvider_create(base::android::AttachCurrentThread(),
                                       (jlong)this)),
      tab_(tab) {
  DCHECK(tab_);
}

VrModuleProvider::~VrModuleProvider() {
  if (!j_vr_module_provider_.obj()) {
    return;
  }
  Java_VrModuleProvider_onNativeDestroy(base::android::AttachCurrentThread(),
                                        j_vr_module_provider_);
}

bool VrModuleProvider::ModuleInstalled() {
  return Java_VrModuleProvider_isModuleInstalled(
      base::android::AttachCurrentThread());
}

void VrModuleProvider::InstallModule(
    base::OnceCallback<void(bool)> on_finished) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_finished_callbacks_.push(std::move(on_finished));
  // Don't request VR module multiple times in parallel.
  if (on_finished_callbacks_.size() > 1) {
    return;
  }

  Java_VrModuleProvider_installModule(base::android::AttachCurrentThread(),
                                      j_vr_module_provider_,
                                      tab_->GetJavaObject());
}

void VrModuleProvider::OnInstalledModule(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(on_finished_callbacks_.size(), 0UL);
  while (!on_finished_callbacks_.empty()) {
    std::move(on_finished_callbacks_.front()).Run(success);
    on_finished_callbacks_.pop();
  }
}

// static
std::unique_ptr<VrModuleProvider> VrModuleProviderFactory::CreateModuleProvider(
    int render_process_id,
    int render_frame_id) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  DCHECK(render_frame_host);

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);

  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  DCHECK(tab);

  return std::make_unique<VrModuleProvider>(tab);
}

static void JNI_VrModuleProvider_Init(JNIEnv* env) {
  GvrConsentHelper::SetInstance(std::make_unique<vr::GvrConsentHelperImpl>());
#if BUILDFLAG(ENABLE_ARCORE)
  ArCoreConsentPromptInterface::SetInstance(new ArCoreConsentPrompt());
#endif
}

static void JNI_VrModuleProvider_RegisterJni(JNIEnv* env) {
  CHECK(RegisterJni(env));
}

}  // namespace vr
