// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_dm_token_storage_android.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/android/cloud_management_android_connection.h"
#include "chrome/browser/policy/android/cloud_management_shared_preferences.h"
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

bool DeleteDmTokenFromSharedPreferences() {
  android::DeleteDmTokenFromSharedPreferences();
  return true;
}

}  // namespace

BrowserDMTokenStorageAndroid::BrowserDMTokenStorageAndroid()
    : task_runner_(base::ThreadPool::CreateTaskRunner({base::MayBlock()})) {}

BrowserDMTokenStorageAndroid::~BrowserDMTokenStorageAndroid() {}

std::string BrowserDMTokenStorageAndroid::InitClientId() {
  return android::GetClientId();
}

std::string BrowserDMTokenStorageAndroid::InitEnrollmentToken() {
  // When a DMToken is available or main profile is managed, it's possible that
  // this method was called very early in the initialization process, even
  // before `g_browser_process` be initialized.
  // However, if DM token is available we don't need enrollment token. And main
  // profile only requires the device identity to calculate the profile id.
  // Neither of them actually need enrollment token so it's safe to return an
  // empty string for now.
  if (!CanInitEnrollmentToken()) {
    return std::string();
  }

  const base::Value* value =
      g_browser_process->browser_policy_connector()
          ->GetPolicyService()
          ->GetPolicies(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
          .GetValue(key::kCloudManagementEnrollmentToken,
                    base::Value::Type::STRING);
  return value ? value->GetString() : std::string();
}

std::string BrowserDMTokenStorageAndroid::InitDMToken() {
  return android::ReadDmTokenFromSharedPreferences();
}

bool BrowserDMTokenStorageAndroid::InitEnrollmentErrorOption() {
  return false;
}

bool BrowserDMTokenStorageAndroid::CanInitEnrollmentToken() const {
  return g_browser_process && g_browser_process->browser_policy_connector() &&
         g_browser_process->browser_policy_connector()->HasPolicyService();
}

BrowserDMTokenStorage::StoreTask BrowserDMTokenStorageAndroid::SaveDMTokenTask(
    const std::string& token,
    const std::string& client_id) {
  return base::BindOnce(&StoreDmTokenInSharedPreferences, token);
}

BrowserDMTokenStorage::StoreTask
BrowserDMTokenStorageAndroid::DeleteDMTokenTask(const std::string& client_id) {
  return base::BindOnce(&DeleteDmTokenFromSharedPreferences);
}

scoped_refptr<base::TaskRunner>
BrowserDMTokenStorageAndroid::SaveDMTokenTaskRunner() {
  return task_runner_;
}

}  // namespace policy
