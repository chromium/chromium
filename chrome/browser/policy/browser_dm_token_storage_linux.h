// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_LINUX_H_
#define CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_LINUX_H_

#include "chrome/browser/policy/browser_dm_token_storage.h"

#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"

namespace policy {

// Implementation of BrowserDMTokenStorage for Linux. The global singleton
// instance can be retrieved by calling BrowserDMTokenStorage::Get().
class BrowserDMTokenStorageLinux : public BrowserDMTokenStorage {
 public:
  // Get the global singleton instance by calling BrowserDMTokenStorage::Get().
  BrowserDMTokenStorageLinux();
  ~BrowserDMTokenStorageLinux() override;

 private:
  // override BrowserDMTokenStorage
  std::string InitClientId() override;
  std::string InitEnrollmentToken() override;
  std::string InitDMToken() override;
  bool InitEnrollmentErrorOption() override;
  void SaveDMToken(const std::string& token) override;

  // Returns the content of "/etc/machine-id". Virtual for tests.
  virtual std::string ReadMachineIdFile();

  // This should always be the last member of the class.
  base::WeakPtrFactory<BrowserDMTokenStorageLinux> weak_factory_{this};

  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageLinuxTest, InitClientId);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageLinuxTest, InitEnrollmentToken);
  // TODO(crbug.com/907589): Remove once no longer in use.
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageLinuxTest,
                           InitOldEnrollmentToken);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageLinuxTest, InitDMToken);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageLinuxTest,
                           InitDMTokenWithoutDirectory);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageLinuxTest, SaveDMToken);

  DISALLOW_COPY_AND_ASSIGN(BrowserDMTokenStorageLinux);
};

}  // namespace policy
#endif  // CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_LINUX_H_
