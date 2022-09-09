// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/browser/speech/tts_lacros.h"
#include "chromeos/lacros/lacros_test_helper.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_platform.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/test_extension_registry_observer.h"

namespace extensions {

namespace {

void GiveItSomeTime(base::TimeDelta delta) {
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), delta);
  run_loop.Run();
}

}  // namespace

class LacrosTtsApiTest : public ExtensionApiTest,
                         public content::VoicesChangedDelegate {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    content::TtsController::SkipAddNetworkChangeObserverForTests(true);
    content::TtsController* tts_controller =
        content::TtsController::GetInstance();
    tts_controller->SetTtsEngineDelegate(TtsExtensionEngine::GetInstance());
    TtsPlatformImplLacros::EnablePlatformSupportForTesting();
  }

 protected:
  bool IsServiceAvailable() {
    // Ash version must have the change to allow enabling lacros_tts_support
    // for testing.
    return chromeos::IsAshVersionAtLeastForTesting(
        base::Version({106, 0, 5228}));
  }

  bool HasVoiceWithName(const std::string& name) {
    std::vector<content::VoiceData> voices;
    content::TtsController::GetInstance()->GetVoices(profile(), GURL(),
                                                     &voices);
    for (auto& voice : voices) {
      if (voice.name == name)
        return true;
    }

    return false;
  }

  // content::VoicesChangedDelegate:
  void OnVoicesChanged() override {
    voices_changed_ = true;
    std::vector<content::VoiceData> voices;
    content::TtsController::GetInstance()->GetVoices(profile(), GURL(),
                                                     &voices);
  }

  bool VoicesChangedNotified() { return voices_changed_; }
  void ResetVoicesChanged() { voices_changed_ = false; }

  void WaitUntilVoicesLoaded() {
    while (!HasVoiceWithName("Alice")) {
      GiveItSomeTime(base::Milliseconds(100));
    }
  }

  void WaitUntilVoicesUnloaded() {
    while (HasVoiceWithName("Alice")) {
      GiveItSomeTime(base::Milliseconds(100));
    }
  }

 private:
  bool voices_changed_ = false;
};

//
// TTS Engine tests.
//
IN_PROC_BROWSER_TEST_F(LacrosTtsApiTest, LoadAndUnloadLacrosTtsEngine) {
  if (!IsServiceAvailable())
    GTEST_SKIP() << "Unsupported ash version.";

  // Before tts engine extension is loaded, verify the internal states are
  // clean.
  EXPECT_FALSE(VoicesChangedNotified());
  EXPECT_FALSE(HasVoiceWithName("Alice"));
  EXPECT_FALSE(HasVoiceWithName("Pat"));
  EXPECT_FALSE(HasVoiceWithName("Cat"));

  // Load tts engine extension and register the tts engine events.
  content::TtsController::GetInstance()->AddVoicesChangedDelegate(this);
  ASSERT_TRUE(RunExtensionTest("tts_engine/lacros_register_engine", {},
                               {.ignore_manifest_warnings = true}))
      << message_;

  // Wait until Lacros gets the voices registered by the tts engine extension.
  WaitUntilVoicesLoaded();

  // Verify TtsController notifies VoicesChangedDelegate for the voices change.
  EXPECT_TRUE(VoicesChangedNotified());

  // Verify all the voices from tts engine extension are returned by
  // TtsController::GetVoices().
  std::vector<content::VoiceData> voices;
  content::TtsController::GetInstance()->GetVoices(profile(), GURL(), &voices);
  EXPECT_TRUE(HasVoiceWithName("Alice"));
  EXPECT_TRUE(HasVoiceWithName("Pat"));
  EXPECT_TRUE(HasVoiceWithName("Cat"));

  ResetVoicesChanged();

  // Uninstall tts engine extension.
  extensions::TestExtensionRegistryObserver observer(
      extensions::ExtensionRegistry::Get(profile()),
      last_loaded_extension_id());
  UninstallExtension(last_loaded_extension_id());
  observer.WaitForExtensionUninstalled();

  WaitUntilVoicesUnloaded();

  // Verify TtsController notifies VoicesChangedDelegate for the voices change.
  EXPECT_TRUE(VoicesChangedNotified());

  // Verify the voices from the tts engine extensions are unloaded in Lacros
  // TtsController.
  EXPECT_FALSE(HasVoiceWithName("Alice"));
  EXPECT_FALSE(HasVoiceWithName("Pat"));
  EXPECT_FALSE(HasVoiceWithName("Cat"));
}

}  // namespace extensions
