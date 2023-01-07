// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_CLOUD_EXTERNAL_DATA_MANAGER_BASE_TEST_UTIL_H_
#define CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_CLOUD_EXTERNAL_DATA_MANAGER_BASE_TEST_UTIL_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"

namespace base {
class Value;
}

namespace net {
namespace test_server {
class EmbeddedTestServer;
}
}  // namespace net

namespace policy {

namespace test {

// Passes |data| to |destination| and invokes |done_callback| to indicate that
// the |data| has been retrieved.
void ExternalDataFetchCallback(std::unique_ptr<std::string>* data_destination,
                               base::FilePath* file_path_destination,
                               base::OnceClosure done_callback,
                               std::unique_ptr<std::string> data,
                               const base::FilePath& file_path);

// Constructs a value that points a policy referencing external data at |url|
// and sets the expected hash of the external data to that of |data|.
base::Value ConstructExternalDataReference(const std::string& url,
                                           const std::string& data);

// Constructs the external data policy from the content of the file located on
// |external_data_path|.
std::string ConstructExternalDataPolicy(
    const net::test_server::EmbeddedTestServer& test_server,
    const std::string& external_data_path);

}  // namespace test
}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_CLOUD_EXTERNAL_DATA_MANAGER_BASE_TEST_UTIL_H_
