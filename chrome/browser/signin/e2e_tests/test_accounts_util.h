// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_E2E_TESTS_TEST_ACCOUNTS_UTIL_H_
#define CHROME_BROWSER_SIGNIN_E2E_TESTS_TEST_ACCOUNTS_UTIL_H_

#include <map>
#include <string>
#include "base/macros.h"

namespace base {
class FilePath;
}

namespace signin {
namespace test {

struct TestAccount {
  std::string user;
  std::string password;
  TestAccount() = default;
  TestAccount(const std::string& user, const std::string& password) {
    this->user = user;
    this->password = password;
  }
};

class TestAccountsUtil {
 public:
  TestAccountsUtil();
  virtual ~TestAccountsUtil();
  bool Init(const base::FilePath& config_path);
  bool GetAccount(const std::string& name, TestAccount& out_account) const;

 private:
  std::map<std::string, TestAccount> all_accounts_;
  DISALLOW_COPY_AND_ASSIGN(TestAccountsUtil);
};

}  // namespace test
}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_E2E_TESTS_TEST_ACCOUNTS_UTIL_H_
