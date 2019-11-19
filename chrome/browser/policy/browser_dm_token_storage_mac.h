// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_MAC_H_
#define CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_MAC_H_

#include "chrome/browser/policy/browser_dm_token_storage.h"

#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"

namespace policy {

// Implementation of BrowserDMTokenStorage for Mac OS. The global singleton
// instance can be retrieved by calling BrowserDMTokenStorage::Get().
class BrowserDMTokenStorageMac : public BrowserDMTokenStorage {
 public:
  // Get the global singleton instance by calling BrowserDMTokenStorage::Get().
  BrowserDMTokenStorageMac();
  ~BrowserDMTokenStorageMac() override;

 private:
  // override BrowserDMTokenStorage
  std::string InitClientId() override;
  std::string InitEnrollmentToken() override;
  std::string InitDMToken() override;
  bool InitEnrollmentErrorOption() override;
  void SaveDMToken(const std::string& token) override;

  // This should always be the last member of the class.
  base::WeakPtrFactory<BrowserDMTokenStorageMac> weak_factory_;

  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageMacTest, InitClientId);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageMacTest, InitEnrollmentToken);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageMacTest, InitDMToken);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageMacTest,
                           InitDMTokenWithoutDirectory);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageMacTest, SaveDMToken);
  DISALLOW_COPY_AND_ASSIGN(BrowserDMTokenStorageMac);
};

}  // namespace policy
#endif  // CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_MAC_H_
