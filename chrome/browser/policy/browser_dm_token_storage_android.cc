// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_dm_token_storage_android.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/android/cloud_management_shared_preferences.h"
#include "chrome/browser/policy/android/jni_headers/CloudManagementAndroidConnection_jni.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"

namespace policy {

namespace {

bool StoreDmTokenInSharedPreferences(const std::string& dm_token) {
  android::SaveDmTokenInSharedPreferences(dm_token);
  return true;
}

}  // namespace

BrowserDMTokenStorageAndroid::BrowserDMTokenStorageAndroid()
    : task_runner_(base::ThreadPool::CreateTaskRunner({base::MayBlock()})) {}

BrowserDMTokenStorageAndroid::~BrowserDMTokenStorageAndroid() {}

std::string BrowserDMTokenStorageAndroid::InitClientId() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::android::ConvertJavaStringToUTF8(
      env, Java_CloudManagementAndroidConnection_getClientId(
               env, Java_CloudManagementAndroidConnection_getInstance(env)));
}

std::string BrowserDMTokenStorageAndroid::InitEnrollmentToken() {
  PolicyService* policy_service =
      g_browser_process->browser_policy_connector()->GetPolicyService();
  DCHECK(policy_service);

  const base::Value* value =
      policy_service
          ->GetPolicies(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
          .GetValue(key::kCloudManagementEnrollmentToken);
  return value && value->is_string() ? value->GetString() : std::string();
}

std::string BrowserDMTokenStorageAndroid::InitDMToken() {
  return android::ReadDmTokenFromSharedPreferences();
}

bool BrowserDMTokenStorageAndroid::InitEnrollmentErrorOption() {
  return false;
}

BrowserDMTokenStorage::StoreTask BrowserDMTokenStorageAndroid::SaveDMTokenTask(
    const std::string& token,
    const std::string& client_id) {
  return base::BindOnce(&StoreDmTokenInSharedPreferences, token);
}

scoped_refptr<base::TaskRunner>
BrowserDMTokenStorageAndroid::SaveDMTokenTaskRunner() {
  return task_runner_;
}

}  // namespace policy
