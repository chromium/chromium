// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/translate_kit_component_installer.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {
namespace {

using ::testing::_;

constexpr char kFakeTranslateKitVersion[] = "0.0.1";

}  // namespace

class TranslateKitComponentMockComponentUpdateService
    : public component_updater::MockComponentUpdateService {
 public:
  TranslateKitComponentMockComponentUpdateService() = default;

  // Not Copyable.
  TranslateKitComponentMockComponentUpdateService(
      const TranslateKitComponentMockComponentUpdateService&) = delete;
  TranslateKitComponentMockComponentUpdateService& operator=(
      const TranslateKitComponentMockComponentUpdateService&) = delete;

  ~TranslateKitComponentMockComponentUpdateService() override = default;
};

class RegisterTranslateKitComponentTest : public ::testing::Test {
 public:
  RegisterTranslateKitComponentTest() = default;

  void SetUp() override {
    ASSERT_TRUE(fake_install_dir_.CreateUniqueTempDir());
    SetVersion(kFakeTranslateKitVersion);
  }

  // Not Copyable.
  RegisterTranslateKitComponentTest(const RegisterTranslateKitComponentTest&) =
      delete;
  RegisterTranslateKitComponentTest& operator=(
      const RegisterTranslateKitComponentTest&) = delete;

 protected:
  content::BrowserTaskEnvironment& env() { return env_; }
  PrefService* pref_service() { return &pref_service_; }
  const base::FilePath& install_dir() const {
    return fake_install_dir_.GetPath();
  }
  const base::Version& version() const { return fake_version_; }
  const base::Value::Dict& manifest() const { return fake_manifest_; }

  void SetVersion(std::string_view version_str) {
    fake_version_ = base::Version(version_str);
    fake_manifest_.Set("version", version_str);
  }

 private:
  content::BrowserTaskEnvironment env_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  base::ScopedTempDir fake_install_dir_;
  base::Version fake_version_;
  base::Value::Dict fake_manifest_;
};

TEST_F(RegisterTranslateKitComponentTest, ComponentRegistration) {
  auto service =
      std::make_unique<TranslateKitComponentMockComponentUpdateService>();

  EXPECT_CALL(*service, RegisterComponent(_)).Times(1);
  RegisterTranslateKitComponent(service.get(), pref_service());

  env().RunUntilIdle();
}

TEST_F(RegisterTranslateKitComponentTest, VerifyInstallationDefaultEmpty) {
  TranslateKitComponentInstallerPolicy policy(pref_service());

  // An empty directory lacks all required files.
  EXPECT_FALSE(policy.VerifyInstallation(manifest(), install_dir()));
}

}  // namespace component_updater
