// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/persisted_tab_data/persisted_tab_data_android.h"

#include "base/no_destructor.h"
#include "chrome/browser/android/persisted_tab_data/persisted_tab_data_config_android.h"
#include "chrome/browser/android/persisted_tab_data/persisted_tab_data_storage_android.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/tab/jni_headers/PersistedTabData_jni.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "content/public/browser/browser_thread.h"

namespace {

std::string GetCachedCallbackKey(TabAndroid* tab_android,
                                 const void* user_data_key) {
  const char* data_id =
      PersistedTabDataConfigAndroid::Get(user_data_key, tab_android->profile())
          ->data_id();
  return base::StringPrintf("%d-%s", tab_android->GetAndroidId(), data_id);
}

}  // namespace

PersistedTabDataAndroid::PersistedTabDataAndroid(TabAndroid* tab_android,
                                                 const void* user_data_key)
    : persisted_tab_data_storage_android_(
          PersistedTabDataConfigAndroid::Get(user_data_key,
                                             tab_android->profile())
              ->persisted_tab_data_storage_android()),
      data_id_(PersistedTabDataConfigAndroid::Get(user_data_key,
                                                  tab_android->profile())
                   ->data_id()),
      tab_id_(tab_android->GetAndroidId()) {}

PersistedTabDataAndroid::~PersistedTabDataAndroid() = default;

void PersistedTabDataAndroid::From(base::WeakPtr<TabAndroid> tab_android,
                                   const void* user_data_key,
                                   SupplierCallback supplier_callback,
                                   FromCallback from_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!tab_android) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(from_callback), nullptr));
    return;
  }

  if (!deferred_startup_complete_) {
    std::unique_ptr<DeferredRequest> deferred_request =
        std::make_unique<DeferredRequest>();
    deferred_request->tab_android = tab_android;
    deferred_request->user_data_key = user_data_key;
    deferred_request->supplier_callback = std::move(supplier_callback);
    deferred_request->from_callback = std::move(from_callback);
    GetDeferredRequests()->push_back(std::move(deferred_request));
    return;
  }

  if (tab_android->GetUserData(user_data_key)) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(from_callback),
                       static_cast<PersistedTabDataAndroid*>(
                           tab_android->GetUserData(user_data_key))));
  } else {
    std::unique_ptr<PersistedTabDataConfigAndroid>
        persisted_tab_data_config_android = PersistedTabDataConfigAndroid::Get(
            user_data_key, tab_android->profile());
    std::string cached_callback_key =
        GetCachedCallbackKey(tab_android.get(), user_data_key);
    if (PersistedTabDataAndroid::GetCachedCallbackMap()->contains(
            cached_callback_key)) {
      std::vector<FromCallback>& callbacks =
          PersistedTabDataAndroid::GetCachedCallbackMap()
              ->find(cached_callback_key)
              ->second;
      callbacks.push_back(std::move(from_callback));
      return;
    } else {
      PersistedTabDataAndroid::GetCachedCallbackMap()->emplace(
          cached_callback_key, std::vector<FromCallback>());
      PersistedTabDataAndroid::GetCachedCallbackMap()
          ->find(cached_callback_key)
          ->second.push_back(std::move(from_callback));
    }

    persisted_tab_data_config_android->persisted_tab_data_storage_android()
        ->Restore(
            tab_android->GetAndroidId(),
            persisted_tab_data_config_android->data_id(),
            base::BindOnce(
                [](base::WeakPtr<TabAndroid> tab_android,
                   SupplierCallback supplier_callback,
                   const void* user_data_key,
                   const std::vector<uint8_t>& data) {
                  if (!tab_android) {
                    return;
                  }

                  tab_android->SetUserData(user_data_key,
                                           std::move(supplier_callback).Run());
                  PersistedTabDataAndroid* persisted_tab_data_android =
                      static_cast<PersistedTabDataAndroid*>(
                          tab_android->GetUserData(user_data_key));
                  if (data.empty()) {
                    // No PersistedTabData found - use default result of the
                    // supplier (no deserialization) and save for use across
                    // restarts.
                    persisted_tab_data_android->Save();
                  } else {
                    // Deserialize PersistedTabData found in storage.
                    content::GetIOThreadTaskRunner({})
                        ->PostTaskAndReplyWithResult(
                            FROM_HERE,
                            base::BindOnce(
                                [](PersistedTabDataAndroid*
                                       persisted_tab_data_android,
                                   const std::vector<uint8_t>& data) {
                                  DCHECK_CURRENTLY_ON(
                                      content::BrowserThread::IO);
                                  persisted_tab_data_android->Deserialize(data);
                                  return persisted_tab_data_android;
                                },
                                persisted_tab_data_android, data),
                            base::BindOnce(
                                &PersistedTabDataAndroid::RunCallbackOnUIThread,
                                tab_android, user_data_key));
                    return;
                  }
                  content::GetUIThreadTaskRunner({})->PostTask(
                      FROM_HERE,
                      base::BindOnce(
                          &PersistedTabDataAndroid::RunCallbackOnUIThread,
                          tab_android, user_data_key,
                          persisted_tab_data_android));
                },
                tab_android->GetWeakPtr(), std::move(supplier_callback),
                user_data_key));
  }
}

void PersistedTabDataAndroid::Save() {
  persisted_tab_data_storage_android_->Save(tab_id_, data_id_,
                                            *Serialize().get());
}

void PersistedTabDataAndroid::Remove() {
  persisted_tab_data_storage_android_->Remove(tab_id_, data_id_);
}

void PersistedTabDataAndroid::RemoveAll(int tab_id, Profile* profile) {
  std::unique_ptr<std::vector<PersistedTabDataStorageAndroid*>> storage =
      PersistedTabDataConfigAndroid::GetAllStorage(profile);
  for (PersistedTabDataStorageAndroid* persisted_tab_data_storage_android :
       *storage) {
    persisted_tab_data_storage_android->RemoveAll(tab_id);
  }
}

void PersistedTabDataAndroid::OnTabClose(TabAndroid* tab_android) {
  // TODO(b/295219049) cleanup orphaned data
  Profile* profile = tab_android->profile();
  if (!profile || profile->IsOffTheRecord()) {
    return;
  }
  PersistedTabDataAndroid::RemoveAll(tab_android->GetAndroidId(), profile);
}

void PersistedTabDataAndroid::OnDeferredStartup() {
  deferred_startup_complete_ = true;
  std::deque<std::unique_ptr<PersistedTabDataAndroid::DeferredRequest>>*
      deferred_requests = GetDeferredRequests();
  if (deferred_requests->empty()) {
    return;
  }
  std::unique_ptr<PersistedTabDataAndroid::DeferredRequest> deferred_request =
      std::move(deferred_requests->front());
  deferred_requests->pop_front();
  if (!deferred_request->tab_android) {
    // Recursively clear rest of the DeferredRequest queue.
    PersistedTabDataAndroid::OnDeferredStartup();
    return;
  }
  // Process deferred requests one at a time (to minimize risk of
  // resource over-utilization which could lead to jank).
  PersistedTabDataAndroid::From(
      deferred_request->tab_android, deferred_request->user_data_key,
      std::move(deferred_request->supplier_callback),
      base::BindOnce(
          [](FromCallback from_callback,
             PersistedTabDataAndroid* persisted_tab_data_android) {
            // Callbacks should have been posted to the UI thread.
            DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
            std::move(from_callback).Run(persisted_tab_data_android);

            // Recursive call to clear rest of queue (if it's non-empty).
            PersistedTabDataAndroid::OnDeferredStartup();
          },
          std::move(deferred_request->from_callback)));
}

void PersistedTabDataAndroid::ExistsForTesting(
    TabAndroid* tab_android,
    const void* user_data_key,
    base::OnceCallback<void(bool)> exists_callback) {
  std::unique_ptr<PersistedTabDataConfigAndroid>
      persisted_tab_data_config_android = PersistedTabDataConfigAndroid::Get(
          user_data_key, tab_android->profile());
  persisted_tab_data_config_android->persisted_tab_data_storage_android()
      ->Restore(tab_android->GetAndroidId(),
                persisted_tab_data_config_android->data_id(),
                base::BindOnce(
                    [](base::OnceCallback<void(bool)> exists_callback,
                       const std::vector<uint8_t>& data) {
                      content::GetUIThreadTaskRunner({})->PostTask(
                          FROM_HERE, base::BindOnce(std::move(exists_callback),
                                                    !data.empty()));
                    },
                    std::move(exists_callback)));
}

std::map<std::string, std::vector<PersistedTabDataAndroid::FromCallback>>*
PersistedTabDataAndroid::GetCachedCallbackMap() {
  static base::NoDestructor<
      std::map<std::string, std::vector<PersistedTabDataAndroid::FromCallback>>>
      cached_callback_map;
  return cached_callback_map.get();
}

void PersistedTabDataAndroid::RunCallbackOnUIThread(
    base::WeakPtr<TabAndroid> tab_android,
    const void* user_data_key,
    PersistedTabDataAndroid* persisted_tab_data_android) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!tab_android) {
    return;
  }

  std::string cached_callback_key =
      GetCachedCallbackKey(tab_android.get(), user_data_key);
  for (auto& callback : PersistedTabDataAndroid::GetCachedCallbackMap()
                            ->find(cached_callback_key)
                            ->second) {
    std::move(callback).Run(persisted_tab_data_android);
  }
  PersistedTabDataAndroid::GetCachedCallbackMap()->erase(cached_callback_key);
}

PersistedTabDataAndroid::DeferredRequest::DeferredRequest() = default;

PersistedTabDataAndroid::DeferredRequest::~DeferredRequest() = default;

std::deque<std::unique_ptr<PersistedTabDataAndroid::DeferredRequest>>*
PersistedTabDataAndroid::GetDeferredRequests() {
  static base::NoDestructor<
      std::deque<std::unique_ptr<PersistedTabDataAndroid::DeferredRequest>>>
      deferred_requests;
  return deferred_requests.get();
}

bool PersistedTabDataAndroid::deferred_startup_complete_ = false;

class PersistedTabDataAndroidHelper {
 private:
  friend void ::JNI_PersistedTabData_OnTabClose(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_tab);
  friend void ::JNI_PersistedTabData_OnDeferredStartup(JNIEnv* env);

  static void OnTabClose(TabAndroid* tab_android) {
    PersistedTabDataAndroid::OnTabClose(tab_android);
  }

  static void OnDeferredStartup() {
    PersistedTabDataAndroid::OnDeferredStartup();
  }
};

static void JNI_PersistedTabData_OnTabClose(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_tab) {
  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, j_tab);
  PersistedTabDataAndroidHelper::OnTabClose(tab_android);
}

static void JNI_PersistedTabData_OnDeferredStartup(JNIEnv* env) {
  PersistedTabDataAndroidHelper::OnDeferredStartup();
}

TAB_ANDROID_USER_DATA_KEY_IMPL(PersistedTabDataAndroid)
