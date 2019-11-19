// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/android/evaluation/offline_page_evaluation_bridge.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "chrome/android/chrome_jni_headers/OfflinePageEvaluationBridge_jni.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/offline_pages/android/background_scheduler_bridge.h"
#include "chrome/browser/offline_pages/android/evaluation/evaluation_test_scheduler.h"
#include "chrome/browser/offline_pages/background_loader_offliner.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/common/chrome_constants.h"
#include "components/offline_pages/core/background/offliner.h"
#include "components/offline_pages/core/background/offliner_policy.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/background/request_notifier.h"
#include "components/offline_pages/core/background/request_queue.h"
#include "components/offline_pages/core/background/request_queue_store.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/downloads/download_notifying_observer.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "content/public/browser/browser_context.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace network {
class NetworkQualityTracker;
}

namespace offline_pages {
namespace android {

namespace {
const char kNativeTag[] = "OPNative";
const base::FilePath::CharType kTestRequestQueueDirname[] =
    FILE_PATH_LITERAL("Offline Pages/test_request_queue");

void JNI_OfflinePageEvaluationBridge_ToJavaOfflinePageList(
    JNIEnv* env,
    const JavaRef<jobject>& j_result_obj,
    const std::vector<OfflinePageItem>& offline_pages) {
  for (const OfflinePageItem& offline_page : offline_pages) {
    Java_OfflinePageEvaluationBridge_createOfflinePageAndAddToList(
        env, j_result_obj,
        ConvertUTF8ToJavaString(env, offline_page.url.spec()),
        offline_page.offline_id,
        ConvertUTF8ToJavaString(env, offline_page.client_id.name_space),
        ConvertUTF8ToJavaString(env, offline_page.client_id.id),
        ConvertUTF16ToJavaString(env, offline_page.title),
        ConvertUTF8ToJavaString(env, offline_page.file_path.value()),
        offline_page.file_size, offline_page.creation_time.ToJavaTime(),
        offline_page.access_count, offline_page.last_access_time.ToJavaTime(),
        ConvertUTF8ToJavaString(env, offline_page.request_origin));
  }
}

ScopedJavaLocalRef<jobject>
JNI_OfflinePageEvaluationBridge_ToJavaSavePageRequest(
    JNIEnv* env,
    const SavePageRequest& request) {
  return Java_OfflinePageEvaluationBridge_createSavePageRequest(
      env, static_cast<int>(request.request_state()), request.request_id(),
      ConvertUTF8ToJavaString(env, request.url().spec()),
      ConvertUTF8ToJavaString(env, request.client_id().name_space),
      ConvertUTF8ToJavaString(env, request.client_id().id));
}

ScopedJavaLocalRef<jobjectArray>
JNI_OfflinePageEvaluationBridge_CreateJavaSavePageRequests(
    JNIEnv* env,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  ScopedJavaLocalRef<jclass> save_page_request_clazz = base::android::GetClass(
      env, "org/chromium/chrome/browser/offlinepages/SavePageRequest");
  jobjectArray joa = env->NewObjectArray(
      requests.size(), save_page_request_clazz.obj(), nullptr);
  base::android::CheckException(env);

  for (size_t i = 0; i < requests.size(); i++) {
    SavePageRequest request = *(requests[i]);
    ScopedJavaLocalRef<jobject> j_save_page_request =
        JNI_OfflinePageEvaluationBridge_ToJavaSavePageRequest(env, request);
    env->SetObjectArrayElement(joa, i, j_save_page_request.obj());
  }

  return ScopedJavaLocalRef<jobjectArray>(env, joa);
}

void GetAllPagesCallback(
    const ScopedJavaGlobalRef<jobject>& j_result_obj,
    const ScopedJavaGlobalRef<jobject>& j_callback_obj,
    const OfflinePageModel::MultipleOfflinePageItemResult& result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  JNI_OfflinePageEvaluationBridge_ToJavaOfflinePageList(env, j_result_obj,
                                                        result);
  base::android::RunObjectCallbackAndroid(j_callback_obj, j_result_obj);
}

void OnGetAllRequestsDone(
    const ScopedJavaGlobalRef<jobject>& j_callback_obj,
    std::vector<std::unique_ptr<SavePageRequest>> all_requests) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jobjectArray> j_result_obj =
      JNI_OfflinePageEvaluationBridge_CreateJavaSavePageRequests(
          env, std::move(all_requests));
  base::android::RunObjectCallbackAndroid(j_callback_obj, j_result_obj);
}

void OnRemoveRequestsDone(const ScopedJavaGlobalRef<jobject>& j_callback_obj,
                          const MultipleItemStatuses& removed_request_results) {
  base::android::RunIntCallbackAndroid(
      j_callback_obj, static_cast<int>(removed_request_results.size()));
}

std::unique_ptr<KeyedService> GetTestingRequestCoordinator(
    content::BrowserContext* context,
    std::unique_ptr<OfflinerPolicy> policy,
    std::unique_ptr<Offliner> offliner) {
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::CreateSequencedTaskRunner({base::MayBlock()});
  Profile* profile = Profile::FromBrowserContext(context);
  base::FilePath queue_store_path =
      profile->GetPath().Append(kTestRequestQueueDirname);

  std::unique_ptr<RequestQueueStore> queue_store(
      new RequestQueueStore(background_task_runner, queue_store_path));
  std::unique_ptr<RequestQueue> queue(new RequestQueue(std::move(queue_store)));
  std::unique_ptr<android::EvaluationTestScheduler> scheduler(
      new android::EvaluationTestScheduler());
  network::NetworkQualityTracker* network_quality_tracker =
      g_browser_process->network_quality_tracker();
  std::unique_ptr<RequestCoordinator> request_coordinator =
      std::make_unique<RequestCoordinator>(
          std::move(policy), std::move(offliner), std::move(queue),
          std::move(scheduler), network_quality_tracker);
  request_coordinator->SetInternalStartProcessingCallbackForTest(
      base::Bind(&android::EvaluationTestScheduler::ImmediateScheduleCallback,
                 base::Unretained(scheduler.get())));

  return std::move(request_coordinator);
}

std::unique_ptr<KeyedService> GetTestBackgroundLoaderRequestCoordinator(
    content::BrowserContext* context) {
  std::unique_ptr<OfflinerPolicy> policy(new OfflinerPolicy());
  std::unique_ptr<Offliner> offliner(new BackgroundLoaderOffliner(
      context, policy.get(),
      OfflinePageModelFactory::GetForBrowserContext(context),
      nullptr));  // no need to connect LoadTerminationListener for harness.
  return GetTestingRequestCoordinator(context, std::move(policy),
                                      std::move(offliner));
}

RequestCoordinator* GetRequestCoordinator(Profile* profile,
                                          bool use_evaluation_scheduler) {
  if (!use_evaluation_scheduler) {
    return RequestCoordinatorFactory::GetForBrowserContext(profile);
  }
  return static_cast<RequestCoordinator*>(
      RequestCoordinatorFactory::GetInstance()->SetTestingFactoryAndUse(
          profile,
          base::BindRepeating(&GetTestBackgroundLoaderRequestCoordinator)));
}

}  // namespace

static jlong JNI_OfflinePageEvaluationBridge_CreateBridgeForProfile(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_profile,
    const jboolean j_use_evaluation_scheduler) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);

  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(profile);

  RequestCoordinator* request_coordinator = GetRequestCoordinator(
      profile, static_cast<bool>(j_use_evaluation_scheduler));

  if (offline_page_model == nullptr || request_coordinator == nullptr)
    return 0;

  OfflinePageEvaluationBridge* bridge = new OfflinePageEvaluationBridge(
      env, obj, profile, offline_page_model, request_coordinator);

  return reinterpret_cast<jlong>(bridge);
}

OfflinePageEvaluationBridge::OfflinePageEvaluationBridge(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    content::BrowserContext* browser_context,
    OfflinePageModel* offline_page_model,
    RequestCoordinator* request_coordinator)
    : weak_java_ref_(env, obj),
      browser_context_(browser_context),
      offline_page_model_(offline_page_model),
      request_coordinator_(request_coordinator) {
  DCHECK(offline_page_model_);
  DCHECK(request_coordinator_);
  NotifyIfDoneLoading();
  offline_page_model_->AddObserver(this);
  request_coordinator_->AddObserver(this);
  offline_page_model_->GetLogger()->SetClient(this);
  request_coordinator_->GetLogger()->SetClient(this);
}

OfflinePageEvaluationBridge::~OfflinePageEvaluationBridge() {}

void OfflinePageEvaluationBridge::Destroy(JNIEnv* env,
                                          const JavaParamRef<jobject>&) {
  offline_page_model_->RemoveObserver(this);
  request_coordinator_->RemoveObserver(this);
  delete this;
}

// Implement OfflinePageModel::Observer
void OfflinePageEvaluationBridge::OfflinePageModelLoaded(
    OfflinePageModel* model) {
  DCHECK_EQ(offline_page_model_, model);
  NotifyIfDoneLoading();
}

void OfflinePageEvaluationBridge::OfflinePageAdded(
    OfflinePageModel* model,
    const OfflinePageItem& added_page) {}

void OfflinePageEvaluationBridge::OfflinePageDeleted(
    const OfflinePageItem& item) {}

// Implement RequestCoordinator::Observer
void OfflinePageEvaluationBridge::OnAdded(const SavePageRequest& request) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_OfflinePageEvaluationBridge_savePageRequestAdded(
      env, obj,
      JNI_OfflinePageEvaluationBridge_ToJavaSavePageRequest(env, request));
}

void OfflinePageEvaluationBridge::OnCompleted(
    const SavePageRequest& request,
    RequestNotifier::BackgroundSavePageResult status) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_OfflinePageEvaluationBridge_savePageRequestCompleted(
      env, obj,
      JNI_OfflinePageEvaluationBridge_ToJavaSavePageRequest(env, request),
      static_cast<int>(status));
}

void OfflinePageEvaluationBridge::OnChanged(const SavePageRequest& request) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_OfflinePageEvaluationBridge_savePageRequestChanged(
      env, obj,
      JNI_OfflinePageEvaluationBridge_ToJavaSavePageRequest(env, request));
}

void OfflinePageEvaluationBridge::OnNetworkProgress(
    const SavePageRequest& request,
    int64_t received_bytes) {}

void OfflinePageEvaluationBridge::CustomLog(const std::string& message) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_OfflinePageEvaluationBridge_log(env, obj,
                                       ConvertUTF8ToJavaString(env, kNativeTag),
                                       ConvertUTF8ToJavaString(env, message));
}

void OfflinePageEvaluationBridge::GetAllPages(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_result_obj,
    const JavaParamRef<jobject>& j_callback_obj) {
  DCHECK(j_result_obj);
  DCHECK(j_callback_obj);

  ScopedJavaGlobalRef<jobject> j_result_ref(j_result_obj);
  ScopedJavaGlobalRef<jobject> j_callback_ref(j_callback_obj);

  offline_page_model_->GetAllPages(
      base::Bind(&GetAllPagesCallback, j_result_ref, j_callback_ref));
}

bool OfflinePageEvaluationBridge::PushRequestProcessing(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_callback_obj) {
  ScopedJavaGlobalRef<jobject> j_callback_ref(j_callback_obj);
  DCHECK(request_coordinator_);
  base::android::RunBooleanCallbackAndroid(j_callback_obj, false);

  return request_coordinator_->StartImmediateProcessing(base::BindRepeating(
      &base::android::RunBooleanCallbackAndroid, j_callback_ref));
}

void OfflinePageEvaluationBridge::SavePageLater(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_url,
    const JavaParamRef<jstring>& j_namespace,
    const JavaParamRef<jstring>& j_client_id,
    jboolean user_requested) {
  offline_pages::ClientId client_id;
  client_id.name_space = ConvertJavaStringToUTF8(env, j_namespace);
  client_id.id = ConvertJavaStringToUTF8(env, j_client_id);

  RequestCoordinator::SavePageLaterParams params;
  params.url = GURL(ConvertJavaStringToUTF8(env, j_url));
  params.client_id = client_id;
  params.user_requested = static_cast<bool>(user_requested);
  request_coordinator_->SavePageLater(params);
}

void OfflinePageEvaluationBridge::GetRequestsInQueue(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_callback_obj) {
  ScopedJavaGlobalRef<jobject> j_callback_ref(j_callback_obj);
  request_coordinator_->GetAllRequests(
      base::Bind(&OnGetAllRequestsDone, j_callback_ref));
}

void OfflinePageEvaluationBridge::RemoveRequestsFromQueue(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jlongArray>& j_request_ids,
    const JavaParamRef<jobject>& j_callback_obj) {
  std::vector<int64_t> request_ids;
  base::android::JavaLongArrayToInt64Vector(env, j_request_ids, &request_ids);
  ScopedJavaGlobalRef<jobject> j_callback_ref(j_callback_obj);
  request_coordinator_->RemoveRequests(
      request_ids, base::Bind(&OnRemoveRequestsDone, j_callback_ref));
}

void OfflinePageEvaluationBridge::NotifyIfDoneLoading() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_OfflinePageEvaluationBridge_offlinePageModelLoaded(env, obj);
}

}  // namespace android
}  // namespace offline_pages
