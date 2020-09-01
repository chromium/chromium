// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/autofill_states_component_installer.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "components/autofill/core/browser/geo/state_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

class AutofillStatesDataComponentInstallerPolicyTest : public ::testing::Test {
 public:
  AutofillStatesDataComponentInstallerPolicyTest() : fake_version_("0.0.1") {}

  void SetUp() override {
    ASSERT_TRUE(component_install_dir_.CreateUniqueTempDir());
    filenames_ = {"US", "IN", "DE", "AB"};
  }

  const base::Version& version() const { return fake_version_; }

  const base::DictionaryValue& manifest() const { return manifest_; }

  const base::FilePath& GetPath() const {
    return component_install_dir_.GetPath();
  }

  void CreateEmptyFiles() {
    for (const char* filename : filenames_)
      base::WriteFile(GetPath().AppendASCII(filename), "");
  }

  void DeleteCreatedFiles() {
    for (const char* filename : filenames_)
      base::DeleteFile(GetPath().AppendASCII(filename));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

 private:
  base::DictionaryValue manifest_;
  base::ScopedTempDir component_install_dir_;
  std::vector<const char*> filenames_;
  base::FilePath fake_install_dir_;
  base::Version fake_version_;
};

// Tests that VerifyInstallation only returns true when all expected files are
// present.
TEST_F(AutofillStatesDataComponentInstallerPolicyTest, VerifyInstallation) {
  AutofillStatesComponentInstallerPolicy policy(
      base::BindLambdaForTesting([&](const base::FilePath& path) {}));

  // An empty dir lacks all required files.
  EXPECT_FALSE(policy.VerifyInstallationForTesting(manifest(), GetPath()));

  CreateEmptyFiles();
  // Files should exist.
  EXPECT_TRUE(policy.VerifyInstallationForTesting(manifest(), GetPath()));

  // Delete all the created files.
  DeleteCreatedFiles();
  EXPECT_FALSE(policy.VerifyInstallationForTesting(manifest(), GetPath()));
}

// Tests that ComponentReady calls Lambda.
TEST_F(AutofillStatesDataComponentInstallerPolicyTest,
       ComponentReady_CallsLambda) {
  base::FilePath given_path;

  AutofillStatesComponentInstallerPolicy policy(base::BindLambdaForTesting(
      [&](const base::FilePath& path) { given_path = path; }));

  policy.ComponentReadyForTesting(version(), GetPath(),
                                  std::make_unique<base::DictionaryValue>());

  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(GetPath(), given_path);
}

}  // namespace component_updater
