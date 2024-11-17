// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_element_identifiers.h"
#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/chromevox_panel.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/chromeos/crosier/ash_integration_test.h"
#include "chrome/test/base/chromeos/crosier/upstart.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/process_manager.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

namespace {
constexpr char kGoogleTtsJobName[] = "googletts";
constexpr char kGoogleTestSupportPath[] =
    "crosier_js_helpers/google_tts_test_support.js";
}  // namespace

// Integration tests for the ChromeVox screen reader. These tests run on
// physical devices and VMs running a complete ChromeOS image.
class SpokenFeedbackIntegrationTest : public AshIntegrationTest {
 public:
  SpokenFeedbackIntegrationTest() = default;
  ~SpokenFeedbackIntegrationTest() override = default;
  SpokenFeedbackIntegrationTest(const SpokenFeedbackIntegrationTest&) = delete;
  SpokenFeedbackIntegrationTest& operator=(
      const SpokenFeedbackIntegrationTest&) = delete;
};

IN_PROC_BROWSER_TEST_F(SpokenFeedbackIntegrationTest, KeyboardShortcut) {
  SetupContextWidget();
  RunTestSequence(Log("Enabling ChromeVox with the keyboard shortcut"), Do([] {
                    ui_controls::SendKeyPress(/*window=*/nullptr, ui::VKEY_Z,
                                              /*control=*/true, /*shift=*/false,
                                              /*alt=*/true, /*command=*/false);
                  }),
                  Log("Waiting for the ChromeVox panel to show"),
                  WaitForShow(kChromeVoxPanelElementId));
}

// Integration tests for the Google TTS engine on ChromeOS. These tests run on
// physical devices and VMs running a complete ChromeOS image.
class GoogleTtsIntegrationTest : public AshIntegrationTest {
 public:
  GoogleTtsIntegrationTest() {
    // Google TTS files are mounted by upstart when the UI job is started.
    // However, Crosier (which is the UI job in this context) is started by a
    // test script, not upstart. The result is that Google TTS will never get
    // mounted because upstart doesn't know that the UI job has started.
    // Therefore, we need to manually start googletts so that files are
    // available at runtime.
    upstart::StartJob(kGoogleTtsJobName);
    upstart::WaitForJobStatus(kGoogleTtsJobName, upstart::Goal::kStart,
                              upstart::State::kRunning,
                              upstart::WrongGoalPolicy::kReject);
  }

  ~GoogleTtsIntegrationTest() override {
    // Since googletts is manually started in `SetUpOnMainThread`, we need to
    // manually stop it.
    upstart::StopJob(kGoogleTtsJobName);
    upstart::WaitForJobStatus(kGoogleTtsJobName, upstart::Goal::kStop,
                              upstart::State::kWaiting,
                              upstart::WrongGoalPolicy::kReject);
    TestSudoHelperClient().RunCommand("rm -rf /tmp/tts/");
  }
  GoogleTtsIntegrationTest(const GoogleTtsIntegrationTest&) = delete;
  GoogleTtsIntegrationTest& operator=(const GoogleTtsIntegrationTest&) = delete;

  void EnableGoogleTts() {
    base::RunLoop loop{base::RunLoop::Type::kNestableTasksAllowed};
    Profile* profile = AccessibilityManager::Get()->profile();
    extensions::ExtensionHostTestHelper host_helper(
        profile, extension_misc::kGoogleSpeechSynthesisExtensionId);
    TtsExtensionEngine::GetInstance()->LoadBuiltInTtsEngine(profile);
    extensions::ProcessManager::Get(profile)->WakeEventPage(
        extension_misc::kGoogleSpeechSynthesisExtensionId,
        base::BindLambdaForTesting([&loop](bool success) { loop.Quit(); }));
    loop.Run();
    host_helper.WaitForHostCompletedFirstLoad();
  }

  void InjectTestSupportScript() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath source_dir;
    base::FilePath test_support_path(kGoogleTestSupportPath);
    std::string script;
    ASSERT_TRUE(base::ReadFileToString(test_support_path, &script))
        << test_support_path;
    ExecuteGoogleTtsScript(script);
  }

  void EnsureTtsEngineLoaded() {
    ExecuteGoogleTtsScript("googleTtsTestSupport.ensureLoaded();");
  }

  void SendSpeechRequest(const std::string& utterance) {
    std::string script = base::StringPrintf("googleTtsTestSupport.speak(`%s`)",
                                            utterance.c_str());
    ExecuteGoogleTtsScript(script);
  }

  void ConsumeUtterance(const std::string& utterance) {
    std::string script = base::StringPrintf(
        "googleTtsTestSupport.consume(`%s`)", utterance.c_str());
    ExecuteGoogleTtsScript(script);
  }

 private:
  void ExecuteGoogleTtsScript(const std::string& script) {
    extensions::browsertest_util::ExecuteScriptInBackgroundPage(
        /*context=*/AccessibilityManager::Get()->profile(),
        /*extension_id=*/extension_misc::kGoogleSpeechSynthesisExtensionId,
        /*script=*/script);
  }
};

IN_PROC_BROWSER_TEST_F(GoogleTtsIntegrationTest, Speak) {
  SetupContextWidget();
  RunTestSequence(Log("Setting up Google TTS extension"), Do([this] {
                    EnableGoogleTts();
                    InjectTestSupportScript();
                    EnsureTtsEngineLoaded();
                  }),
                  Log("Requesting and verifying speech"), Do([this] {
                    SendSpeechRequest("Test");
                    ConsumeUtterance("Test");
                  }));
}

}  // namespace ash
