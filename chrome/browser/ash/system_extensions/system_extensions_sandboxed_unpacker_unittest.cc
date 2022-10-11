// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/system_extensions_sandboxed_unpacker.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/system_extensions/system_extensions_install_status.h"
#include "chrome/browser/ash/system_extensions/system_extensions_status_or.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using Status = SystemExtensionsInstallStatus;

namespace {

enum class TestMethod {
  kFromDir,
  kFromString,
};

std::string TestMethodToString(
    const ::testing::TestParamInfo<TestMethod>& info) {
  switch (info.param) {
    case TestMethod::kFromDir:
      return "FromDir";
    case TestMethod::kFromString:
      return "FromString";
  }
}

class SystemExtensionsSandboxedUnpackerTest
    : public testing::TestWithParam<TestMethod> {
 protected:
  InstallStatusOrSystemExtension GetSystemExtensionFromDirAndWait(
      base::FilePath system_extension_dir) {
    base::RunLoop run_loop;
    InstallStatusOrSystemExtension status;

    SystemExtensionsSandboxedUnpacker unpacker;
    unpacker.GetSystemExtensionFromDir(
        system_extension_dir,
        base::BindLambdaForTesting([&](InstallStatusOrSystemExtension s) {
          status = std::move(s);
          run_loop.Quit();
        }));
    run_loop.Run();
    return status;
  }

  InstallStatusOrSystemExtension GetSystemExtensionFromStringAndWait(
      base::StringPiece manifest) {
    base::RunLoop run_loop;
    InstallStatusOrSystemExtension status;

    // Create SystemExtension.
    SystemExtensionsSandboxedUnpacker unpacker;
    unpacker.GetSystemExtensionFromString(
        manifest,
        base::BindLambdaForTesting([&](InstallStatusOrSystemExtension s) {
          status = std::move(s);
          run_loop.Quit();
        }));
    run_loop.Run();
    return status;
  }

  // Runs the necessary setup and calls the method being tested.
  InstallStatusOrSystemExtension CallGetSystemExtensionFrom(
      base::StringPiece manifest_str) {
    TestMethod method = GetParam();
    if (method == TestMethod::kFromDir) {
      base::ScopedTempDir system_extension_dir;
      CHECK(system_extension_dir.CreateUniqueTempDir());

      base::FilePath manifest_file =
          system_extension_dir.GetPath().Append("manifest.json");
      CHECK(base::WriteFile(manifest_file, manifest_str));

      return GetSystemExtensionFromDirAndWait(system_extension_dir.GetPath());
    }
    CHECK_EQ(method, TestMethod::kFromString);
    return GetSystemExtensionFromStringAndWait(manifest_str);
  }

 private:
  base::ScopedTempDir system_extension_dir_;
  base::test::TaskEnvironment task_environment;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

using SystemExtensionsSandboxedUnpackerFromDirTest =
    SystemExtensionsSandboxedUnpackerTest;

}  // namespace

TEST_P(SystemExtensionsSandboxedUnpackerTest, Success) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "01020304",
    "type": "window-management",
    "service_worker_url": "/sw.js",
    "name": "Long Test",
    "short_name": "Test"
  })";

  auto result = CallGetSystemExtensionFrom(kSystemExtensionManifest);
  EXPECT_TRUE(result.ok());
  auto system_extension = std::move(result).value();

  EXPECT_EQ(SystemExtensionId({1, 2, 3, 4}), system_extension.id);
  EXPECT_EQ(SystemExtensionType::kWindowManagement, system_extension.type);
  EXPECT_EQ("chrome-untrusted://system-extension-window-management-01020304/",
            system_extension.base_url.spec());
  EXPECT_EQ(
      "chrome-untrusted://system-extension-window-management-01020304/sw.js",
      system_extension.service_worker_url.spec());
  EXPECT_EQ("Long Test", system_extension.name);
  ASSERT_TRUE(system_extension.short_name.has_value());
  EXPECT_EQ("Test", system_extension.short_name);
}

TEST_P(SystemExtensionsSandboxedUnpackerTest, Failure_EmptyManifest) {
  static constexpr const char kSystemExtensionManifest[] = R"()";
  std::unique_ptr<SystemExtension> system_extension;
  auto result = CallGetSystemExtensionFrom(kSystemExtensionManifest);
  EXPECT_EQ(Status::kFailedJsonErrorParsingManifest, result.status());
}

TEST_P(SystemExtensionsSandboxedUnpackerTest, Failure_EmptyManifest2) {
  static constexpr const char kSystemExtensionManifest[] = R"({})";
  auto result = CallGetSystemExtensionFrom(kSystemExtensionManifest);
  EXPECT_EQ(Status::kFailedIdMissing, result.status());
}

TEST_P(SystemExtensionsSandboxedUnpackerTest, Failure_IdMissing) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "type": "window-management",
    "service_worker_url": "/sw.js",
    "name": "Long Test",
    "short_name": "Test"
  })";

  auto result = CallGetSystemExtensionFrom(kSystemExtensionManifest);
  EXPECT_EQ(Status::kFailedIdMissing, result.status());
}

TEST_P(SystemExtensionsSandboxedUnpackerTest, Failure_IdInvalidTooShort) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "0102",
    "type": "window-management",
    "service_worker_url": "/sw.js",
    "name": "Long Test",
    "short_name": "Test"
  })";

  auto result = CallGetSystemExtensionFrom(kSystemExtensionManifest);
  EXPECT_EQ(Status::kFailedIdInvalid, result.status());
}

TEST_P(SystemExtensionsSandboxedUnpackerTest, Failure_IdInvalidTooLong) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "010203040506",
    "type": "window-management",
    "service_worker_url": "/sw.js",
    "name": "Long Test",
    "short_name": "Test"
  })";

  auto result = CallGetSystemExtensionFrom(kSystemExtensionManifest);
  EXPECT_EQ(Status::kFailedIdInvalid, result.status());
}

TEST_P(SystemExtensionsSandboxedUnpackerTest, Failure_IdInvalidCharacters) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "0102030X",
    "type": "window-management",
    "service_worker_url": "/sw.js",
    "name": "Long Test",
    "short_name": "Test"
  })";

  auto result = CallGetSystemExtensionFrom(kSystemExtensionManifest);
  EXPECT_EQ(Status::kFailedIdInvalid, result.status());
}

TEST_P(SystemExtensionsSandboxedUnpackerTest, Failure_TypeMissing) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "01020304",
    "service_worker_url": "/sw.js",
    "name": "Long Test",
    "short_name": "Test"
  })";

  auto result = CallGetSystemExtensionFrom(kSystemExtensionManifest);
  EXPECT_EQ(Status::kFailedTypeMissing, result.status());
}

TEST_P(SystemExtensionsSandboxedUnpackerTest, Failure_TypeInvalid) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "01020304",
    "type": "foo",
    "service_worker_url": "/sw.js",
    "name": "Long Test",
    "short_name": "Test"
  })";

  auto result = CallGetSystemExtensionFrom(kSystemExtensionManifest);
  EXPECT_EQ(Status::kFailedTypeInvalid, result.status());
}

TEST_P(SystemExtensionsSandboxedUnpackerTest, Failure_ServiceWorkerUrlMissing) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "01020304",
    "type": "window-management",
    "name": "Long Test",
    "short_name": "Test"
  })";

  auto result = CallGetSystemExtensionFrom(kSystemExtensionManifest);
  EXPECT_EQ(Status::kFailedServiceWorkerUrlMissing, result.status());
}

TEST_P(SystemExtensionsSandboxedUnpackerTest, Failure_ServiceWorkerUrlInvalid) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "01020304",
    "type": "window-management",
    "service_worker_url": "../",
    "name": "Long Test",
    "short_name": "Test"
  })";

  auto result = CallGetSystemExtensionFrom(kSystemExtensionManifest);
  EXPECT_EQ(Status::kFailedServiceWorkerUrlInvalid, result.status());
}

TEST_P(SystemExtensionsSandboxedUnpackerTest, Failure_ServiceWorkerUrlEmpty) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "01020304",
    "type": "window-management",
    "service_worker_url": "",
    "name": "Long Test",
    "short_name": "Test"
  })";

  auto result = CallGetSystemExtensionFrom(kSystemExtensionManifest);
  EXPECT_EQ(Status::kFailedServiceWorkerUrlInvalid, result.status());
}

TEST_P(SystemExtensionsSandboxedUnpackerTest,
       Failure_ServiceWorkerUrlInvalidDifferentOrigin) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "01020304",
    "type": "window-management",
    "service_worker_url": "https://test.example",
    "name": "Long Test",
    "short_name": "Test"
  })";

  auto result = CallGetSystemExtensionFrom(kSystemExtensionManifest);
  EXPECT_EQ(Status::kFailedServiceWorkerUrlDifferentOrigin, result.status());
}

TEST_P(SystemExtensionsSandboxedUnpackerTest, Failure_NameMissing) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "01020304",
    "type": "window-management",
    "service_worker_url": "/sw.js",
    "short_name": "Test"
  })";

  auto result = CallGetSystemExtensionFrom(kSystemExtensionManifest);
  EXPECT_EQ(Status::kFailedNameMissing, result.status());
}

TEST_P(SystemExtensionsSandboxedUnpackerTest, Failure_NameEmpty) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "01020304",
    "type": "window-management",
    "service_worker_url": "/sw.js",
    "name": "",
    "short_name": "Test"
  })";

  auto result = CallGetSystemExtensionFrom(kSystemExtensionManifest);
  EXPECT_EQ(Status::kFailedNameEmpty, result.status());
}

TEST_P(SystemExtensionsSandboxedUnpackerTest, Success_NoShortName) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "01020304",
    "type": "window-management",
    "service_worker_url": "/sw.js",
    "name": "Long Test"
  })";

  auto result = CallGetSystemExtensionFrom(kSystemExtensionManifest);
  EXPECT_FALSE(result.value().short_name.has_value());
}

TEST_P(SystemExtensionsSandboxedUnpackerTest, Success_EmptyShortName) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "01020304",
    "type": "window-management",
    "service_worker_url": "/sw.js",
    "name": "Long Test",
    "short_name": ""
  })";

  auto result = CallGetSystemExtensionFrom(kSystemExtensionManifest);
  EXPECT_FALSE(result.value().short_name.has_value());
}

TEST_P(SystemExtensionsSandboxedUnpackerFromDirTest, Failure_NoDir) {
  // Create a directory and delete a directory to get a unique file path to a
  // non existent directory.
  base::ScopedTempDir system_extension_dir;
  ASSERT_TRUE(system_extension_dir.CreateUniqueTempDir());
  ASSERT_TRUE(base::DeleteFile(system_extension_dir.GetPath()));

  auto result =
      GetSystemExtensionFromDirAndWait(system_extension_dir.GetPath());
  EXPECT_EQ(Status::kFailedDirectoryMissing, result.status());
}

TEST_P(SystemExtensionsSandboxedUnpackerFromDirTest, Failure_NoManifest) {
  base::ScopedTempDir system_extension_dir;
  ASSERT_TRUE(system_extension_dir.CreateUniqueTempDir());

  auto result =
      GetSystemExtensionFromDirAndWait(system_extension_dir.GetPath());
  EXPECT_EQ(Status::kFailedManifestReadError, result.status());
}

// Tests that the "oem-diagnostics-control" type can't be parsed since it is
// only available with a feature flag that is disabled by default.
TEST_P(SystemExtensionsSandboxedUnpackerTest,
       Failure_ParseOemDiagnosticsAndControlType) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "01020304",
    "type": "oem-diagnostics-control",
    "service_worker_url": "/sw.js",
    "name": "Long Test",
    "short_name": "Test"
  })";

  auto result = CallGetSystemExtensionFrom(kSystemExtensionManifest);
  EXPECT_FALSE(result.ok());
}

INSTANTIATE_TEST_SUITE_P(CreateFromDir,
                         SystemExtensionsSandboxedUnpackerFromDirTest,
                         testing::Values(TestMethod::kFromDir),
                         TestMethodToString);

INSTANTIATE_TEST_SUITE_P(CreateFrom,
                         SystemExtensionsSandboxedUnpackerTest,
                         testing::Values(TestMethod::kFromDir,
                                         TestMethod::kFromString),
                         TestMethodToString);

}  // namespace ash
