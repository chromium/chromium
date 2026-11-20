// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/autofill_custom_form_control_scripts_component_installer.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/global_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/autofill/core/browser/integrators/custom_form_controls/custom_form_control_script_index.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace component_updater {
namespace {

using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;

class AutofillCustomFormControlScriptsComponentInstallerTest
    : public ::testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(install_dir_.CreateUniqueTempDir()); }

 protected:
  base::ScopedPathOverride component_user_dir_override_{DIR_COMPONENT_USER};
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir install_dir_;
  std::unique_ptr<ComponentInstallerPolicy> policy_ = std::make_unique<
      AutofillCustomFormControlScriptsComponentInstallerPolicy>();
  NiceMock<MockComponentUpdateService> cus_;
};

// Tests that `VerifyInstallation()` succeeds iff `index.json` exists in the
// installation directory.
TEST_F(AutofillCustomFormControlScriptsComponentInstallerTest,
       VerifyInstallation) {
  EXPECT_FALSE(
      policy_->VerifyInstallation(base::DictValue(), install_dir_.GetPath()));

  ASSERT_TRUE(
      base::WriteFile(install_dir_.GetPath().AppendASCII("index.json"), "[]"));
  EXPECT_TRUE(
      policy_->VerifyInstallation(base::DictValue(), install_dir_.GetPath()));
}

// Tests that `ComponentReady()` loads scripts from the installation directory
// into `GlobalFeatures::custom_form_control_script_index()`.
TEST_F(AutofillCustomFormControlScriptsComponentInstallerTest, ComponentReady) {
  base::test::ScopedFeatureList feature_list{
      autofill::features::kAutofillCustomFormControlScripts};
  GlobalFeatures* global_features =
      TestingBrowserProcess::GetGlobal()->GetFeatures();
  ASSERT_NE(global_features, nullptr);
  global_features->PreMainMessageLoopRun();

  autofill::CustomFormControlScriptIndex* index =
      global_features->custom_form_control_script_index();
  ASSERT_NE(index, nullptr);
  EXPECT_FALSE(index->is_initialized());

  ASSERT_TRUE(base::WriteFile(
      install_dir_.GetPath().AppendASCII("index.json"),
      R"([{"host": "example.com", "script_path": "site.js"}])"));
  ASSERT_TRUE(base::WriteFile(install_dir_.GetPath().AppendASCII("site.js"),
                              "/*component_script*/"));

  policy_->ComponentReady(base::Version("1.0.0.0"), install_dir_.GetPath(),
                          base::DictValue());
  ASSERT_TRUE(base::test::RunUntil([&] { return index->is_initialized(); }));
  EXPECT_EQ(index->FindScript(GURL("https://example.com/checkout")),
            "/*component_script*/");
}

// Tests that `RegisterAutofillCustomFormControlScriptsComponent()` registers
// the component with `ComponentUpdateService` when the feature is enabled.
TEST_F(AutofillCustomFormControlScriptsComponentInstallerTest,
       ComponentRegistered) {
  base::test::ScopedFeatureList feature_list{
      autofill::features::kAutofillCustomFormControlScripts};

  base::RunLoop run_loop;
  EXPECT_CALL(cus_, RegisterComponent)
      .WillOnce(DoAll([&] { run_loop.Quit(); }, Return(true)));

  RegisterAutofillCustomFormControlScriptsComponent(&cus_);
  run_loop.Run();
}

// Tests that `RegisterAutofillCustomFormControlScriptsComponent()` skips
// registration when `--autofill-custom-form-control-scripts-dir` is set.
TEST_F(AutofillCustomFormControlScriptsComponentInstallerTest,
       ComponentNotRegisteredWhenDirectorySwitchSet) {
  base::test::ScopedFeatureList feature_list{
      autofill::features::kAutofillCustomFormControlScripts};
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchPath(
      autofill::switches::kAutofillCustomFormControlScriptsDir,
      install_dir_.GetPath());

  EXPECT_CALL(cus_, RegisterComponent).Times(0);
  RegisterAutofillCustomFormControlScriptsComponent(&cus_);
}

}  // namespace
}  // namespace component_updater
