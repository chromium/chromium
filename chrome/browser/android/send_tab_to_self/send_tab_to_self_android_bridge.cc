// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/send_tab_to_self/android_notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service_factory.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "components/send_tab_to_self/entry_point_display_reason.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/proto_conversions.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/sync/protocol/send_tab_to_self_specifics.pb.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/PageContext_jni.h"
#include "chrome/android/chrome_jni_headers/SendTabToSelfAndroidBridge_jni.h"
#include "chrome/android/chrome_jni_headers/TargetDeviceInfo_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace jni_zero {

template <>
send_tab_to_self::PageContext FromJniType<send_tab_to_self::PageContext>(
    JNIEnv* env,
    const JavaRef<jobject>& j_page_context) {
  if (j_page_context.is_null()) {
    return send_tab_to_self::PageContext();
  }

  ScopedJavaLocalRef<jbyteArray> j_bytes =
      Java_PageContext_getSerializedProto(env, j_page_context);
  std::string serialized_proto;
  base::android::JavaByteArrayToString(env, j_bytes, &serialized_proto);

  sync_pb::PageContext pb_page_context;
  if (!pb_page_context.ParseFromString(serialized_proto)) {
    return send_tab_to_self::PageContext();
  }

  return send_tab_to_self::PageContextFromProto(pb_page_context);
}

template <>
ScopedJavaLocalRef<jobject> ToJniType<send_tab_to_self::PageContext>(
    JNIEnv* env,
    const send_tab_to_self::PageContext& page_context) {
  sync_pb::PageContext pb_page_context =
      send_tab_to_self::PageContextToProto(page_context);
  std::string serialized_proto;
  if (!pb_page_context.SerializeToString(&serialized_proto)) {
    return nullptr;
  }

  ScopedJavaLocalRef<jbyteArray> j_bytes =
      base::android::ToJavaByteArray(env, serialized_proto);
  return Java_PageContext_Constructor(env, j_bytes);
}

}  // namespace jni_zero

// The delegate to fetch SendTabToSelf information and persist new
// SendTabToSelf entries. The functions are called by the SendTabToSelf Java
// counterpart.
namespace send_tab_to_self {

static std::vector<ScopedJavaLocalRef<jobject>>
JNI_SendTabToSelfAndroidBridge_GetAllTargetDeviceInfos(JNIEnv* env,
                                                       Profile* profile) {
  std::vector<ScopedJavaLocalRef<jobject>> infos;
  SendTabToSelfModel* model =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile)
          ->GetSendTabToSelfModel();
  if (model->IsReady()) {
    for (const TargetDeviceInfo& info :
         model->GetTargetDeviceInfoSortedList()) {
      infos.push_back(Java_TargetDeviceInfo_build(
          env, ConvertUTF8ToJavaString(env, info.device_name),
          ConvertUTF8ToJavaString(env, info.cache_guid),
          static_cast<int>(info.form_factor),
          info.last_updated_timestamp.InMillisecondsSinceUnixEpoch()));
    }
  }

  return infos;
}

// Adds a new entry with the specified parameters. Returns whether the
// the persistent entry in the bridge was created.
static bool JNI_SendTabToSelfAndroidBridge_AddEntry(
    JNIEnv* env,
    Profile* profile,
    const JavaRef<jstring>& j_url,
    const JavaRef<jstring>& j_title,
    const JavaRef<jstring>& j_target_device_sync_cache_guid,
    PageContext page_context) {
  const std::string url = ConvertJavaStringToUTF8(env, j_url);
  const std::string title = ConvertJavaStringToUTF8(env, j_title);
  const std::string target_device_sync_cache_guid =
      ConvertJavaStringToUTF8(env, j_target_device_sync_cache_guid);

  SendTabToSelfModel* model =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile)
          ->GetSendTabToSelfModel();
  return model->IsReady() &&
         model->AddEntry(GURL(url), title, target_device_sync_cache_guid,
                         page_context);
}

static PageContext JNI_SendTabToSelfAndroidBridge_CreatePageContext(
    JNIEnv* env,
    const JavaRef<jobject>& j_web_contents) {
  if (!base::FeatureList::IsEnabled(kSendTabToSelfPropagateFormFields)) {
    return PageContext();
  }

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  if (!web_contents) {
    return PageContext();
  }

  return ExtractFormFieldsFromWebContents(web_contents);
}

// Deletes the entry associated with the passed in GUID.
static void JNI_SendTabToSelfAndroidBridge_DeleteEntry(
    JNIEnv* env,
    Profile* profile,
    const JavaRef<jstring>& j_guid) {
  SendTabToSelfModel* model =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile)
          ->GetSendTabToSelfModel();
  if (model->IsReady()) {
    const std::string guid = ConvertJavaStringToUTF8(env, j_guid);
    model->DeleteEntry(guid);
  }
}

// Marks the entry with the associated GUID as dismissed.
static void JNI_SendTabToSelfAndroidBridge_DismissEntry(
    JNIEnv* env,
    Profile* profile,
    const JavaRef<jstring>& j_guid) {
  SendTabToSelfModel* model =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile)
          ->GetSendTabToSelfModel();
  if (model->IsReady()) {
    const std::string guid = ConvertJavaStringToUTF8(env, j_guid);
    model->DismissEntry(guid);
  }
}

static void JNI_SendTabToSelfAndroidBridge_UpdateActiveWebContents(
    JNIEnv* env,
    const JavaRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!web_contents->GetBrowserContext()->IsOffTheRecord()) {
    static_cast<AndroidNotificationHandler*>(
        SendTabToSelfClientServiceFactory::GetForProfile(profile)
            ->GetReceivingUiHandler())
        ->UpdateWebContents(web_contents);
  }
}

static ScopedJavaLocalRef<jobject>
JNI_SendTabToSelfAndroidBridge_GetEntryPointDisplayReason(
    JNIEnv* env,
    Profile* profile,
    const JavaRef<jstring>& j_url_to_share) {
  send_tab_to_self::SendTabToSelfSyncService* service =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile);
  std::optional<send_tab_to_self::EntryPointDisplayReason> reason =
      service ? service->GetEntryPointDisplayReason(
                    GURL(ConvertJavaStringToUTF8(env, j_url_to_share)))
              : std::nullopt;

  if (!reason) {
    return nullptr;
  }

  // Wrap the content in a java.lang.Integer, so it can be nullable.
  // TODO(crbug.com/40772220): Having an empty optional/null to represent the
  // hidden entry point doesn't seem worth it after all. Make that just another
  // value in the enum, sparing the complexity here.
  return jni_zero::ToJavaInteger(env, static_cast<int32_t>(*reason));
}

}  // namespace send_tab_to_self

DEFINE_JNI(SendTabToSelfAndroidBridge)
DEFINE_JNI(TargetDeviceInfo)
