// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/send_tab_to_self/send_tab_to_self_android_bridge.h"

#include <string>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/android/send_tab_to_self/android_notification_handler.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service_factory.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_page_handler.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "components/send_tab_to_self/entry_point_display_reason.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/sync/protocol/send_tab_to_self_specifics.pb.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/SendTabToSelfAndroidBridge_jni.h"
#include "chrome/android/chrome_jni_headers/TargetDeviceInfo_jni.h"
#include "chrome/browser/tab/jni_headers/SendTabToSelfTabCardLabelData_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

// The delegate to fetch SendTabToSelf information and persist new
// SendTabToSelf entries. The functions are called by the SendTabToSelf Java
// counterpart.
namespace send_tab_to_self {

class DeviceInfoObserverBridge : public syncer::DeviceInfoTracker::Observer,
                                 public ProfileObserver {
 public:
  DeviceInfoObserverBridge(JNIEnv* env,
                           const JavaRef<jobject>& j_observer,
                           Profile* profile,
                           syncer::DeviceInfoTracker* tracker)
      : j_observer_(env, j_observer) {
    observation_.Observe(tracker);
    profile_observation_.Observe(profile);
  }
  ~DeviceInfoObserverBridge() override = default;

  // syncer::DeviceInfoTracker::Observer:
  void OnDeviceInfoChange() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_DeviceInfoObserver_onDeviceInfoChanged(env, j_observer_);
  }

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override {
    // It's not strictly guaranteed that the Java side (which owns this object)
    // will destroy it before the Profile and its KeyedServices are shut down.
    // So to prevent any dangling pointers, handle Profile shutdown here.
    observation_.Reset();
    profile_observation_.Reset();
  }

 private:
  ScopedJavaGlobalRef<jobject> j_observer_;
  base::ScopedObservation<syncer::DeviceInfoTracker,
                          syncer::DeviceInfoTracker::Observer>
      observation_{this};
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
};

class SendTabToSelfModelObserverBridge : public SendTabToSelfModelObserver,
                                         public ProfileObserver {
 public:
  SendTabToSelfModelObserverBridge(JNIEnv* env,
                                   const JavaRef<jobject>& j_observer,
                                   Profile* profile,
                                   SendTabToSelfModel* model)
      : j_observer_(env, j_observer) {
    observation_.Observe(model);
    profile_observation_.Observe(profile);
  }
  ~SendTabToSelfModelObserverBridge() override = default;

  // SendTabToSelfModelObserver:
  void OnModelReady() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_SendTabToSelfModelObserver_onModelReady(env, j_observer_);
  }

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override {
    // It's not strictly guaranteed that the Java side (which owns this object)
    // will destroy it before the Profile and its KeyedServices are shut down.
    // So to prevent any dangling pointers, handle Profile shutdown here.
    observation_.Reset();
    profile_observation_.Reset();
  }

 private:
  ScopedJavaGlobalRef<jobject> j_observer_;
  base::ScopedObservation<SendTabToSelfModel, SendTabToSelfModelObserver>
      observation_{this};
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
};

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
    const JavaRef<jobject>& j_callback,
    int32_t j_entry_point) {
  const std::string target_device_sync_cache_guid =
      ConvertJavaStringToUTF8(env, j_target_device_sync_cache_guid);
  const std::string url = ConvertJavaStringToUTF8(env, j_url);
  const std::string title = ConvertJavaStringToUTF8(env, j_title);
  const ShareEntryPoint entry_point =
      static_cast<ShareEntryPoint>(j_entry_point);

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
                          std::move(commit_confirmation), entry_point);
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
                   std::move(commit_confirmation), entry_point);
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

static void JNI_SendTabToSelfAndroidBridge_MarkEntryActivated(
    JNIEnv* env,
    Profile* profile,
    const JavaRef<jstring>& j_guid,
    jint j_entry_point) {
  auto* service = SendTabToSelfSyncServiceFactory::GetForProfile(profile);
  SendTabToSelfModel* model =
      service ? service->GetSendTabToSelfModel() : nullptr;
  if (model) {
    const std::string guid = ConvertJavaStringToUTF8(env, j_guid);
    model->MarkEntryActivated(
        guid, static_cast<ShareActivatedEntryPoint>(j_entry_point));
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

static void JNI_SendTabToSelfAndroidBridge_RecordTargetDeviceCount(
    JNIEnv* env,
    jint j_entry_point,
    jint j_display_reason,
    jint j_device_count) {
  CHECK_LE(0, j_entry_point);
  CHECK_LE(j_entry_point, static_cast<jint>(ShareEntryPoint::kMaxValue));
  CHECK_LE(0, j_display_reason);
  CHECK_LE(j_display_reason,
           static_cast<jint>(EntryPointDisplayReason::kMaxValue));
  RecordTargetDeviceCount(
      static_cast<ShareEntryPoint>(j_entry_point),
      static_cast<EntryPointDisplayReason>(j_display_reason),
      static_cast<size_t>(j_device_count));
}

static void JNI_SendTabToSelfTabCardLabelData_MarkEntryActivated(
    JNIEnv* env,
    Profile* profile,
    const JavaRef<jstring>& j_guid,
    jint j_entry_point) {
  auto* service = SendTabToSelfSyncServiceFactory::GetForProfile(profile);
  SendTabToSelfModel* model =
      service ? service->GetSendTabToSelfModel() : nullptr;
  if (model) {
    const std::string guid = ConvertJavaStringToUTF8(env, j_guid);
    model->MarkEntryActivated(
        guid, static_cast<ShareActivatedEntryPoint>(j_entry_point));
  }
}

void AttachTabLabel(TabAndroid* tab,
                    std::string_view guid,
                    std::string_view device_name) {
  CHECK(tab);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SendTabToSelfAndroidBridge_attachTabLabel(
      env, tab->GetJavaObject(), ConvertUTF8ToJavaString(env, guid),
      ConvertUTF8ToJavaString(env, device_name));
}

void ShowMessageBanner(content::WebContents* web_contents,
                       std::string_view device_name,
                       int opened_tab_count) {
  JNIEnv* const env = base::android::AttachCurrentThread();
  Java_SendTabToSelfAndroidBridge_showMessageBanner(
      env, web_contents->GetJavaWebContents(),
      base::android::ConvertUTF8ToJavaString(env, device_name),
      opened_tab_count);
}

static int64_t JNI_SendTabToSelfAndroidBridge_AddDeviceInfoObserver(
    JNIEnv* env,
    Profile* profile,
    const JavaRef<jobject>& j_observer) {
  syncer::DeviceInfoTracker* tracker =
      DeviceInfoSyncServiceFactory::GetForProfile(profile)
          ->GetDeviceInfoTracker();
  return reinterpret_cast<int64_t>(
      new DeviceInfoObserverBridge(env, j_observer, profile, tracker));
}

static void JNI_SendTabToSelfAndroidBridge_RemoveDeviceInfoObserver(
    JNIEnv* env,
    int64_t observer_ptr) {
  delete reinterpret_cast<DeviceInfoObserverBridge*>(observer_ptr);
}

static int64_t JNI_SendTabToSelfAndroidBridge_AddModelObserver(
    JNIEnv* env,
    Profile* profile,
    const JavaRef<jobject>& j_observer) {
  SendTabToSelfModel* model =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile)
          ->GetSendTabToSelfModel();
  return reinterpret_cast<int64_t>(
      new SendTabToSelfModelObserverBridge(env, j_observer, profile, model));
}

static void JNI_SendTabToSelfAndroidBridge_RemoveModelObserver(
    JNIEnv* env,
    int64_t observer_ptr) {
  delete reinterpret_cast<SendTabToSelfModelObserverBridge*>(observer_ptr);
}

}  // namespace send_tab_to_self

DEFINE_JNI(SendTabToSelfAndroidBridge)
DEFINE_JNI(TargetDeviceInfo)
DEFINE_JNI(SendTabToSelfTabCardLabelData)
