// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "chrome/browser/android/send_tab_to_self/android_notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service_factory.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_page_handler.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "components/send_tab_to_self/entry_point_display_reason.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/sync/protocol/send_tab_to_self_specifics.pb.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/SendTabToSelfAndroidBridge_jni.h"
#include "chrome/android/chrome_jni_headers/TargetDeviceInfo_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

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
          base::android::ConvertUTF16ToJavaString(
              env, info.GetLastActiveTimeForDisplay())));
    }
  }
  return infos;
}

static void JNI_SendTabToSelfAndroidBridge_SendTabToDevice(
    JNIEnv* env,
    Profile* profile,
    const JavaRef<jobject>& j_web_contents,
    const JavaRef<jstring>& j_target_device_sync_cache_guid,
    const JavaRef<jstring>& j_url,
    const JavaRef<jstring>& j_title,
    const JavaRef<jobject>& j_callback) {
  const std::string target_device_sync_cache_guid =
      ConvertJavaStringToUTF8(env, j_target_device_sync_cache_guid);
  const std::string url = ConvertJavaStringToUTF8(env, j_url);
  const std::string title = ConvertJavaStringToUTF8(env, j_title);

  // TODO(crbug.com/492072882) Consider adding a `CHECK` once Android is updated
  // to always provide the callback.
  base::OnceCallback<void(SendTabToSelfResult)> commit_confirmation =
      base::DoNothing();
  if (j_callback) {
    commit_confirmation = base::BindOnce(
        [](const base::android::ScopedJavaGlobalRef<jobject>& j_callback,
           SendTabToSelfResult result) {
          JNIEnv* env = base::android::AttachCurrentThread();
          Java_CommitConfirmationCallback_onResult(env, j_callback,
                                                   static_cast<int>(result));
        },
        base::android::ScopedJavaGlobalRef<jobject>(j_callback));
  }

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  if (web_contents) {
    SendTabToSelfPageHandler::GetOrCreateForWebContents(web_contents)
        ->SendTabToDevice(target_device_sync_cache_guid, GURL(url), title,
                          std::move(commit_confirmation));
    return;
  }

  // WebContents is not available (the caller may not have a tab, e.g.
  // right-click on a link to share). Send the entry without page context
  // (scroll position, form fields, navigation history).
  SendTabToSelfModel* model =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile)
          ->GetSendTabToSelfModel();
  CHECK(model);
  model->SendEntry(GURL(url), title, target_device_sync_cache_guid,
                   PageContext(), NavigationHistory(),
                   std::move(commit_confirmation));
}

// Marks the entry with the associated GUID as opened.
static void JNI_SendTabToSelfAndroidBridge_MarkEntryOpened(
    JNIEnv* env,
    Profile* profile,
    const JavaRef<jstring>& j_guid) {
  SendTabToSelfModel* model =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile)
          ->GetSendTabToSelfModel();
  if (model->IsReady()) {
    const std::string guid = ConvertJavaStringToUTF8(env, j_guid);
    model->MarkEntryOpened(guid);
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
