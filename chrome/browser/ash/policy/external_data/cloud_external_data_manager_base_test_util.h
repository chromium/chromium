// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_CLOUD_EXTERNAL_DATA_MANAGER_BASE_TEST_UTIL_H_
#define CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_CLOUD_EXTERNAL_DATA_MANAGER_BASE_TEST_UTIL_H_

#include <string>

#include "base/values.h"

namespace net {
namespace test_server {
class EmbeddedTestServer;
}
}  // namespace net

namespace policy {

namespace test {

// Constructs a value that points a policy referencing external data at |url|
// and sets the expected hash of the external data to that of |data|.
base::Value::Dict ConstructExternalDataReference(const std::string& url,
                                                 const std::string& data);

// Constructs the external data policy from the content of the file located on
// |external_data_path|, and returns it as a dictionary.
base::Value::Dict ConstructExternalDataPolicy(
    const net::test_server::EmbeddedTestServer& test_server,
    const std::string& external_data_path);

}  // namespace test
}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_CLOUD_EXTERNAL_DATA_MANAGER_BASE_TEST_UTIL_H_
