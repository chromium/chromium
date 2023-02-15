// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_LINUX_H_
#define CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_LINUX_H_

#include "components/enterprise/browser/controller/browser_dm_token_storage.h"

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"

namespace policy {

// Implementation of BrowserDMTokenStorage delegate for Linux.
class BrowserDMTokenStorageLinux : public BrowserDMTokenStorage::Delegate {
 public:
  BrowserDMTokenStorageLinux();
  BrowserDMTokenStorageLinux(const BrowserDMTokenStorageLinux&) = delete;
  BrowserDMTokenStorageLinux& operator=(const BrowserDMTokenStorageLinux&) =
      delete;
  ~BrowserDMTokenStorageLinux() override;

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

  // Returns the content of "/etc/machine-id". Virtual for tests.
  virtual std::string ReadMachineIdFile();

  // Allows caching of the machine ID
  std::string client_id_;
  scoped_refptr<base::TaskRunner> task_runner_;

  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageLinuxTest, InitClientId);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageLinuxTest, InitEnrollmentToken);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageLinuxTest, InitDMToken);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageLinuxTest,
                           InitDMTokenWithoutDirectory);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageLinuxTest, SaveDMToken);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageLinuxTest, DeleteDMToken);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageLinuxTest, DeleteEmptyDMToken);
};

}  // namespace policy
#endif  // CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_LINUX_H_
