// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_WIN_H_
#define CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_WIN_H_

#include "components/enterprise/browser/controller/browser_dm_token_storage.h"

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"

namespace policy {

// Implementation of BrowserDMTokenStorage delegate for Windows.
class BrowserDMTokenStorageWin : public BrowserDMTokenStorage::Delegate {
 public:
  BrowserDMTokenStorageWin();
  BrowserDMTokenStorageWin(const BrowserDMTokenStorageWin&) = delete;
  BrowserDMTokenStorageWin& operator=(const BrowserDMTokenStorageWin&) = delete;
  ~BrowserDMTokenStorageWin() override;

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

  scoped_refptr<base::SingleThreadTaskRunner> com_sta_task_runner_;

  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageWinTest, InitClientId);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageWinTest,
                           InitEnrollmentTokenFromPreferred);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageWinTest,
                           InitEnrollmentTokenFromSecondary);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageWinTest, InitDMToken);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageWinTest,
                           InitDMTokenFromBrowserLocation);
};

}  // namespace policy
#endif  // CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_WIN_H_
