// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_messaging_test_util.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/registry.h"
#endif

namespace extensions {

namespace {

void WriteTestNativeHostManifest(const base::FilePath& target_dir,
                                 const std::string& host_name,
                                 const base::FilePath& host_path,
                                 bool user_level,
                                 bool supports_native_initiated_connections) {
  auto manifest = base::Value::Dict()
                      .Set("name", host_name)
                      .Set("description", "Native Messaging Echo Test")
                      .Set("type", "stdio")
                      .Set("path", host_path.AsUTF8Unsafe())
                      .Set("supports_native_initiated_connections",
                           supports_native_initiated_connections);

  manifest.Set("allowed_origins",
               base::Value::List().Append(base::StringPrintf(
                   "chrome-extension://%s/",
                   ScopedTestNativeMessagingHost::kExtensionId)));

  base::FilePath manifest_path = target_dir.AppendASCII(host_name + ".json");
  JSONFileValueSerializer serializer(manifest_path);
  ASSERT_TRUE(serializer.Serialize(manifest));

#if BUILDFLAG(IS_WIN)
  HKEY root_key = user_level ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
  std::wstring key = L"SOFTWARE\\Google\\Chrome\\NativeMessagingHosts\\" +
                     base::UTF8ToWide(host_name);
  base::win::RegKey manifest_key(
      root_key, key.c_str(),
      KEY_SET_VALUE | KEY_CREATE_SUB_KEY | KEY_CREATE_LINK);
  ASSERT_EQ(ERROR_SUCCESS,
            manifest_key.WriteValue(NULL, manifest_path.value().c_str()));
#endif
}

}  // namespace

const char ScopedTestNativeMessagingHost::kHostName[] =
    "com.google.chrome.test.echo";
const char ScopedTestNativeMessagingHost::kBinaryMissingHostName[] =
    "com.google.chrome.test.host_binary_missing";
const char ScopedTestNativeMessagingHost::
    kSupportsNativeInitiatedConnectionsHostName[] =
        "com.google.chrome.test.inbound_native_echo";
const char ScopedTestNativeMessagingHost::kExtensionId[] =
    "knldjmfmopnpolahpmmgbagdohdnhkik";
#if BUILDFLAG(IS_WIN)
const char ScopedTestNativeMessagingHost::kHostExeName[] =
    "com.google.chrome.test.exe.echo";
#endif

ScopedTestNativeMessagingHost::ScopedTestNativeMessagingHost() {}

void ScopedTestNativeMessagingHost::RegisterTestHost(bool user_level) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  base::FilePath test_user_data_dir;
  ASSERT_TRUE(
      base::PathService::Get(chrome::DIR_TEST_DATA, &test_user_data_dir));
  test_user_data_dir = test_user_data_dir.AppendASCII("native_messaging")
                           .AppendASCII("native_hosts");

#if BUILDFLAG(IS_WIN)
  HKEY root_key = user_level ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
  ASSERT_NO_FATAL_FAILURE(registry_override_.OverrideRegistry(root_key));
#else
  path_override_ = std::make_unique<base::ScopedPathOverride>(
      user_level ? chrome::DIR_USER_NATIVE_MESSAGING
                 : chrome::DIR_NATIVE_MESSAGING,
      temp_dir_.GetPath());
#endif

  base::CopyFile(test_user_data_dir.AppendASCII("echo.py"),
                 temp_dir_.GetPath().AppendASCII("echo.py"));
#if BUILDFLAG(IS_WIN)
  base::FilePath host_path = temp_dir_.GetPath().AppendASCII("echo.bat");
  base::CopyFile(test_user_data_dir.AppendASCII("echo.bat"), host_path);
#endif

#if BUILDFLAG(IS_POSIX)
  base::FilePath host_path = temp_dir_.GetPath().AppendASCII("echo.py");
  ASSERT_TRUE(base::SetPosixFilePermissions(
      host_path, base::FILE_PERMISSION_READ_BY_USER |
                     base::FILE_PERMISSION_WRITE_BY_USER |
                     base::FILE_PERMISSION_EXECUTE_BY_USER));
#endif
  ASSERT_NO_FATAL_FAILURE(WriteTestNativeHostManifest(
      temp_dir_.GetPath(), kHostName, host_path, user_level, false));

  ASSERT_NO_FATAL_FAILURE(WriteTestNativeHostManifest(
      temp_dir_.GetPath(), kBinaryMissingHostName,
      test_user_data_dir.AppendASCII("missing_nm_binary.exe"), user_level,
      false));

  ASSERT_NO_FATAL_FAILURE(WriteTestNativeHostManifest(
      temp_dir_.GetPath(), kSupportsNativeInitiatedConnectionsHostName,
      host_path, user_level, true));
}

#if BUILDFLAG(IS_WIN)
// On Windows, a new codepath is used to directly launch .EXE-based Native
// Hosts.
void ScopedTestNativeMessagingHost::RegisterTestExeHost(
    std::string_view filename,
    bool user_level) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  base::FilePath binary_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &binary_dir));
  HKEY root_key = user_level ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
  ASSERT_NO_FATAL_FAILURE(registry_override_.OverrideRegistry(root_key));

  // Unlike in the |RegisterTestHost| case above, we must leave the Host
  // .exe where it was built, because the Host will fail to run from the
  // temp_dir_ if is_component_build is set for the build.
  base::FilePath host_path = binary_dir.AppendASCII(filename);
  ASSERT_NO_FATAL_FAILURE(WriteTestNativeHostManifest(
      temp_dir_.GetPath(), kHostExeName, host_path, user_level, false));
}
#endif

ScopedTestNativeMessagingHost::~ScopedTestNativeMessagingHost() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::ignore = temp_dir_.Delete();
}

}  // namespace extensions
