// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/e2e_tests/test_accounts_util.h"

#include <ostream>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

using base::Value;

namespace signin {
namespace test {

#if BUILDFLAG(IS_WIN)
const char kPlatform[] = "win";
#elif BUILDFLAG(IS_MAC)
const char kPlatform[] = "mac";
#elif BUILDFLAG(IS_CHROMEOS_ASH)
const char kPlatform[] = "chromeos";
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
const char kPlatform[] = "linux";
#elif BUILDFLAG(IS_ANDROID)
const char kPlatform[] = "android";
#else
const char kPlatform[] = "all_platform";
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
  for (auto [account_name, content] : content_json->GetDict()) {
    const Value::Dict& content_dict = content.GetDict();
    const Value::Dict* platform_account = content_dict.FindDict(kPlatform);
    if (!platform_account) {
      platform_account = content_dict.FindDict("all_platform");
      if (!platform_account) {
        continue;
      }
    }
    TestAccount ta(*(platform_account->FindString("user")),
                   *(platform_account->FindString("password")));
    all_accounts_.insert(std::pair<std::string, TestAccount>(account_name, ta));
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
