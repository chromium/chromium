// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_browser_session.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_helper.h"
#include "ash/wm/overview/overview_controller.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/browser_delegate/browser_controller_impl.h"
#include "chrome/browser/chromeos/app_mode/kiosk_metrics_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace chromeos {

namespace {

constexpr char kTestAppId[] = "aaaabbbbaaaabbbbaaaabbbbaaaabbbb";
constexpr char kTestWebAppId1[] = "test_web_app_id1";

enum class KioskType { kChromeApp = 0, kWebApp = 1, kIwa = 2 };
enum class NoParam {};

}  // namespace

// TODO(b/271336749): split kiosk_browser_session_unittest.cc file into smaller
// test files.
template <typename KioskBrowserSessionParamType>
class KioskBrowserSessionBaseTest
    : public ::testing::TestWithParam<KioskBrowserSessionParamType> {
 public:
  KioskBrowserSessionBaseTest() = default;
  KioskBrowserSessionBaseTest(const KioskBrowserSessionBaseTest&) = delete;
  KioskBrowserSessionBaseTest& operator=(const KioskBrowserSessionBaseTest&) =
      delete;

  static void SetUpTestSuite() {
    chromeos::PowerManagerClient::InitializeFake();
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ash::AshTestHelper::InitParams init_params;
    init_params.post_subsystems_teardown_callback =
        base::BindOnce(&KioskBrowserSessionBaseTest::OnSubsystemsTornDown,
                       base::Unretained(this));
    ash_test_helper_.SetUp(std::move(init_params));
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());
    profile_ = testing_profile_manager_->CreateTestingProfile("test@user");
  }

  void TearDown() override {
    kiosk_browser_session_.reset();
    ash_test_helper_.TearDown();
  }

  void OnSubsystemsTornDown() {
    profile_ = nullptr;
    testing_profile_manager_.reset();
  }

  static void TearDownTestSuite() { chromeos::PowerManagerClient::Shutdown(); }

  PrefService* local_state() {
    return TestingBrowserProcess::GetGlobal()->local_state();
  }

  TestingProfile* profile() { return profile_; }

  base::HistogramTester* histogram() { return &histogram_; }

  // Simulate starting a web kiosk session.
  void StartWebKioskSession(const std::string& web_app_id = kTestWebAppId1) {
    kiosk_browser_session_ = KioskBrowserSession::CreateForTesting(
        local_state(), profile(), base::DoNothing(), {crash_path().value()});
    kiosk_browser_session_->InitForWebKiosk(web_app_id);

    task_environment_.RunUntilIdle();
  }

  // Simulate starting an IWA kiosk session.
  void StartIwaKioskSession(const std::string& iwa_id = kTestWebAppId1) {
    kiosk_browser_session_ = KioskBrowserSession::CreateForTesting(
        local_state(), profile(), base::DoNothing(), {crash_path().value()});
    kiosk_browser_session_->InitForIwaKiosk(iwa_id);
  }

  // Simulate starting a chrome app kiosk session.
  void StartChromeAppKioskSession() {
    kiosk_browser_session_ = std::make_unique<KioskBrowserSession>(
        local_state(), profile(), base::DoNothing());
    kiosk_browser_session_->InitForChromeAppKiosk(kTestAppId);
  }

  bool IsSessionShuttingDown() const {
    return kiosk_browser_session_->is_shutting_down();
  }

  PrefService* GetPrefs() { return profile()->GetPrefs(); }

  base::FilePath crash_path() const { return temp_dir_.GetPath(); }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  ash::AshTestHelper ash_test_helper_;
  ash::BrowserControllerImpl browser_controller_;

  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  raw_ptr<TestingProfile> profile_;
  base::HistogramTester histogram_;
  std::unique_ptr<KioskBrowserSession> kiosk_browser_session_;
};

using KioskBrowserSessionTest = KioskBrowserSessionBaseTest<NoParam>;

TEST_F(KioskBrowserSessionTest, IwaKioskSessionState) {
  StartIwaKioskSession();
  histogram()->ExpectBucketCount(kKioskSessionStateHistogram,
                                 KioskSessionState::kIwaStarted, 1);
  histogram()->ExpectTotalCount(kKioskSessionCountPerDayHistogram, 1);
}

TEST_F(KioskBrowserSessionTest, ChromeAppKioskSessionState) {
  StartChromeAppKioskSession();
  histogram()->ExpectBucketCount(kKioskSessionStateHistogram,
                                 KioskSessionState::kStarted, 1);
  histogram()->ExpectTotalCount(kKioskSessionCountPerDayHistogram, 1);
}

// Kiosk type agnostic test class. Runs all tests for web and chrome app kiosks.
class KioskBrowserSessionTroubleshootingTest
    : public KioskBrowserSessionBaseTest<KioskType> {
 public:
  void SetUpKioskSession() {
    switch (GetKioskType()) {
      case KioskType::kChromeApp:
        StartChromeAppKioskSession();
        break;
      case KioskType::kWebApp:
        StartWebKioskSession();
        break;
      case KioskType::kIwa:
        StartIwaKioskSession();
        break;
    }
  }

  void UpdateTroubleshootingToolsPolicy(bool enable) {
    GetPrefs()->SetBoolean(ash::prefs::kKioskTroubleshootingToolsEnabled,
                           enable);
  }

 private:
  KioskType GetKioskType() const { return GetParam(); }
};

TEST_P(KioskBrowserSessionTroubleshootingTest,
       EnableTroubleshootingToolsDuringSession) {
  SetUpKioskSession();
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);

  // Kiosk session should shoutdown only if policy is changed from enable to
  // disable.
  EXPECT_FALSE(IsSessionShuttingDown());

  UpdateTroubleshootingToolsPolicy(/*enable=*/false);
  EXPECT_TRUE(IsSessionShuttingDown());
}

TEST_P(KioskBrowserSessionTroubleshootingTest,
       EnableTroubleshootingToolsBeforeSessionStarted) {
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);
  SetUpKioskSession();

  UpdateTroubleshootingToolsPolicy(/*enable=*/false);
  EXPECT_TRUE(IsSessionShuttingDown());
}

INSTANTIATE_TEST_SUITE_P(KioskBrowserSessionTroubleshootingTools,
                         KioskBrowserSessionTroubleshootingTest,
                         ::testing::Values(KioskType::kChromeApp,
                                           KioskType::kWebApp,
                                           KioskType::kIwa));

class FakeNewWindowDelegate : public ash::TestNewWindowDelegate {
 public:
  FakeNewWindowDelegate() = default;
  ~FakeNewWindowDelegate() override = default;

  void NewWindow(bool incognito, bool should_trigger_session_restore) override {
    new_window_called_ = true;
  }

  void NewTab() override { new_tab_called_ = true; }

  void ShowTaskManager(bool from_context_menu) override {
    task_manager_called_ = true;
  }

  void OpenFeedbackPage(FeedbackSource source,
                        const std::string& description_template) override {
    open_feedback_page_called_ = true;
  }

  bool is_new_window_called() const { return new_window_called_; }
  bool is_new_tab_called() const { return new_tab_called_; }
  bool is_task_manager_called() const { return task_manager_called_; }
  bool is_open_feedback_page_called() const {
    return open_feedback_page_called_;
  }

 private:
  bool new_window_called_ = false;
  bool new_tab_called_ = false;
  bool task_manager_called_ = false;
  bool open_feedback_page_called_ = false;
};

// Tests actions after pressing troubleshooting shortcuts. Runs all tests for
// web and chrome app kiosks.
class KioskBrowserSessionTroubleshootingShortcutsTest
    : public KioskBrowserSessionTroubleshootingTest {
 public:
  static bool ProcessInController(const ui::Accelerator& accelerator) {
    return ash::Shell::Get()->accelerator_controller()->Process(accelerator);
  }

  void SetUp() override {
    KioskBrowserSessionTroubleshootingTest::SetUp();

    ash::SessionInfo info;
    info.is_running_in_app_mode = true;
    info.state = session_manager::SessionState::ACTIVE;
    ash::Shell::Get()->session_controller()->SetSessionInfo(info);
  }

  bool IsOverviewToggled() const {
    ash::OverviewController* overview_controller =
        ash::Shell::Get()->overview_controller();
    return overview_controller->InOverviewSession();
  }

  bool IsNewWindowCalled() const {
    return fake_new_window_delegate_.is_new_window_called();
  }

  bool IsNewTabCalled() const {
    return fake_new_window_delegate_.is_new_tab_called();
  }

  bool IsTaskManagerCalled() const {
    return fake_new_window_delegate_.is_task_manager_called();
  }

  bool IsOpenFeedbackPageCalled() const {
    return fake_new_window_delegate_.is_open_feedback_page_called();
  }

 protected:
  ui::Accelerator new_window_accelerator =
      ui::Accelerator(ui::VKEY_N, ui::EF_CONTROL_DOWN);
  ui::Accelerator task_manager_accelerator =
      ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN);
  ui::Accelerator open_feedback_page_accelerator =
      ui::Accelerator(ui::VKEY_I, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
  ui::Accelerator toggle_overview_accelerator =
      ui::Accelerator(ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_NONE);

 private:
  FakeNewWindowDelegate fake_new_window_delegate_;
};

TEST_P(KioskBrowserSessionTroubleshootingShortcutsTest,
       NewWindowShortcutEnabled) {
  SetUpKioskSession();
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);

  ProcessInController(new_window_accelerator);
  EXPECT_TRUE(IsNewWindowCalled());
}

// Just confirm that other shortcuts (e.g. new tab) do not work.
TEST_P(KioskBrowserSessionTroubleshootingShortcutsTest,
       NewTabShortcutIsNoAction) {
  SetUpKioskSession();
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);

  ProcessInController(ui::Accelerator(ui::VKEY_T, ui::EF_CONTROL_DOWN));
  EXPECT_FALSE(IsNewTabCalled());
  EXPECT_FALSE(IsNewWindowCalled());
}

TEST_P(KioskBrowserSessionTroubleshootingShortcutsTest,
       NewWindowShortcutNoActionByDefault) {
  SetUpKioskSession();

  ProcessInController(new_window_accelerator);
  EXPECT_FALSE(IsNewWindowCalled());
}

TEST_P(KioskBrowserSessionTroubleshootingShortcutsTest,
       NewWindowShortcutNoActionIfPolicyDisabled) {
  SetUpKioskSession();
  UpdateTroubleshootingToolsPolicy(/*enable=*/false);

  ProcessInController(new_window_accelerator);
  EXPECT_FALSE(IsNewWindowCalled());
}

TEST_P(KioskBrowserSessionTroubleshootingShortcutsTest,
       TaskManagerShortcutEnabled) {
  SetUpKioskSession();
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);

  ProcessInController(task_manager_accelerator);
  EXPECT_TRUE(IsTaskManagerCalled());
}

TEST_P(KioskBrowserSessionTroubleshootingShortcutsTest,
       TaskManagerShortcutNoActionByDefault) {
  SetUpKioskSession();

  ProcessInController(task_manager_accelerator);
  EXPECT_FALSE(IsTaskManagerCalled());
}

TEST_P(KioskBrowserSessionTroubleshootingShortcutsTest,
       TaskManagerShortcutNoActionIfPolicyDisabled) {
  SetUpKioskSession();
  UpdateTroubleshootingToolsPolicy(/*enable=*/false);

  ProcessInController(task_manager_accelerator);
  EXPECT_FALSE(IsTaskManagerCalled());
}

TEST_P(KioskBrowserSessionTroubleshootingShortcutsTest,
       OpenFeedbackPageShortcutEnabled) {
  SetUpKioskSession();
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);

  ProcessInController(open_feedback_page_accelerator);
  EXPECT_TRUE(IsOpenFeedbackPageCalled());
}

TEST_P(KioskBrowserSessionTroubleshootingShortcutsTest,
       OpenFeedbackPageShortcutNoActionByDefault) {
  SetUpKioskSession();

  ProcessInController(open_feedback_page_accelerator);
  EXPECT_FALSE(IsOpenFeedbackPageCalled());
}

TEST_P(KioskBrowserSessionTroubleshootingShortcutsTest,
       OpenFeedbackPageShortcutNoActionIfPolicyDisabled) {
  SetUpKioskSession();
  UpdateTroubleshootingToolsPolicy(/*enable=*/false);

  ProcessInController(open_feedback_page_accelerator);
  EXPECT_FALSE(IsOpenFeedbackPageCalled());
}

TEST_P(KioskBrowserSessionTroubleshootingShortcutsTest,
       ToggleOverviewShortcutEnabled) {
  SetUpKioskSession();
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);

  ProcessInController(toggle_overview_accelerator);
  EXPECT_TRUE(IsOverviewToggled());
}

TEST_P(KioskBrowserSessionTroubleshootingShortcutsTest,
       ToggleOverviewShortcutNoActionByDefault) {
  SetUpKioskSession();

  ProcessInController(toggle_overview_accelerator);
  EXPECT_FALSE(IsOverviewToggled());
}

TEST_P(KioskBrowserSessionTroubleshootingShortcutsTest,
       ToggleOverviewShortcutNoActionIfPolicyDisabled) {
  SetUpKioskSession();
  UpdateTroubleshootingToolsPolicy(/*enable=*/false);

  ProcessInController(toggle_overview_accelerator);
  EXPECT_FALSE(IsOverviewToggled());
}

INSTANTIATE_TEST_SUITE_P(KioskBrowserSessionTroubleshootingShortcuts,
                         KioskBrowserSessionTroubleshootingShortcutsTest,
                         ::testing::Values(KioskType::kChromeApp,
                                           KioskType::kWebApp,
                                           KioskType::kIwa));

}  // namespace chromeos
