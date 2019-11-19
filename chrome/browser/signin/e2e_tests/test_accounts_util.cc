// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/e2e_tests/test_accounts_util.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "build/build_config.h"

using base::Value;

namespace signin {
namespace test {

#if defined(OS_WIN)
std::string kPlatform = "win";
#elif defined(OS_MACOSX)
std::string kPlatform = "mac";
#elif defined(OS_LINUX)
std::string kPlatform = "linux";
#elif defined(OS_CHROMEOS)
std::string kPlatform = "chromeos";
#elif defined(OS_ANDROID)
std::string kPlatform = "android";
#else
std::string kPlatform = "all_platform";
#endif

TestAccountsUtil::TestAccountsUtil() = default;
TestAccountsUtil::~TestAccountsUtil() = default;

bool TestAccountsUtil::Init(const base::FilePath& config_path) {
  int error_code = 0;
  std::string error_str;
  JSONFileValueDeserializer deserializer(config_path);
  std::unique_ptr<Value> content_json =
      deserializer.Deserialize(&error_code, &error_str);
  CHECK(error_code == 0) << "Error reading json file. Error code: "
                         << error_code << " " << error_str;
  CHECK(content_json);

  // Only store platform specific users. If an account does not have
  // platform specific user, try to use all_platform user.
  for (auto account : content_json->DictItems()) {
    const Value* platform_account = account.second.FindDictKey(kPlatform);
    if (platform_account == nullptr) {
      platform_account = account.second.FindDictKey("all_platform");
      if (platform_account == nullptr) {
        continue;
      }
    }
    TestAccount ta(*(platform_account->FindStringKey("user")),
                   *(platform_account->FindStringKey("password")));
    all_accounts_.insert(
        std::pair<std::string, TestAccount>(account.first, ta));
  }
  return true;
}

bool TestAccountsUtil::GetAccount(const std::string& name,
                                  TestAccount& out_account) const {
  auto it = all_accounts_.find(name);
  if (it == all_accounts_.end()) {
    return false;
  }
  out_account = it->second;
  return true;
}

}  // namespace test
}  // namespace signin
