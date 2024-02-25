// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/external_data/cloud_external_data_manager_base_test_util.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/common/chrome_paths.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "crypto/sha2.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

namespace {
// Keys for 'Value::Dict' objects
const char kUrlKey[] = "url";
const char kHashKey[] = "hash";
}  // namespace

namespace test {

base::Value::Dict ConstructExternalDataReference(const std::string& url,
                                                 const std::string& data) {
  const std::string hash = crypto::SHA256HashString(data);
  base::Value::Dict metadata;
  metadata.Set(kUrlKey, url);
  metadata.Set(kHashKey, base::HexEncode(hash));
  return metadata;
}

base::Value::Dict ConstructExternalDataPolicy(
    const net::test_server::EmbeddedTestServer& test_server,
    const std::string& external_data_path) {
  std::string url =
      test_server.GetURL(std::string("/") + external_data_path).spec();

  std::string external_data;
  base::FilePath test_data_dir;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::ReadFileToString(
        test_data_dir.AppendASCII(external_data_path), &external_data));
  }
  return ConstructExternalDataReference(url, external_data);
}

}  // namespace test
}  // namespace policy
