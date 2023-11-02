// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_CLOUD_EXTERNAL_DATA_MANAGER_BASE_TEST_UTIL_H_
#define CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_CLOUD_EXTERNAL_DATA_MANAGER_BASE_TEST_UTIL_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/files/file_path.h"

namespace base {
class DictionaryValue;
}

namespace net {
namespace test_server {
class EmbeddedTestServer;
}
}  // namespace net

namespace policy {

class CloudPolicyCore;

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
std::unique_ptr<base::DictionaryValue> ConstructExternalDataReference(
    const std::string& url,
    const std::string& data);

// Constructs the external data policy from the content of the file located on
// |external_data_path|.
std::string ConstructExternalDataPolicy(
    const net::test_server::EmbeddedTestServer& test_server,
    const std::string& external_data_path);

// TODO(bartfab): Makes an arbitrary |policy| in |core| reference external data
// as specified in |metadata|. This is only done because there are no policies
// that reference external data yet. Once the first such policy is added, it
// will be sufficient to set its value to |metadata| and this method should be
// removed.
void SetExternalDataReference(CloudPolicyCore* core,
                              const std::string& policy,
                              std::unique_ptr<base::DictionaryValue> metadata);

}  // namespace test
}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_CLOUD_EXTERNAL_DATA_MANAGER_BASE_TEST_UTIL_H_
