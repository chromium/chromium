// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_WIN_H_
#define CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_WIN_H_

#include "chrome/browser/policy/browser_dm_token_storage.h"

#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"

namespace policy {

// Implementation of BrowserDMTokenStorage for Windows. The global singleton
// instance can be retrieved by calling BrowserDMTokenStorage::Get().
class BrowserDMTokenStorageWin : public BrowserDMTokenStorage {
 public:
  // Get the global singleton instance by calling BrowserDMTokenStorage::Get().
  BrowserDMTokenStorageWin();
  ~BrowserDMTokenStorageWin() override;

 private:
  // override BrowserDMTokenStorage
  std::string InitClientId() override;
  std::string InitEnrollmentToken() override;
  std::string InitDMToken() override;
  bool InitEnrollmentErrorOption() override;
  void SaveDMToken(const std::string& token) override;

  scoped_refptr<base::SingleThreadTaskRunner> com_sta_task_runner_;

  // This should always be the last member of the class.
  base::WeakPtrFactory<BrowserDMTokenStorageWin> weak_factory_{this};

  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageWinTest, InitClientId);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageWinTest, InitEnrollmentToken);
  // TODO(crbug.com/907589): Remove once no longer in use.
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageWinTest,
                           InitOldEnrollmentToken);
  // TODO(crbug.com/907589): Remove once no longer in use.
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageWinTest,
                           InitOldEnrollmentTokenPriority);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageWinTest, InitDMToken);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageWinTest, SaveDMToken);

  DISALLOW_COPY_AND_ASSIGN(BrowserDMTokenStorageWin);
};

}  // namespace policy
#endif  // CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_WIN_H_
