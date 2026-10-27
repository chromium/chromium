// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/operation_manager.h"

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/image_writer_private/error_constants.h"
#include "chrome/browser/extensions/api/image_writer_private/features.h"
#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/test_event_router.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/mojom/api_permission_id.mojom-shared.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {
namespace image_writer {

namespace {

class ImageWriterOperationManagerTest : public ImageWriterUnitTestBase {
 public:
  void StartCallback(bool success, const std::string& error) {
    started_ = true;
    start_success_ = success;
    start_error_ = error;
  }

  void CancelCallback(bool success, const std::string& error) {
    cancelled_ = true;
    cancel_success_ = true;
    cancel_error_ = error;
  }

 protected:
  ImageWriterOperationManagerTest() : started_(false), start_success_(false) {}

  void SetUp() override {
    ImageWriterUnitTestBase::SetUp();
    CreateAndUseTestEventRouter(&test_profile_);
  }

  bool started_;
  bool start_success_;
  std::string start_error_;

  bool cancelled_;
  bool cancel_success_;
  std::string cancel_error_;

#if BUILDFLAG(IS_ANDROID)
  base::test::ScopedFeatureList scoped_feature_list_{
      kImageWriterPrivateAndroid};
#endif

  TestingProfile test_profile_;
};

TEST_F(ImageWriterOperationManagerTest, WriteFromFile) {
  OperationManager manager(&test_profile_);

  manager.StartWriteFromFile(
      kDummyExtensionId, test_utils_.GetImagePath(),
      test_utils_.GetDevicePath().AsUTF8Unsafe(),
      base::BindOnce(&ImageWriterOperationManagerTest::StartCallback,
                     base::Unretained(this)));

  EXPECT_TRUE(started_);
  EXPECT_TRUE(start_success_);
  EXPECT_EQ("", start_error_);

  manager.CancelWrite(
      kDummyExtensionId,
      base::BindOnce(&ImageWriterOperationManagerTest::CancelCallback,
                     base::Unretained(this)));

  EXPECT_TRUE(cancelled_);
  EXPECT_TRUE(cancel_success_);
  EXPECT_EQ("", cancel_error_);

  content::RunAllTasksUntilIdle();
}

TEST_F(ImageWriterOperationManagerTest, DestroyPartitions) {
  OperationManager manager(&test_profile_);

  manager.DestroyPartitions(
      kDummyExtensionId, test_utils_.GetDevicePath().AsUTF8Unsafe(),
      base::BindOnce(&ImageWriterOperationManagerTest::StartCallback,
                     base::Unretained(this)));

  EXPECT_TRUE(started_);
  EXPECT_TRUE(start_success_);
  EXPECT_EQ("", start_error_);

  manager.CancelWrite(
      kDummyExtensionId,
      base::BindOnce(&ImageWriterOperationManagerTest::CancelCallback,
                     base::Unretained(this)));

  EXPECT_TRUE(cancelled_);
  EXPECT_TRUE(cancel_success_);
  EXPECT_EQ("", cancel_error_);

  content::RunAllTasksUntilIdle();
}

TEST_F(ImageWriterOperationManagerTest, PermissionAllowlistScoping) {
  const Feature* feature =
      FeatureProvider::GetPermissionFeature("imageWriterPrivate");
  ASSERT_NE(feature, nullptr);

  // 1. Production Chromebook Recovery Utility extension ID.
  scoped_refptr<const Extension> prod_dru_ext =
      ExtensionBuilder("Chromebook Recovery Utility Prod")
          .SetID("pocpnlppkickgojjlmhdmidojbmbodfm")
          .AddAPIPermission("imageWriterPrivate")
          .Build();
  ASSERT_TRUE(prod_dru_ext);
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            feature->IsAvailableToExtension(prod_dru_ext.get()).result());
  EXPECT_TRUE(prod_dru_ext->permissions_data()->HasAPIPermission(
      mojom::APIPermissionID::kImageWriterPrivate));

  // 2. Dev Chromebook Recovery Utility extension ID.
  scoped_refptr<const Extension> dev_dru_ext =
      ExtensionBuilder("Chromebook Recovery Utility Dev")
          .SetID("dnhblfkbjpepijbdlobjfifmidhjdnhp")
          .AddAPIPermission("imageWriterPrivate")
          .Build();
  ASSERT_TRUE(dev_dru_ext);
  EXPECT_EQ(Feature::AvailabilityResult::kIsAvailable,
            feature->IsAvailableToExtension(dev_dru_ext.get()).result());
  EXPECT_TRUE(dev_dru_ext->permissions_data()->HasAPIPermission(
      mojom::APIPermissionID::kImageWriterPrivate));

  // 3. Non-allowlisted extension ID must be denied imageWriterPrivate.
  scoped_refptr<const Extension> unlisted_ext =
      ExtensionBuilder("Unlisted Extension")
          .SetID("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")
          .AddAPIPermission("imageWriterPrivate")
          .Build();
  ASSERT_TRUE(unlisted_ext);
  EXPECT_EQ(Feature::AvailabilityResult::kNotFoundInAllowlist,
            feature->IsAvailableToExtension(unlisted_ext.get()).result());
  EXPECT_FALSE(unlisted_ext->permissions_data()->HasAPIPermission(
      mojom::APIPermissionID::kImageWriterPrivate));
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(ImageWriterOperationManagerTest, DisabledByKillSwitchFeature) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kImageWriterPrivateAndroid);

  OperationManager manager(&test_profile_);

  manager.StartWriteFromFile(
      kDummyExtensionId, test_utils_.GetImagePath(),
      test_utils_.GetDevicePath().AsUTF8Unsafe(),
      base::BindOnce(&ImageWriterOperationManagerTest::StartCallback,
                     base::Unretained(this)));
  EXPECT_TRUE(started_);
  EXPECT_FALSE(start_success_);
  EXPECT_EQ(error::kUnsupportedOperation, start_error_);

  started_ = false;
  manager.StartWriteFromUrl(
      kDummyExtensionId, GURL("http://localhost/test.bin"), /*hash=*/"",
      test_utils_.GetDevicePath().AsUTF8Unsafe(),
      base::BindOnce(&ImageWriterOperationManagerTest::StartCallback,
                     base::Unretained(this)));
  EXPECT_TRUE(started_);
  EXPECT_FALSE(start_success_);
  EXPECT_EQ(error::kUnsupportedOperation, start_error_);

  started_ = false;
  manager.DestroyPartitions(
      kDummyExtensionId, test_utils_.GetDevicePath().AsUTF8Unsafe(),
      base::BindOnce(&ImageWriterOperationManagerTest::StartCallback,
                     base::Unretained(this)));
  EXPECT_TRUE(started_);
  EXPECT_FALSE(start_success_);
  EXPECT_EQ(error::kUnsupportedOperation, start_error_);
}
#endif

}  // namespace
}  // namespace image_writer
}  // namespace extensions
