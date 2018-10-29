// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/cdm/media_drm_credential_manager.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "content/public/browser/provision_fetcher_factory.h"
#include "jni/MediaDrmCredentialManager_jni.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/provision_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"
#include "url/gurl.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;

namespace {

void MediaDrmCredentialManagerCallback(
    const ScopedJavaGlobalRef<jobject>& j_media_drm_credential_manager_callback,
    bool succeeded) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MediaDrmCredentialManagerCallback_onCredentialResetFinished(
      env, j_media_drm_credential_manager_callback, succeeded);
}

}  // namespace

MediaDrmCredentialManager::MediaDrmCredentialManager() {}

MediaDrmCredentialManager::~MediaDrmCredentialManager() {}

// static
MediaDrmCredentialManager* MediaDrmCredentialManager::GetInstance() {
  return base::Singleton<MediaDrmCredentialManager>::get();
}

void MediaDrmCredentialManager::ResetCredentials(
    const ResetCredentialsCB& reset_credentials_cb) {
  // Ignore reset request if one is already in progress.
  if (!reset_credentials_cb_.is_null())
    return;

  reset_credentials_cb_ = reset_credentials_cb;

  // First reset the L3 credentials.
  ResetCredentialsInternal(media::MediaDrmBridge::SECURITY_LEVEL_3);
}

// static
void JNI_MediaDrmCredentialManager_ResetCredentials(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& j_media_drm_credential_manager_callback) {
  MediaDrmCredentialManager* media_drm_credential_manager =
      MediaDrmCredentialManager::GetInstance();

  ScopedJavaGlobalRef<jobject> j_scoped_media_drm_credential_manager_callback;
  j_scoped_media_drm_credential_manager_callback.Reset(
      env, j_media_drm_credential_manager_callback);

  MediaDrmCredentialManager::ResetCredentialsCB callback_runner =
      base::Bind(&MediaDrmCredentialManagerCallback,
                 j_scoped_media_drm_credential_manager_callback);

  media_drm_credential_manager->ResetCredentials(callback_runner);
}

void MediaDrmCredentialManager::OnResetCredentialsCompleted(
    SecurityLevel security_level, bool success) {
  if (security_level == media::MediaDrmBridge::SECURITY_LEVEL_3 && success) {
    ResetCredentialsInternal(media::MediaDrmBridge::SECURITY_LEVEL_1);
    return;
  }

  base::ResetAndReturn(&reset_credentials_cb_).Run(success);
  media_drm_bridge_ = nullptr;
}

// TODO(ddorwin): The key system should be passed in. http://crbug.com/459400
void MediaDrmCredentialManager::ResetCredentialsInternal(
    SecurityLevel security_level) {
  // Create provision fetcher for the default browser http request context.
  media::CreateFetcherCB create_fetcher_cb =
      base::Bind(&content::CreateProvisionFetcher,
                 g_browser_process->system_network_context_manager()
                     ->GetSharedURLLoaderFactory());

  ResetCredentialsCB reset_credentials_cb =
      base::Bind(&MediaDrmCredentialManager::OnResetCredentialsCompleted,
                 base::Unretained(this), security_level);

  media_drm_bridge_ = media::MediaDrmBridge::CreateWithoutSessionSupport(
      kWidevineKeySystem, "" /* origin_id */, security_level,
      create_fetcher_cb);

  // No need to reset credentials for unsupported |security_level|.
  if (!media_drm_bridge_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(reset_credentials_cb, true));
    return;
  }

  media_drm_bridge_->ResetDeviceCredentials(reset_credentials_cb);
}
