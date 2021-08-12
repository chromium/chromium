// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/system_extensions_sandboxed_unpacker.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

using Status = SystemExtensionsSandboxedUnpacker::Status;

namespace {

class SystemExtensionsSandboxedUnpackerTest : public testing::Test {
 public:
  std::tuple<Status, std::unique_ptr<SystemExtension>>
  GetSystemExtensionFromStringAndWait(base::StringPiece manifest) {
    base::RunLoop run_loop;
    Status status;
    std::unique_ptr<SystemExtension> system_extension;

    // Create SystemExtension.
    SystemExtensionsSandboxedUnpacker unpacker;
    unpacker.GetSystemExtensionFromString(
        manifest, base::BindLambdaForTesting(
                      [&](Status returned_status,
                          std::unique_ptr<SystemExtension> returned_extension) {
                        status = returned_status;
                        system_extension = std::move(returned_extension);
                        run_loop.Quit();
                      }));
    run_loop.Run();

    return {status, std::move(system_extension)};
  }

 private:
  base::ScopedTempDir system_extension_dir_;
  base::test::TaskEnvironment task_environment;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

}  // namespace

TEST_F(SystemExtensionsSandboxedUnpackerTest, Success) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "01020304",
    "type": "echo",
    "service_worker_url": "/sw.js",
    "name": "Long Test",
    "short_name": "Test",
    "companion_web_app_url": "https://test.example/"
  })";

  std::unique_ptr<SystemExtension> system_extension;
  Status status;
  std::tie(status, system_extension) =
      GetSystemExtensionFromStringAndWait(kSystemExtensionManifest);
  EXPECT_EQ(Status::kOk, status);
  EXPECT_TRUE(system_extension.get());

  EXPECT_EQ(SystemExtensionId({1, 2, 3, 4}), system_extension->id);
  EXPECT_EQ(SystemExtensionType::kEcho, system_extension->type);
  EXPECT_EQ("chrome-untrusted://system-extension-echo-01020304/",
            system_extension->base_url.spec());
  EXPECT_EQ("chrome-untrusted://system-extension-echo-01020304/sw.js",
            system_extension->service_worker_url.spec());
  EXPECT_EQ("Long Test", system_extension->name);
  ASSERT_TRUE(system_extension->short_name.has_value());
  EXPECT_EQ("Test", system_extension->short_name);
  ASSERT_TRUE(system_extension->companion_web_app_url.has_value());
  EXPECT_EQ("https://test.example/",
            system_extension->companion_web_app_url->spec());
}

TEST_F(SystemExtensionsSandboxedUnpackerTest, Failure_EmptyManifest) {
  static constexpr const char kSystemExtensionManifest[] = R"()";
  std::unique_ptr<SystemExtension> system_extension;
  Status status;
  std::tie(status, system_extension) =
      GetSystemExtensionFromStringAndWait(kSystemExtensionManifest);
  EXPECT_EQ(Status::kFailedJsonErrorParsingManifest, status);
  EXPECT_FALSE(system_extension.get());
}

TEST_F(SystemExtensionsSandboxedUnpackerTest, Failure_EmptyManifest2) {
  static constexpr const char kSystemExtensionManifest[] = R"({})";
  std::unique_ptr<SystemExtension> system_extension;
  Status status;
  std::tie(status, system_extension) =
      GetSystemExtensionFromStringAndWait(kSystemExtensionManifest);

  EXPECT_EQ(Status::kFailedIdMissing, status);
  EXPECT_FALSE(system_extension.get());
}

TEST_F(SystemExtensionsSandboxedUnpackerTest, Failure_IdMissing) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "type": "echo",
    "service_worker_url": "/sw.js",
    "name": "Long Test",
    "short_name": "Test",
    "companion_web_app_url": "https://test.example/"
  })";

  std::unique_ptr<SystemExtension> system_extension;
  Status status;
  std::tie(status, system_extension) =
      GetSystemExtensionFromStringAndWait(kSystemExtensionManifest);

  EXPECT_EQ(Status::kFailedIdMissing, status);
  EXPECT_FALSE(system_extension.get());
}

TEST_F(SystemExtensionsSandboxedUnpackerTest, Failure_IdInvalidTooShort) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "0102",
    "type": "echo",
    "service_worker_url": "/sw.js",
    "name": "Long Test",
    "short_name": "Test",
    "companion_web_app_url": "https://test.example/"
  })";

  std::unique_ptr<SystemExtension> system_extension;
  Status status;
  std::tie(status, system_extension) =
      GetSystemExtensionFromStringAndWait(kSystemExtensionManifest);

  EXPECT_EQ(Status::kFailedIdInvalid, status);
  EXPECT_FALSE(system_extension.get());
}

TEST_F(SystemExtensionsSandboxedUnpackerTest, Failure_IdInvalidTooLong) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "010203040506",
    "type": "echo",
    "service_worker_url": "/sw.js",
    "name": "Long Test",
    "short_name": "Test",
    "companion_web_app_url": "https://test.example/"
  })";

  std::unique_ptr<SystemExtension> system_extension;
  Status status;
  std::tie(status, system_extension) =
      GetSystemExtensionFromStringAndWait(kSystemExtensionManifest);

  EXPECT_EQ(Status::kFailedIdInvalid, status);
  EXPECT_FALSE(system_extension.get());
}

TEST_F(SystemExtensionsSandboxedUnpackerTest, Failure_IdInvalidCharacters) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "0102030X",
    "type": "echo",
    "service_worker_url": "/sw.js",
    "name": "Long Test",
    "short_name": "Test",
    "companion_web_app_url": "https://test.example/"
  })";

  std::unique_ptr<SystemExtension> system_extension;
  Status status;
  std::tie(status, system_extension) =
      GetSystemExtensionFromStringAndWait(kSystemExtensionManifest);

  EXPECT_EQ(Status::kFailedIdInvalid, status);
  EXPECT_FALSE(system_extension.get());
}

TEST_F(SystemExtensionsSandboxedUnpackerTest, Failure_TypeMissing) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "01020304",
    "service_worker_url": "/sw.js",
    "name": "Long Test",
    "short_name": "Test",
    "companion_web_app_url": "https://test.example/"
  })";

  std::unique_ptr<SystemExtension> system_extension;
  Status status;
  std::tie(status, system_extension) =
      GetSystemExtensionFromStringAndWait(kSystemExtensionManifest);

  EXPECT_EQ(Status::kFailedTypeMissing, status);
  EXPECT_FALSE(system_extension.get());
}

TEST_F(SystemExtensionsSandboxedUnpackerTest, Failure_TypeInvalid) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "01020304",
    "type": "foo",
    "service_worker_url": "/sw.js",
    "name": "Long Test",
    "short_name": "Test",
    "companion_web_app_url": "https://test.example/"
  })";

  std::unique_ptr<SystemExtension> system_extension;
  Status status;
  std::tie(status, system_extension) =
      GetSystemExtensionFromStringAndWait(kSystemExtensionManifest);

  EXPECT_EQ(Status::kFailedTypeInvalid, status);
  EXPECT_FALSE(system_extension.get());
}

TEST_F(SystemExtensionsSandboxedUnpackerTest, Failure_ServiceWorkerUrlMissing) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "01020304",
    "type": "echo",
    "name": "Long Test",
    "short_name": "Test",
    "companion_web_app_url": "https://test.example/"
  })";

  std::unique_ptr<SystemExtension> system_extension;
  Status status;
  std::tie(status, system_extension) =
      GetSystemExtensionFromStringAndWait(kSystemExtensionManifest);

  EXPECT_EQ(Status::kFailedServiceWorkerUrlMissing, status);
  EXPECT_FALSE(system_extension.get());
}

TEST_F(SystemExtensionsSandboxedUnpackerTest, Failure_ServiceWorkerUrlInvalid) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "01020304",
    "type": "echo",
    "service_worker_url": "../",
    "name": "Long Test",
    "short_name": "Test",
    "companion_web_app_url": "https://test.example/"
  })";

  std::unique_ptr<SystemExtension> system_extension;
  Status status;
  std::tie(status, system_extension) =
      GetSystemExtensionFromStringAndWait(kSystemExtensionManifest);

  EXPECT_EQ(Status::kFailedServiceWorkerUrlInvalid, status);
  EXPECT_FALSE(system_extension.get());
}

TEST_F(SystemExtensionsSandboxedUnpackerTest, Failure_ServiceWorkerUrlEmpty) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "01020304",
    "type": "echo",
    "service_worker_url": "",
    "name": "Long Test",
    "short_name": "Test",
    "companion_web_app_url": "https://test.example/"
  })";

  std::unique_ptr<SystemExtension> system_extension;
  Status status;
  std::tie(status, system_extension) =
      GetSystemExtensionFromStringAndWait(kSystemExtensionManifest);

  EXPECT_EQ(Status::kFailedServiceWorkerUrlInvalid, status);
  EXPECT_FALSE(system_extension.get());
}

TEST_F(SystemExtensionsSandboxedUnpackerTest,
       Failure_ServiceWorkerUrlInvalidDifferentOrigin) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "01020304",
    "type": "echo",
    "service_worker_url": "https://test.example",
    "name": "Long Test",
    "short_name": "Test",
    "companion_web_app_url": "https://test.example/"
  })";

  std::unique_ptr<SystemExtension> system_extension;
  Status status;
  std::tie(status, system_extension) =
      GetSystemExtensionFromStringAndWait(kSystemExtensionManifest);

  EXPECT_EQ(Status::kFailedServiceWorkerUrlDifferentOrigin, status);
  EXPECT_FALSE(system_extension.get());
}

TEST_F(SystemExtensionsSandboxedUnpackerTest, Failure_NameMissing) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "01020304",
    "type": "echo",
    "service_worker_url": "/sw.js",
    "short_name": "Test",
    "companion_web_app_url": "https://test.example/"
  })";

  std::unique_ptr<SystemExtension> system_extension;
  Status status;
  std::tie(status, system_extension) =
      GetSystemExtensionFromStringAndWait(kSystemExtensionManifest);

  EXPECT_EQ(Status::kFailedNameMissing, status);
  EXPECT_FALSE(system_extension.get());
}

TEST_F(SystemExtensionsSandboxedUnpackerTest, Failure_NameEmpty) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "01020304",
    "type": "echo",
    "service_worker_url": "/sw.js",
    "name": "",
    "short_name": "Test",
    "companion_web_app_url": "https://test.example/"
  })";

  std::unique_ptr<SystemExtension> system_extension;
  Status status;
  std::tie(status, system_extension) =
      GetSystemExtensionFromStringAndWait(kSystemExtensionManifest);

  EXPECT_EQ(Status::kFailedNameEmpty, status);
  EXPECT_FALSE(system_extension.get());
}

TEST_F(SystemExtensionsSandboxedUnpackerTest, Success_NoShortName) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "01020304",
    "type": "echo",
    "service_worker_url": "/sw.js",
    "name": "Long Test",
    "companion_web_app_url": "https://test.example/"
  })";

  std::unique_ptr<SystemExtension> system_extension;
  Status status;
  std::tie(status, system_extension) =
      GetSystemExtensionFromStringAndWait(kSystemExtensionManifest);

  EXPECT_EQ(Status::kOk, status);
  ASSERT_TRUE(system_extension.get());
  EXPECT_FALSE(system_extension->short_name.has_value());
}

TEST_F(SystemExtensionsSandboxedUnpackerTest, Success_EmptyShortName) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "01020304",
    "type": "echo",
    "service_worker_url": "/sw.js",
    "name": "Long Test",
    "short_name": "",
    "companion_web_app_url": "https://test.example/"
  })";

  std::unique_ptr<SystemExtension> system_extension;
  Status status;
  std::tie(status, system_extension) =
      GetSystemExtensionFromStringAndWait(kSystemExtensionManifest);

  EXPECT_EQ(Status::kOk, status);
  ASSERT_TRUE(system_extension.get());
  EXPECT_FALSE(system_extension->short_name.has_value());
}

TEST_F(SystemExtensionsSandboxedUnpackerTest, Success_NoCompanionWebAppUrl) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "01020304",
    "type": "echo",
    "service_worker_url": "/sw.js",
    "name": "Long Test",
    "short_name": "Test"
  })";

  std::unique_ptr<SystemExtension> system_extension;
  Status status;
  std::tie(status, system_extension) =
      GetSystemExtensionFromStringAndWait(kSystemExtensionManifest);

  EXPECT_EQ(Status::kOk, status);
  ASSERT_TRUE(system_extension.get());
  EXPECT_FALSE(system_extension->companion_web_app_url.has_value());
}

TEST_F(SystemExtensionsSandboxedUnpackerTest,
       Success_InvalidCompanionWebAppUrl) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "01020304",
    "type": "echo",
    "service_worker_url": "/sw.js",
    "name": "Long Test",
    "short_name": "Test",
    "companion_web_app_url": "foobar"
  })";

  std::unique_ptr<SystemExtension> system_extension;
  Status status;
  std::tie(status, system_extension) =
      GetSystemExtensionFromStringAndWait(kSystemExtensionManifest);

  EXPECT_EQ(Status::kOk, status);
  ASSERT_TRUE(system_extension.get());
  EXPECT_FALSE(system_extension->companion_web_app_url.has_value());
}

TEST_F(SystemExtensionsSandboxedUnpackerTest,
       Success_InsecureCompanionWebAppUrl) {
  static constexpr const char kSystemExtensionManifest[] = R"({
    "id": "01020304",
    "type": "echo",
    "service_worker_url": "/sw.js",
    "name": "Long Test",
    "short_name": "Test",
    "companion_web_app_url": "http://test.example"
  })";

  std::unique_ptr<SystemExtension> system_extension;
  Status status;
  std::tie(status, system_extension) =
      GetSystemExtensionFromStringAndWait(kSystemExtensionManifest);

  EXPECT_EQ(Status::kOk, status);
  ASSERT_TRUE(system_extension.get());
  EXPECT_FALSE(system_extension->companion_web_app_url.has_value());
}
