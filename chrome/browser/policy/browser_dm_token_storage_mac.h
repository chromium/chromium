// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_MAC_H_
#define CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_MAC_H_

#include "components/enterprise/browser/controller/browser_dm_token_storage.h"

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"

namespace policy {

// Implementation of BrowserDMTokenStorage delegate for Mac OS.
class BrowserDMTokenStorageMac : public BrowserDMTokenStorage::Delegate {
 public:
  BrowserDMTokenStorageMac();
  BrowserDMTokenStorageMac(const BrowserDMTokenStorageMac&) = delete;
  BrowserDMTokenStorageMac& operator=(const BrowserDMTokenStorageMac&) = delete;
  ~BrowserDMTokenStorageMac() override;

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

  // Allows caching of the machine serial number.
  std::string client_id_;
  scoped_refptr<base::TaskRunner> task_runner_;

  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageMacTest, InitClientId);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageMacTest, InitEnrollmentToken);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageMacTest, InitDMToken);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageMacTest,
                           InitDMTokenWithoutDirectory);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageMacTest, SaveDMToken);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageMacTest, DeleteDMToken);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageMacTest, DeleteEmptyDMToken);
};

}  // namespace policy
#endif  // CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_MAC_H_
