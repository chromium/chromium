// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_service_proxy_android.h"

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/time/time.h"
#include "chrome/android/chrome_jni_headers/SharingServiceProxy_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_device_source.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/browser/sharing/sharing_send_message_result.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "components/sync_device_info/device_info.h"

void JNI_SharingServiceProxy_InitSharingService(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_profile) {
  SharingService* service = SharingServiceFactory::GetForBrowserContext(
      ProfileAndroid::FromProfileAndroid(j_profile));
  DCHECK(service);
}

SharingServiceProxyAndroid::SharingServiceProxyAndroid(
    SharingService* sharing_service)
    : sharing_service_(sharing_service) {
  DCHECK(sharing_service_);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SharingServiceProxy_onProxyCreated(env,
                                          reinterpret_cast<intptr_t>(this));
}

SharingServiceProxyAndroid::~SharingServiceProxyAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SharingServiceProxy_onProxyDestroyed(env);
}

void SharingServiceProxyAndroid::SendSharedClipboardMessage(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& j_guid,
    const base::android::JavaParamRef<jstring>& j_text,
    const base::android::JavaParamRef<jobject>& j_runnable) {
  auto callback =
      base::BindOnce(base::android::RunIntCallbackAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(j_runnable));

  std::string guid = base::android::ConvertJavaStringToUTF8(env, j_guid);
  DCHECK(!guid.empty());

  std::unique_ptr<syncer::DeviceInfo> device =
      sharing_service_->GetDeviceByGuid(guid);

  if (!device) {
    std::move(callback).Run(
        static_cast<int32_t>(SharingSendMessageResult::kDeviceNotFound));
    return;
  }

  std::string text = base::android::ConvertJavaStringToUTF8(env, j_text);
  chrome_browser_sharing::SharingMessage sharing_message;
  sharing_message.mutable_shared_clipboard_message()->set_text(std::move(text));

  sharing_service_->SendMessageToDevice(
      *device, kSharingMessageTTL, std::move(sharing_message),
      base::BindOnce(
          [](base::OnceCallback<void(int)> callback,
             SharingSendMessageResult result,
             std::unique_ptr<chrome_browser_sharing::ResponseMessage>
                 response) {
            std::move(callback).Run(static_cast<int>(result));
          },
          std::move(callback)));
}

void SharingServiceProxyAndroid::GetDeviceCandidates(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_device_info,
    jint j_required_feature) {
  auto device_candidates = sharing_service_->GetDeviceCandidates(
      static_cast<sync_pb::SharingSpecificFields::EnabledFeatures>(
          j_required_feature));
  for (const auto& device_info : device_candidates) {
    Java_SharingServiceProxy_createDeviceInfoAndAppendToList(
        env, j_device_info,
        base::android::ConvertUTF8ToJavaString(env, device_info->guid()),
        base::android::ConvertUTF8ToJavaString(env, device_info->client_name()),
        static_cast<int>(device_info->form_factor()),
        device_info->last_updated_timestamp().InMillisecondsSinceUnixEpoch());
  }
}

void SharingServiceProxyAndroid::AddDeviceCandidatesInitializedObserver(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_runnable) {
  sharing_service_->GetDeviceSource()->AddReadyCallback(
      base::BindOnce(base::android::RunRunnableAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(j_runnable)));
}
