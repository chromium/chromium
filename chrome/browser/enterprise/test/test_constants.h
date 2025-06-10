// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_TEST_TEST_CONSTANTS_H_
#define CHROME_BROWSER_ENTERPRISE_TEST_TEST_CONSTANTS_H_

#include "components/policy/test_support/client_storage.h"

namespace enterprise::test {

extern const char kFakeCustomerId[];
extern const char kDifferentCustomerId[];

extern const char kTestUserId[];
extern const char kTestUserEmail[];

extern const char kProfileDmToken[];
extern const char kProfileClientId[];

extern const char kBrowserDmToken[];
extern const char kBrowserClientId[];

extern const char kDeviceDmToken[];
extern const char kDeviceClientId[];

extern const char kEnrollmentToken[];

policy::ClientStorage::ClientInfo CreateBrowserClientInfo();

}  // namespace enterprise::test

#endif  // CHROME_BROWSER_ENTERPRISE_TEST_TEST_CONSTANTS_H_
