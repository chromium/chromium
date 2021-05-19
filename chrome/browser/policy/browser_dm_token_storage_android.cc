// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_dm_token_storage_android.h"

#include <string>

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/policy/android/cloud_management_shared_preferences.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"

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
  return std::string();
}

std::string BrowserDMTokenStorageAndroid::InitEnrollmentToken() {
  return std::string();
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
