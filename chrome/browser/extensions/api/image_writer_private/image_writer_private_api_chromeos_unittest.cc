// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/image_writer_private_api.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/image_writer_private/error_constants.h"
#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chromeos/ash/components/disks/disk.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/policy/external_storage/device_id.h"
#include "chromeos/ash/components/policy/external_storage/test_support.h"
#include "extensions/browser/api/file_handlers/app_file_handler_util.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension_builder.h"

namespace extensions {

using ::ash::disks::Disk;
using ::ash::disks::DiskMountManager;
using ::policy::DeviceId;

constexpr char kDevicePath1[] = "/dev/device1";
constexpr uint16_t kVendorId1 = 0x1234;
constexpr uint16_t kProductId1 = 0xAAAA;
constexpr char kDevicePath2[] = "/dev/device2";
constexpr uint16_t kVendorId2 = 0x5678;
constexpr uint16_t kProductId2 = 0xBBBB;
constexpr char kImageUrl[] = "https://image.url.com/";
constexpr char kExtensionName[] = "ImageWriterPrivateApiTestExtension";

class ImageWriterPrivateApiTest : public ExtensionServiceTestBase {
 public:
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    ExtensionServiceTestBase::InitializeEmptyExtensionService();
    test_utils_.SetUp();
    AddDisks();
  }

  void TearDown() override {
    test_utils_.TearDown();
    ExtensionServiceTestBase::TearDown();
  }

  std::unique_ptr<Disk> CreateDisk(const std::string& device_path,
                                   uint16_t vid,
                                   uint16_t pid) {
    return Disk::Builder()
        .SetDevicePath(device_path)
        .SetVendorId(base::StringPrintf("%x", vid))
        .SetProductId(base::StringPrintf("%x", pid))
        .SetHasMedia(true)
        .SetIsParent(true)
        .SetDeviceType(ash::DeviceType::kUSB)
        .Build();
  }

  void AddDisks() {
    DiskMountManager::GetInstance()->AddDiskForTest(
        CreateDisk(kDevicePath1, kVendorId1, kProductId1));
    DiskMountManager::GetInstance()->AddDiskForTest(
        CreateDisk(kDevicePath2, kVendorId2, kProductId2));
  }

  void SetDisabled(bool disabled) {
    policy::external_storage::SetDisabled(*profile()->GetPrefs(), disabled);
  }

  void SetReadOnly(bool read_only) {
    policy::external_storage::SetReadOnly(*profile()->GetPrefs(), read_only);
  }

  void SetAllowlist(DeviceId device_id) {
    policy::external_storage::SetAllowlist(*profile()->GetPrefs(), device_id);
  }

  base::Value::List RunList() {
    auto function = base::MakeRefCounted<
        ImageWriterPrivateListRemovableStorageDevicesFunction>();
    std::optional<base::Value> result =
        api_test_utils::RunFunctionAndReturnSingleResult(function, "[]",
                                                         browser_context());
    return api_test_utils::ToList(result);
  }

  std::string RunWriteFromFile(std::string_view storage_unit_id) {
    auto function =
        base::MakeRefCounted<ImageWriterPrivateWriteFromFileFunction>();

    // Intentionally pass the filesystem_path (3rd argument) as empty to receive
    // an error for ValidateFileEntryAndGetPath, as otherwise it's near
    // impossible to fake it in unittests.
    // [storage_unit_id, filesystem_name, filesystem_path]
    std::string args = base::StringPrintf(R"(["%s", "", ""])", storage_unit_id);

    return api_test_utils::RunFunctionAndReturnError(function, args,
                                                     browser_context());
  }

  base::expected<base::Value::List, std::string> RunWriteFromUrl(
      std::string_view storage_unit_id,
      std::string_view image_url) {
    auto function =
        base::MakeRefCounted<ImageWriterPrivateWriteFromUrlFunction>();
    auto extension = ExtensionBuilder(kExtensionName).Build();
    function->set_extension(extension);

    // [storage_unit_id, image_url, optional image_hash]
    std::string args =
        base::StringPrintf(R"(["%s", "%s"])", storage_unit_id, image_url);

    return api_test_utils::RunFunctionAndReturnExpected(function, args,
                                                        browser_context());
  }

 protected:
  image_writer::ImageWriterTestUtils test_utils_;
};

TEST_F(ImageWriterPrivateApiTest, List) {
  base::Value::List result = RunList();
  EXPECT_EQ(2u, result.size());
}

TEST_F(ImageWriterPrivateApiTest, List_Disabled) {
  SetDisabled(true);
  base::Value::List result = RunList();
  EXPECT_EQ(0u, result.size());
}

TEST_F(ImageWriterPrivateApiTest, List_ReadOnly) {
  SetReadOnly(true);
  base::Value::List result = RunList();
  EXPECT_EQ(0u, result.size());
}

TEST_F(ImageWriterPrivateApiTest, List_Allowlist) {
  SetDisabled(true);
  SetAllowlist({kVendorId1, kProductId1});
  base::Value::List result = RunList();
  EXPECT_EQ(1u, result.size());
}

TEST_F(ImageWriterPrivateApiTest, WriteFromFile) {
  std::string result = RunWriteFromFile(kDevicePath1);
  EXPECT_EQ(app_file_handler_util::kInvalidParameters, result);
}

TEST_F(ImageWriterPrivateApiTest, WriteFromFile_Disabled) {
  SetDisabled(true);
  std::string result = RunWriteFromFile(kDevicePath1);
  EXPECT_EQ(image_writer::error::kDeviceWriteError, result);
}

TEST_F(ImageWriterPrivateApiTest, WriteFromFile_ReadOnly) {
  SetReadOnly(true);
  std::string result = RunWriteFromFile(kDevicePath1);
  EXPECT_EQ(image_writer::error::kDeviceWriteError, result);
}

TEST_F(ImageWriterPrivateApiTest, WriteFromFile_Allowlist) {
  SetReadOnly(true);
  SetAllowlist({kVendorId1, kProductId1});
  std::string result = RunWriteFromFile(kDevicePath1);
  EXPECT_EQ(app_file_handler_util::kInvalidParameters, result);
}

TEST_F(ImageWriterPrivateApiTest, WriteFromUrl) {
  auto result = RunWriteFromUrl(kDevicePath1, kImageUrl);
  EXPECT_TRUE(result.has_value());
}

TEST_F(ImageWriterPrivateApiTest, WriteFromUrl_Disabled) {
  SetDisabled(true);
  auto result = RunWriteFromUrl(kDevicePath1, kImageUrl);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(image_writer::error::kDeviceWriteError, result.error());
}

TEST_F(ImageWriterPrivateApiTest, WriteFromUrl_ReadOnly) {
  SetReadOnly(true);
  auto result = RunWriteFromUrl(kDevicePath1, kImageUrl);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(image_writer::error::kDeviceWriteError, result.error());
}

TEST_F(ImageWriterPrivateApiTest, WriteFromUrl_Allowlist) {
  SetDisabled(true);
  SetAllowlist({kVendorId1, kProductId1});
  auto result = RunWriteFromUrl(kDevicePath1, kImageUrl);
  EXPECT_TRUE(result.has_value());
}

}  // namespace extensions
