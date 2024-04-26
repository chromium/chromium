// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "build/build_config.h"
#include "chrome/browser/ash/login/test/enrollment_helper_mixin.h"
#include "chrome/browser/ash/login/test/enrollment_ui_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_configuration_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/ui/webui/ash/login/hid_detection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/update_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "chromeos/test/chromeos_test_utils.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/ime/ash/input_method_util.h"

namespace ash {

// This test case will use
// src/chromeos/test/data/oobe_configuration/<TestName>.json file as
// OOBE configuration for each of the tests and verify that relevant parts
// of OOBE automation took place. OOBE WebUI will not be started until
// LoadConfiguration() is called to allow configure relevant stubs.
class OobeConfigurationTest : public OobeBaseTest {
 public:
  OobeConfigurationTest() = default;

  OobeConfigurationTest(const OobeConfigurationTest&) = delete;
  OobeConfigurationTest& operator=(const OobeConfigurationTest&) = delete;

  bool ShouldWaitForOobeUI() override { return false; }

  void LoadConfiguration() {
    OobeConfiguration::set_skip_check_for_testing(false);
    // Make sure configuration is loaded
    base::RunLoop run_loop;
    OOBEConfigurationWaiter waiter;

    OobeConfiguration::Get()->CheckConfiguration();

    const bool ready = waiter.IsConfigurationLoaded(run_loop.QuitClosure());
    if (!ready)
      run_loop.Run();

    // Let screens to settle.
    base::RunLoop().RunUntilIdle();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // File name is based on the test name.
    base::FilePath file;
    ASSERT_TRUE(GetTestFileName(".json", &file));

    command_line->AppendSwitchPath(chromeos::switches::kFakeOobeConfiguration,
                                   file);

    command_line->AppendSwitchASCII(switches::kArcAvailability,
                                    "officially-supported");
    OobeBaseTest::SetUpCommandLine(command_line);
  }

  // Stores a name of the configuration data file to `file`.
  // Returns true iff `file` exists.
  bool GetTestFileName(const std::string& suffix, base::FilePath* file) {
    const ::testing::TestInfo* const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    const std::string filename = std::string(test_info->name()) + suffix;
    return chromeos::test_utils::GetTestDataPath("oobe_configuration", filename,
                                                 file);
  }

  // Overridden from InProcessBrowserTest:
  void SetUpOnMainThread() override {
    // Set up fake networks.
    // TODO(pmarko): Find a way for FakeShillManagerClient to be initialized
    // automatically (https://crbug.com/847422).
    ShillManagerClient::Get()->GetTestInterface()->SetupDefaultEnvironment();

    OobeBaseTest::SetUpOnMainThread();
    LoadConfiguration();

    // Make sure that OOBE is run as an "official" build.
    LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
        true;
  }

 protected:
  base::ScopedTempDir fake_policy_dir_;
};

class OobeConfigurationEnrollmentTest : public OobeConfigurationTest {
 public:
  OobeConfigurationEnrollmentTest() = default;

  OobeConfigurationEnrollmentTest(const OobeConfigurationEnrollmentTest&) =
      delete;
  OobeConfigurationEnrollmentTest& operator=(
      const OobeConfigurationEnrollmentTest&) = delete;

  ~OobeConfigurationEnrollmentTest() override = default;

 protected:
  EmbeddedPolicyTestServerMixin policy_server_{&mixin_host_};
  // We need fake gaia to fetch device local account tokens.
  FakeGaiaMixin fake_gaia_{&mixin_host_};
  test::EnrollmentUIMixin enrollment_ui_{&mixin_host_};
};

// Check that configuration lets correctly pass Welcome screen.
IN_PROC_BROWSER_TEST_F(OobeConfigurationTest, TestLeaveWelcomeScreen) {
  LoadConfiguration();
  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
}

// Check that language and input methods are set correctly.
IN_PROC_BROWSER_TEST_F(OobeConfigurationTest, TestSwitchLanguageIME) {
  LoadConfiguration();
  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();

  auto* imm = input_method::InputMethodManager::Get();

  // Configuration specified in TestSwitchLanguageIME.json sets non-default
  // input method fo German (xkb:de:neo:ger) to ensure that input method value
  // is propagated correctly. We need to migrate public IME name to internal
  // scheme to be able to compare them.

  const std::string ime_id =
      imm->GetInputMethodUtil()->GetMigratedInputMethod("xkb:de:neo:ger");
  EXPECT_EQ(ime_id, imm->GetActiveIMEState()->GetCurrentInputMethod().id());

  const std::string language_code = g_browser_process->local_state()->GetString(
      language::prefs::kApplicationLocale);
  EXPECT_EQ("de", language_code);
}

// Check that configuration lets correctly select a network by GUID.
IN_PROC_BROWSER_TEST_F(OobeConfigurationTest, TestSelectNetwork) {
  LoadConfiguration();
  OobeScreenWaiter(UpdateView::kScreenId).Wait();
}

// Check that configuration would proceed if there is a connected network.
IN_PROC_BROWSER_TEST_F(OobeConfigurationTest, TestSelectConnectedNetwork) {
  LoadConfiguration();
  OobeScreenWaiter(UpdateView::kScreenId).Wait();
}

// Check that configuration would not proceed with connected network if
// welcome screen is not automated.
IN_PROC_BROWSER_TEST_F(OobeConfigurationTest, TestConnectedNetworkNoWelcome) {
  LoadConfiguration();
  test::WaitForWelcomeScreen();
}

// Check that when configuration has ONC and EULA, we get to update screen.
IN_PROC_BROWSER_TEST_F(OobeConfigurationTest, TestAcceptEula) {
  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::DOWNLOADING);
  status.set_progress(0.1);
  update_engine_client()->set_default_status(status);

  LoadConfiguration();
  OobeScreenWaiter(UpdateView::kScreenId).Wait();
}

// Check that when configuration has requisition, it gets applied at the
// beginning.
IN_PROC_BROWSER_TEST_F(OobeConfigurationTest, TestDeviceRequisition) {
  LoadConfiguration();
  OobeScreenWaiter(UpdateView::kScreenId).Wait();

  EXPECT_EQ(policy::EnrollmentRequisitionManager::GetDeviceRequisition(),
            "some_requisition");
}

}  // namespace ash
