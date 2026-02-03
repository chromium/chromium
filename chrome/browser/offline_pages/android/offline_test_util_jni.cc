// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_utils.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "chrome/android/test_support_jni_headers/OfflineTestUtil_jni.h"
#include "chrome/browser/android/profile_key_util.h"
#include "chrome/browser/offline_pages/android/offline_page_bridge.h"
#include "chrome/browser/offline_pages/android/request_coordinator_bridge.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/url_loader_interceptor.h"

// Below is the native implementation of OfflineTestUtil.java.

namespace offline_pages {
namespace {
using ::base::android::JavaRef;
using ::base::android::ScopedJavaGlobalRef;
using ::base::android::ScopedJavaLocalRef;
using ::offline_pages::android::OfflinePageBridge;

Profile* GetProfile() {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  DCHECK(profile);
  return profile;
}
RequestCoordinator* GetRequestCoordinator() {
  return RequestCoordinatorFactory::GetForBrowserContext(GetProfile());
}
OfflinePageModel* GetOfflinePageModel() {
  return OfflinePageModelFactory::GetForKey(
      ::android::GetLastUsedRegularProfileKey());
}

void OnGetAllRequestsDone(
    const ScopedJavaGlobalRef<jobject>& j_callback_obj,
    std::vector<std::unique_ptr<SavePageRequest>> all_requests) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::RunObjectCallbackAndroid(
      j_callback_obj, offline_pages::android::CreateJavaSavePageRequests(
                          env, std::move(all_requests)));
}

void OnGetAllPagesDone(
    const ScopedJavaGlobalRef<jobject>& j_result_obj,
    const ScopedJavaGlobalRef<jobject>& j_callback_obj,
    const OfflinePageModel::MultipleOfflinePageItemResult& result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  OfflinePageBridge::AddOfflinePageItemsToJavaList(env, j_result_obj, result);
  base::android::RunObjectCallbackAndroid(j_callback_obj, j_result_obj);
}

void OnGetVisualsDoneExtractThumbnail(
    const ScopedJavaGlobalRef<jobject>& j_callback_obj,
    std::unique_ptr<OfflinePageVisuals> visuals) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_bytes =
      base::android::ToJavaByteArray(env, visuals->thumbnail);
  base::android::RunObjectCallbackAndroid(j_callback_obj, j_bytes);
}

std::string RequestListToString(
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  std::stringstream ss;
  ss << "[\n";
  for (std::unique_ptr<SavePageRequest>& request : requests) {
    ss << " " << request->ToString() << "\n";
  }
  ss << "\n]";
  return ss.str();
}

void DumpRequestCoordinatorState(
    base::OnceCallback<void(std::string)> callback) {
  auto convert_and_return =
      [](base::OnceCallback<void(std::string)> callback,
         std::vector<std::unique_ptr<SavePageRequest>> requests) {
        std::move(callback).Run(RequestListToString(std::move(requests)));
      };
  GetRequestCoordinator()->GetAllRequests(
      base::BindOnce(convert_and_return, std::move(callback)));
}

class Interceptor {
 public:
  void InterceptWithOfflineError(const GURL& url, base::OnceClosure callback) {
    interceptors_.push_back(
        content::URLLoaderInterceptor::SetupRequestFailForURL(
            url, net::Error::ERR_INTERNET_DISCONNECTED, std::move(callback)));
  }

 private:
  std::vector<std::unique_ptr<content::URLLoaderInterceptor>> interceptors_;
};

// This is a raw pointer because global destructors are disallowed.
Interceptor* g_interceptor = nullptr;

// Waits for the connection type to change to a desired value.
class NetworkConnectionObserver
    : public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  // Waits for connection type to change to |type|.
  static void WaitForConnectionType(
      net::NetworkChangeNotifier::ConnectionType type_to_wait_for,
      base::OnceClosure&& callback) {
    // NetworkConnectionObserver manages it's own lifetime.
    new NetworkConnectionObserver(type_to_wait_for, std::move(callback));
  }

 private:
  NetworkConnectionObserver(
      net::NetworkChangeNotifier::ConnectionType type_to_wait_for,
                            base::OnceClosure&& callback)
      : type_to_wait_for_(type_to_wait_for), callback_(std::move(callback)) {
    content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);

    // Call OnConnectionChanged() with the current state.
    net::NetworkChangeNotifier::ConnectionType current_type;
    if (content::GetNetworkConnectionTracker()->GetConnectionType(
            &current_type,
            base::BindOnce(&NetworkConnectionObserver::OnConnectionChanged,
                           weak_factory_.GetWeakPtr()))) {
      OnConnectionChanged(current_type);
    }
  }

  ~NetworkConnectionObserver() override {
    content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(
        this);
  }

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(
      net::NetworkChangeNotifier::ConnectionType type) override {
    if (type == type_to_wait_for_) {
      std::move(callback_).Run();
      delete this;
    }
  }

  net::NetworkChangeNotifier::ConnectionType type_to_wait_for_ =
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN;
  base::OnceClosure callback_;
  base::WeakPtrFactory<NetworkConnectionObserver> weak_factory_{this};
};

}  // namespace

static void JNI_OfflineTestUtil_GetRequestsInQueue(
    JNIEnv* env,
    const JavaRef<jobject>& j_callback_obj) {
  ScopedJavaGlobalRef<jobject> j_callback_ref(j_callback_obj);

  RequestCoordinator* coordinator = GetRequestCoordinator();

  if (!coordinator) {
    // Callback with null to signal that results are unavailable.
    const JavaRef<jobject> empty_result(nullptr);
    base::android::RunObjectCallbackAndroid(j_callback_obj, empty_result);
    return;
  }

  coordinator->GetAllRequests(
      base::BindOnce(&OnGetAllRequestsDone, std::move(j_callback_ref)));
}

static void JNI_OfflineTestUtil_GetAllPages(
    JNIEnv* env,
    const JavaRef<jobject>& j_result_obj,
    const JavaRef<jobject>& j_callback_obj) {
  DCHECK(j_result_obj);
  DCHECK(j_callback_obj);

  ScopedJavaGlobalRef<jobject> j_result_ref(env, j_result_obj);
  ScopedJavaGlobalRef<jobject> j_callback_ref(env, j_callback_obj);
  GetOfflinePageModel()->GetAllPages(base::BindOnce(
      &OnGetAllPagesDone, std::move(j_result_ref), std::move(j_callback_ref)));
}

static void JNI_OfflineTestUtil_GetRawThumbnail(
    JNIEnv* env,
    int64_t j_offline_id,
    const JavaRef<jobject>& j_callback_obj) {
  DCHECK(j_offline_id);

  GetOfflinePageModel()->GetVisualsByOfflineId(
      j_offline_id,
      base::BindOnce(&OnGetVisualsDoneExtractThumbnail,
                     ScopedJavaGlobalRef<jobject>(j_callback_obj)));
}

static JNI_EXPORT void JNI_OfflineTestUtil_StartRequestCoordinatorProcessing(
    JNIEnv* env) {
  GetRequestCoordinator()->StartImmediateProcessing(base::DoNothing());
}

static void JNI_OfflineTestUtil_InterceptWithOfflineError(
    const std::string& url,
    base::OnceClosure&& ready_callback) {
  if (!g_interceptor)
    g_interceptor = new Interceptor;
  g_interceptor->InterceptWithOfflineError(GURL(url),
                                           std::move(ready_callback));
}

static void JNI_OfflineTestUtil_ClearIntercepts() {
  delete g_interceptor;
  g_interceptor = nullptr;
}

static void JNI_OfflineTestUtil_DumpRequestCoordinatorState(
    JNIEnv* env,
    const JavaRef<jobject>& j_callback) {
  auto wrap_callback = base::BindOnce(
      [](base::android::ScopedJavaGlobalRef<jobject> j_callback,
         std::string dump) {
        JNIEnv* env = base::android::AttachCurrentThread();
        base::android::RunObjectCallbackAndroid(
            j_callback, base::android::ConvertUTF8ToJavaString(env, dump));
      },
      base::android::ScopedJavaGlobalRef<jobject>(env, j_callback));
  DumpRequestCoordinatorState(std::move(wrap_callback));
}

static void JNI_OfflineTestUtil_WaitForConnectivityState(
    bool connected,
    base::OnceClosure&& callback) {
  net::NetworkChangeNotifier::ConnectionType type =
      connected ? net::NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN
                : net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE;
  NetworkConnectionObserver::WaitForConnectionType(type, std::move(callback));
}

}  // namespace offline_pages

DEFINE_JNI(OfflineTestUtil)
