// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_ANDROID_H_
#define CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_ANDROID_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"

namespace policy {

// Implementation of BrowserDMTokenStorage delegate for Android.
class BrowserDMTokenStorageAndroid : public BrowserDMTokenStorage::Delegate {
 public:
  BrowserDMTokenStorageAndroid();
  BrowserDMTokenStorageAndroid(const BrowserDMTokenStorageAndroid&) = delete;
  BrowserDMTokenStorageAndroid& operator=(const BrowserDMTokenStorageAndroid&) =
      delete;

  ~BrowserDMTokenStorageAndroid() override;

  static BrowserDMTokenStorageAndroid* Get();

 private:
  // override BrowserDMTokenStorage::Delegate
  std::string InitClientId() override;
  std::string InitEnrollmentToken() override;
  std::string InitDMToken() override;
  bool InitEnrollmentErrorOption() override;
  bool CanInitEnrollmentToken() const override;
  BrowserDMTokenStorage::StoreTask SaveDMTokenTask(
      const std::string& token,
      const std::string& client_id) override;
  BrowserDMTokenStorage::StoreTask DeleteDMTokenTask(
      const std::string& client_id) override;
  scoped_refptr<base::TaskRunner> SaveDMTokenTaskRunner() override;

  scoped_refptr<base::TaskRunner> task_runner_;

  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageAndroidTest, InitClientId);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageAndroidTest,
                           InitEnrollmentToken);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageAndroidTest, InitDMToken);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageAndroidTest,
                           InitEnrollmentErrorOption);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageAndroidTest, SaveDMToken);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_ANDROID_H_
