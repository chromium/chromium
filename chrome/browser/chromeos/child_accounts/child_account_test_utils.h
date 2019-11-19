// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_CHILD_ACCOUNT_TEST_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_CHILD_ACCOUNT_TEST_UTILS_H_

#include <string>

namespace chromeos {
namespace test {

// Returns a base64-encoded dummy token for child log-in.
std::string GetChildAccountOAuthIdToken();

}  // namespace test
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_CHILD_ACCOUNT_TEST_UTILS_H_
