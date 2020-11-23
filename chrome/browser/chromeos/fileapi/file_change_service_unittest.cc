// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/file_change_service.h"

#include "chrome/browser/chromeos/fileapi/file_change_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile_manager.h"

namespace chromeos {

namespace {

// FileChangeServiceTest -------------------------------------------------------

class FileChangeServiceTest : public BrowserWithTestWindowTest {
 public:
  FileChangeServiceTest() = default;
  FileChangeServiceTest(const FileChangeServiceTest& other) = delete;
  FileChangeServiceTest& operator=(const FileChangeServiceTest& other) = delete;
  ~FileChangeServiceTest() override = default;

  // Creates and returns a new profile for the specified `name`.
  TestingProfile* CreateProfileWithName(const std::string& name) {
    return profile_manager()->CreateTestingProfile(name);
  }

 private:
  // BrowserWithTestWindowTest:
  TestingProfile* CreateProfile() override {
    constexpr char kPrimaryProfileName[] = "primary_profile";
    return CreateProfileWithName(kPrimaryProfileName);
  }
};

}  // namespace

// Tests -----------------------------------------------------------------------

// Verifies service instances are created on a per-profile basis.
TEST_F(FileChangeServiceTest, CreatesServiceInstancesPerProfile) {
  auto* factory = FileChangeServiceFactory::GetInstance();
  ASSERT_TRUE(factory);

  // `FileChangeService` should exist for the primary profile.
  auto* primary_profile = GetProfile();
  auto* primary_profile_service = factory->GetService(primary_profile);
  ASSERT_TRUE(primary_profile_service);

  // `FileChangeService` should be created as needed for additional profiles.
  constexpr char kSecondaryProfileName[] = "secondary_profile";
  auto* secondary_profile = CreateProfileWithName(kSecondaryProfileName);
  auto* secondary_profile_service = factory->GetService(secondary_profile);
  ASSERT_TRUE(secondary_profile_service);

  // Per-profile services should be unique.
  ASSERT_NE(primary_profile_service, secondary_profile_service);
}

}  // namespace chromeos
