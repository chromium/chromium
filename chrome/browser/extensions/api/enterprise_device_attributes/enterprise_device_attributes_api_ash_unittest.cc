// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_device_attributes/enterprise_device_attributes_api_ash.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/core/device_attributes_fake.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/api_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr char kFakeAssetId[] = "fake asset ID";

}  // namespace

class EnterpriseDeviceAttributesApiAshTest : public testing::Test {
 protected:
  void SetUp() override {
    // Set up fake device attributes.
    device_attributes_ = std::make_unique<policy::FakeDeviceAttributes>();
    device_attributes_->SetFakeDeviceAssetId(kFakeAssetId);

    // Set up a testing profile. Needs to return true when passed to
    // crosapi::browser_util::IsSigninProfileOrBelongsToAffiliatedUser.
    TestingProfile::Builder builder;
    builder.SetPath(base::FilePath(FILE_PATH_LITERAL(chrome::kInitialProfile)));
    profile_ = builder.Build();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<policy::FakeDeviceAttributes> device_attributes_;
  std::unique_ptr<Profile> profile_;
};

TEST_F(EnterpriseDeviceAttributesApiAshTest, GetDeviceAssetIdFunction) {
  auto function =
      base::MakeRefCounted<EnterpriseDeviceAttributesGetDeviceAssetIdFunction>(
          std::move(device_attributes_));

  std::unique_ptr<base::Value> result =
      api_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), /*args=*/"[]", profile_.get());
  ASSERT_TRUE(result->is_string());
  EXPECT_EQ(kFakeAssetId, result->GetString());
}

}  // namespace extensions
