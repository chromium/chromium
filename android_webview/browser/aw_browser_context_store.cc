// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_context_store.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/common/aw_features.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/spare_render_process_host_manager.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwBrowserContextStore_jni.h"

namespace android_webview {

namespace {

constexpr char kProfileNameKey[] = "name";
constexpr char kProfilePathKey[] = "path";

bool g_initialized = false;

const base::FeatureParam<bool> kCreateSpareRendererForDefaultIfMultiProfile{
    &features::kCreateSpareRendererOnBrowserContextCreation,
    "create_spare_renderer_for_default_if_multi_profile", false};

}  // namespace

AwBrowserContextStore::AwBrowserContextStore(PrefService* pref_service)
    : prefs_(*pref_service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  TRACE_EVENT0("startup", "AwBrowserContextStore::AwBrowserContextStore");

  ScopedListPrefUpdate update(&*prefs_, prefs::kProfileListPref);
  base::Value::List& profiles = update.Get();
  for (const auto& profile : profiles) {
    const base::Value::Dict& profile_dict = profile.GetDict();
    const std::string* name = profile_dict.FindString(kProfileNameKey);
    CHECK(name);
    const std::string* path_string = profile_dict.FindString(kProfilePathKey);
    CHECK(path_string);
    CHECK(!path_string->empty());
    CHECK_NE(*path_string, base::FilePath::kCurrentDirectory);
    CHECK_NE(*path_string, base::FilePath::kParentDirectory);
    CHECK_EQ(path_string->find_first_of(base::FilePath::kSeparators),
             std::string::npos);
    base::FilePath path = base::FilePath(*path_string);
    const bool name_is_new =
        contexts_.emplace(std::string(*name), Entry(std::move(path), nullptr))
            .second;
    CHECK(name_is_new);
  }

  // Ensure default profile entry exists (in both prefs and our data structure)
  // and initialize it.
  default_context_ = Get(kDefaultContextName, true);
}

bool AwBrowserContextStore::Exists(const std::string& name) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return contexts_.find(name) != contexts_.end();
}

std::vector<std::string> AwBrowserContextStore::List() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::vector<std::string> list;
  list.reserve(contexts_.size());
  for (const auto& kv : contexts_) {
    list.push_back(kv.first);
  }
  return list;
}

AwBrowserContext* AwBrowserContextStore::Get(const std::string& name,
                                             const bool create_if_needed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto context_it = contexts_.find(name);
  Entry* entry;
  if (context_it != contexts_.end()) {
    entry = &context_it->second;
  } else {
    if (create_if_needed) {
      entry = CreateNewContext(name);
    } else {
      return nullptr;
    }
  }
  if (!entry->instance) {
    const bool is_default = name == kDefaultContextName;
    entry->instance =
        std::make_unique<AwBrowserContext>(name, entry->path, is_default);
    // Ensure this code path is only taken if the IO thread is already running,
    // as it's needed for launching processes.
    if (base::FeatureList::IsEnabled(
            features::kCreateSpareRendererOnBrowserContextCreation) &&
        content::BrowserThread::IsThreadInitialized(
            content::BrowserThread::IO) &&
        (!is_default || kCreateSpareRendererForDefaultIfMultiProfile.Get())) {
      content::SpareRenderProcessHostManager::Get().WarmupSpare(
          entry->instance.get());
    }
  }
  return entry->instance.get();
}

AwBrowserContextStore::DeletionResult AwBrowserContextStore::Delete(
    const std::string& name) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const auto context_it = contexts_.find(name);
  if (context_it == contexts_.end()) {
    return DeletionResult::kDoesNotExist;
  }
  Entry* entry = &context_it->second;
  if (entry->instance) {
    return DeletionResult::kInUse;
  }

  ScopedListPrefUpdate update(&*prefs_, prefs::kProfileListPref);
  base::Value::List& profiles = update.Get();
  for (auto profile_it = profiles.begin(); profile_it != profiles.end();
       profile_it++) {
    const base::Value::Dict& dict = profile_it->GetDict();
    const std::string* cur_name = dict.FindString(kProfileNameKey);
    CHECK(cur_name);
    if (*cur_name == name) {
      const std::string* cur_path = dict.FindString(kProfilePathKey);
      CHECK(cur_path);
      CHECK_EQ(*cur_path, entry->path.value());
      // TODO(crbug.com/40268809): Make this async and backgroundable.
      AwBrowserContext::DeleteContext(entry->path);
      profiles.erase(profile_it);
      contexts_.erase(context_it);
      return DeletionResult::kDeleted;
    }
  }
  NOTREACHED() << "Profile exists in memory but not in prefs";
}

base::FilePath AwBrowserContextStore::GetRelativePathForTesting(
    const std::string& name) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const auto context_it = contexts_.find(name);
  CHECK(context_it != contexts_.end());
  return context_it->second.path;
}

AwBrowserContextStore::Entry* AwBrowserContextStore::CreateNewContext(
    std::string_view name) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto emplace_result = contexts_.emplace(std::string(name), Entry());
  // Check it was new
  CHECK(emplace_result.second);
  Entry* entry = &emplace_result.first->second;

  ScopedListPrefUpdate update(&*prefs_, prefs::kProfileListPref);
  base::Value::List& profiles = update.Get();
  if (name == kDefaultContextName) {
    entry->path = base::FilePath(AwBrowserContextStore::kDefaultContextPath);
    // Do not store the default profile in prefs - it is implicit.
  } else {
    int number = AssignNewProfileNumber();
    entry->path = base::FilePath(
        base::StrCat({"Profile ", base::NumberToString(number)}));
    AwBrowserContext::PrepareNewContext(entry->path);
    base::Value::Dict profileDict =
        base::Value::Dict()
            .Set(kProfileNameKey, name)
            .Set(kProfilePathKey, entry->path.value());
    profiles.Append(std::move(profileDict));
  }

  return entry;
}

int AwBrowserContextStore::AssignNewProfileNumber() {
  int number = prefs_->GetInteger(prefs::kProfileCounterPref) + 1;
  CHECK_GE(number, 1);
  prefs_->SetInteger(prefs::kProfileCounterPref, number);
  return number;
}

AwBrowserContext* AwBrowserContextStore::GetDefault() const {
  return default_context_;
}

jboolean JNI_AwBrowserContextStore_CheckNamedContextExists(
    JNIEnv* const env,
    const base::android::JavaParamRef<jstring>& jname) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return AwBrowserContextStore::GetInstance()->Exists(
      base::android::ConvertJavaStringToUTF8(env, jname));
}

base::android::ScopedJavaLocalRef<jobject>
JNI_AwBrowserContextStore_GetNamedContextJava(
    JNIEnv* const env,
    const base::android::JavaParamRef<jstring>& jname,
    jboolean create_if_needed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  AwBrowserContext* context = AwBrowserContextStore::GetInstance()->Get(
      base::android::ConvertJavaStringToUTF8(env, jname), create_if_needed);
  return context ? context->GetJavaBrowserContext() : nullptr;
}

jboolean JNI_AwBrowserContextStore_DeleteNamedContext(
    JNIEnv* const env,
    const base::android::JavaParamRef<jstring>& jname) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const std::string name = base::android::ConvertJavaStringToUTF8(env, jname);
  AwBrowserContextStore::DeletionResult result =
      AwBrowserContextStore::GetInstance()->Delete(name);
  switch (result) {
    case AwBrowserContextStore::DeletionResult::kDeleted:
      return true;
    case AwBrowserContextStore::DeletionResult::kDoesNotExist:
      return false;
    case AwBrowserContextStore::DeletionResult::kInUse:
      const std::string error_message =
          base::StrCat({"Cannot delete in-use profile ", name});
      env->ThrowNew(env->FindClass("java/lang/IllegalStateException"),
                    error_message.c_str());
      return false;
  }
}

base::android::ScopedJavaLocalRef<jstring>
JNI_AwBrowserContextStore_GetNamedContextPathForTesting(
    JNIEnv* const env,
    const base::android::JavaParamRef<jstring>& jname) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::string name = base::android::ConvertJavaStringToUTF8(env, jname);
  AwBrowserContextStore* store = AwBrowserContextStore::GetInstance();
  if (!store->Exists(name)) {
    return nullptr;
  }
  base::FilePath path = store->GetRelativePathForTesting(name);
  return base::android::ConvertUTF8ToJavaString(env, path.value());
}

base::android::ScopedJavaLocalRef<jobjectArray>
JNI_AwBrowserContextStore_ListAllContexts(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const std::vector<std::string> names =
      AwBrowserContextStore::GetInstance()->List();
  return base::android::ToJavaArrayOfStrings(env, names);
}

// static
void AwBrowserContextStore::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kProfileListPref, base::Value::List());
  registry->RegisterIntegerPref(prefs::kProfileCounterPref, 0);
}

// static
AwBrowserContextStore* AwBrowserContextStore::GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(g_initialized);
  return GetOrCreateInstance();
}

// static
AwBrowserContextStore* AwBrowserContextStore::GetOrCreateInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  static base::NoDestructor<AwBrowserContextStore> instance(
      AwBrowserProcess::GetInstance()->local_state());
  g_initialized = true;
  return instance.get();
}

AwBrowserContextStore::Entry::Entry() = default;

AwBrowserContextStore::Entry::Entry(
    base::FilePath&& path,
    std::unique_ptr<AwBrowserContext>&& instance)
    : path(std::move(path)), instance(std::move(instance)) {}

AwBrowserContextStore::Entry::Entry(Entry&&) = default;

AwBrowserContextStore::Entry::~Entry() = default;

}  // namespace android_webview
