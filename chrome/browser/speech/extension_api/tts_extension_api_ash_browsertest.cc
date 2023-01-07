// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/crosapi/ash_requires_lacros_extension_apitest.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/browser/speech/tts_crosapi_util.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/test/browser_test.h"

using crosapi::AshRequiresLacrosExtensionApiTest;

namespace {

void GiveItSomeTime(base::TimeDelta delta) {
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), delta);
  run_loop.Run();
}

}  // namespace

namespace extensions {

// Test tts and ttsEngine APIs with Lacros Tts support enabled, which
// requires Lacros running to exercise crosapi calls.
class AshTtsApiTest : public AshRequiresLacrosExtensionApiTest,
                      public content::VoicesChangedDelegate {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    AshRequiresLacrosExtensionApiTest::SetUpInProcessBrowserTestFixture();

    // Enable Lacros tts support feature, and disable the 1st party Ash
    // extension keeplist feature so that it will allow loading test extension
    // in Ash in Lacros only mode.
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitWithFeatures(
        {}, {chromeos::features::kEnforceAshExtensionKeeplist,
             ash::features::kDisableLacrosTtsSupport});

    content::TtsController::SkipAddNetworkChangeObserverForTests(true);
    content::TtsController* tts_controller =
        content::TtsController::GetInstance();
    TtsExtensionEngine::GetInstance()->DisableBuiltInTTSEngineForTesting();
    tts_controller->SetTtsEngineDelegate(TtsExtensionEngine::GetInstance());
  }

  void TearDownInProcessBrowserTestFixture() override {
    scoped_feature_list_.reset(nullptr);
  }

 protected:
  bool HasVoiceWithName(const std::string& name) {
    std::vector<content::VoiceData> voices;

    content::TtsController::GetInstance()->GetVoices(profile(), GURL(),
                                                     &voices);

    for (const auto& voice : voices) {
      if (voice.name == name)
        return true;
    }

    return false;
  }

  bool FoundVoiceInMojoVoices(
      const std::string& voice_name,
      const std::vector<crosapi::mojom::TtsVoicePtr>& mojo_voices) {
    for (const auto& voice : mojo_voices) {
      if (voice_name == voice->voice_name)
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
    expected_voice_loaded_ = false;
    for (const auto& voice : voices) {
      if (voice.name == "Amy") {
        expected_voice_loaded_ = true;
        break;
      }
    }
  }

  void WaitUntilVoicesLoaded() {
    while (!expected_voice_loaded_) {
      GiveItSomeTime(base::Milliseconds(100));
    }
  }

  bool VoicesChangedNotified() { return voices_changed_; }
  void ResetVoicesChanged() { voices_changed_ = false; }

  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  test::AshBrowserTestStarter ash_starter_;

 private:
  bool voices_changed_ = false;
  bool expected_voice_loaded_ = false;
};

//
// TTS Engine tests.
//

IN_PROC_BROWSER_TEST_F(AshTtsApiTest, RegisterEngine) {
  if (!ash_starter_.HasLacrosArgument())
    return;

  EXPECT_FALSE(VoicesChangedNotified());
  EXPECT_FALSE(HasVoiceWithName("Amy"));
  EXPECT_FALSE(HasVoiceWithName("Alex"));
  EXPECT_FALSE(HasVoiceWithName("Amanda"));

  ResetVoicesChanged();
  content::TtsController::GetInstance()->AddVoicesChangedDelegate(this);
  ASSERT_TRUE(
      RunExtensionTest("tts_engine/lacros_tts_support/register_ash_engine", {},
                       {.ignore_manifest_warnings = true}))
      << message_;

  WaitUntilVoicesLoaded();

  EXPECT_TRUE(VoicesChangedNotified());

  // Verify all the voices from tts engine extension are returned by
  // TtsController::GetVoices().
  std::vector<content::VoiceData> voices;
  content::TtsController::GetInstance()->GetVoices(profile(), GURL(), &voices);
  EXPECT_TRUE(HasVoiceWithName("Amy"));
  EXPECT_TRUE(HasVoiceWithName("Alex"));
  EXPECT_TRUE(HasVoiceWithName("Amanda"));

  // Verify all the voices are loaded at Lacros side.
  crosapi::mojom::StandaloneBrowserTestControllerAsyncWaiter waiter(
      GetStandaloneBrowserTestController());

  std::vector<crosapi::mojom::TtsVoicePtr> mojo_voices;
  while (mojo_voices.size() == 0) {
    waiter.GetTtsVoices(&mojo_voices);
    if (mojo_voices.size() > 0)
      break;
    GiveItSomeTime(base::Milliseconds(100));
  }

  EXPECT_TRUE(FoundVoiceInMojoVoices("Amy", mojo_voices));
  EXPECT_TRUE(FoundVoiceInMojoVoices("Alex", mojo_voices));
  EXPECT_TRUE(FoundVoiceInMojoVoices("Amanda", mojo_voices));
}

}  // namespace extensions
