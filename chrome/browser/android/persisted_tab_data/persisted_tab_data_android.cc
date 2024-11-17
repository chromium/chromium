// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/persisted_tab_data/persisted_tab_data_android.h"

#include "base/check_deref.h"
#include "base/no_destructor.h"
#include "chrome/browser/android/persisted_tab_data/persisted_tab_data_config_android.h"
#include "chrome/browser/android/persisted_tab_data/persisted_tab_data_storage_android.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "content/public/browser/browser_thread.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/tab/jni_headers/PersistedTabData_jni.h"

namespace {

std::string GetCachedCallbackKey(const TabAndroid* tab_android,
                                 const void* user_data_key) {
  const char* data_id =
      PersistedTabDataConfigAndroid::Get(user_data_key, tab_android->profile())
          ->data_id();
  return base::StringPrintf("%d-%s", tab_android->GetAndroidId(), data_id);
}

}  // namespace

struct PersistedTabDataAndroidDeferredRequest {
  base::WeakPtr<TabAndroid> tab_android;
  raw_ptr<const void> user_data_key;
  PersistedTabDataAndroid::SupplierCallback supplier_callback;
  PersistedTabDataAndroid::FromCallback from_callback;
};

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
    GetDeferredRequests().push_back({
        .tab_android = std::move(tab_android),
        .user_data_key = user_data_key,
        .supplier_callback = std::move(supplier_callback),
        .from_callback = std::move(from_callback),
    });
    return;
  }

  if (tab_android->GetUserData(user_data_key)) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<TabAndroid> tab_android, const void* user_data_key,
               FromCallback from_callback) {
              // Note: the simple and obvious thing would be to simply post a
              // task with a pointer to the `PersistedTabDataAndroid` here.
              // Unfortunately, that may result in a dangling pointer if the
              // `TabAndroid` is destroyed after the check above, but before the
              // task runs. Instead, post a task and repeat the lookup if
              // `TabAndroid` is still live.
              //
              // In addition, treat it as a programmer error if something
              // removes the user data for `user_data_key` in between the first
              // check and the second lookup. This won't catch ABA errors, but
              // it's better than nothing...
              auto* data =
                  tab_android
                      ? static_cast<PersistedTabDataAndroid*>(&CHECK_DEREF(
                            tab_android->GetUserData(user_data_key)))
                      : nullptr;
              std::move(from_callback).Run(data);
            },
            std::move(tab_android), user_data_key, std::move(from_callback)));

    return;
  }

  std::unique_ptr<PersistedTabDataConfigAndroid>
      persisted_tab_data_config_android = PersistedTabDataConfigAndroid::Get(
          user_data_key, tab_android->profile());
  std::string cached_callback_key =
      GetCachedCallbackKey(tab_android.get(), user_data_key);
  std::vector<FromCallback>& callbacks =
      PersistedTabDataAndroid::GetCachedCallbackMap()[cached_callback_key];
  callbacks.push_back(std::move(from_callback));
  // A restore is already in-flight, so just wait for it to complete.
  if (callbacks.size() > 1) {
    return;
  }

  persisted_tab_data_config_android->persisted_tab_data_storage_android()
      ->Restore(
          tab_android->GetAndroidId(),
          persisted_tab_data_config_android->data_id(),
          base::BindOnce(
              [](base::WeakPtr<TabAndroid> tab_android,
                 SupplierCallback supplier_callback, const void* user_data_key,
                 const std::vector<uint8_t>& data) {
                DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
                if (!tab_android) {
                  return;
                }

                tab_android->SetUserData(
                    user_data_key,
                    std::move(supplier_callback).Run(tab_android.get()));
                PersistedTabDataAndroid* persisted_tab_data_android =
                    static_cast<PersistedTabDataAndroid*>(
                        tab_android->GetUserData(user_data_key));
                if (data.empty()) {
                  // No PersistedTabData found - use default result of the
                  // supplier (no deserialization) and save for use across
                  // restarts.
                  persisted_tab_data_android->Save();
                } else {
                  persisted_tab_data_android->Deserialize(data);
                }

                persisted_tab_data_android->RunCallbackOnUIThread(
                    tab_android.get(), user_data_key);
              },
              tab_android->GetWeakPtr(), std::move(supplier_callback),
              user_data_key));
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
  base::circular_deque<PersistedTabDataAndroidDeferredRequest>&
      deferred_requests = GetDeferredRequests();
  if (deferred_requests.empty()) {
    return;
  }
  PersistedTabDataAndroidDeferredRequest deferred_request =
      std::move(deferred_requests.front());
  deferred_requests.pop_front();
  if (!deferred_request.tab_android) {
    // Recursively clear rest of the DeferredRequest queue.
    PersistedTabDataAndroid::OnDeferredStartup();
    return;
  }
  // Process deferred requests one at a time (to minimize risk of
  // resource over-utilization which could lead to jank).
  PersistedTabDataAndroid::From(
      deferred_request.tab_android, deferred_request.user_data_key,
      std::move(deferred_request.supplier_callback),
      base::BindOnce(
          [](FromCallback from_callback,
             PersistedTabDataAndroid* persisted_tab_data_android) {
            // Callbacks should have been posted to the UI thread.
            DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
            std::move(from_callback).Run(persisted_tab_data_android);

            // Recursive call to clear rest of queue (if it's non-empty).
            PersistedTabDataAndroid::OnDeferredStartup();
          },
          std::move(deferred_request.from_callback)));
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

std::map<std::string, std::vector<PersistedTabDataAndroid::FromCallback>>&
PersistedTabDataAndroid::GetCachedCallbackMap() {
  static base::NoDestructor<
      std::map<std::string, std::vector<PersistedTabDataAndroid::FromCallback>>>
      cached_callback_map;
  return *cached_callback_map;
}

void PersistedTabDataAndroid::RunCallbackOnUIThread(
    const TabAndroid* tab_android,
    const void* user_data_key) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(tab_android);
  std::string cached_callback_key =
      GetCachedCallbackKey(tab_android, user_data_key);
  auto node_handle = PersistedTabDataAndroid::GetCachedCallbackMap().extract(
      cached_callback_key);
  CHECK(node_handle);
  for (auto& callback : node_handle.mapped()) {
    std::move(callback).Run(this);
  }
}

base::circular_deque<PersistedTabDataAndroidDeferredRequest>&
PersistedTabDataAndroid::GetDeferredRequests() {
  static base::NoDestructor<
      base::circular_deque<PersistedTabDataAndroidDeferredRequest>>
      deferred_requests;
  return *deferred_requests;
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
