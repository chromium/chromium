// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/child_account_test_utils.h"

#include "base/base64.h"
#include "base/strings/stringprintf.h"

namespace chromeos {
namespace test {

std::string GetChildAccountOAuthIdToken() {
  std::string encoded;
  base::Base64Encode(R"({ "services": ["uca"] })", &encoded);
  return base::StringPrintf("dummy-header.%s.dummy-signature", encoded.c_str());
}

}  // namespace test
}  // namespace chromeos
