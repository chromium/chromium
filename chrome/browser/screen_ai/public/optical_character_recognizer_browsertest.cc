// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/public/optical_character_recognizer.h"

#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "services/screen_ai/public/cpp/utilities.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_features.mojom-features.h"

class OpticalCharacterRecognizerTest
    : public InProcessBrowserTest,
      public screen_ai::ScreenAIInstallState::Observer,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  OpticalCharacterRecognizerTest() {
    std::vector<base::test::FeatureRef> enabled_features(
        {::features::kScreenAITestMode});
    std::vector<base::test::FeatureRef> disabled_features;

    if (IsOcrServiceEnabled()) {
      enabled_features.push_back(ax::mojom::features::kScreenAIOCREnabled);
    } else {
      disabled_features.push_back(ax::mojom::features::kScreenAIOCREnabled);
    }

    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  ~OpticalCharacterRecognizerTest() override = default;

  bool IsOcrServiceEnabled() const { return std::get<0>(GetParam()); }
  bool IsLibraryAvailable() const {
#if BUILDFLAG(ENABLE_SCREEN_AI_BROWSERTESTS)
    return std::get<1>(GetParam());
#else
    return false;
#endif
  }

  bool IsOcrAvailable() const {
    return IsOcrServiceEnabled() && IsLibraryAvailable();
  }

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    if (IsLibraryAvailable()) {
      screen_ai::ScreenAIInstallState::GetInstance()->SetComponentFolder(
          screen_ai::GetComponentBinaryPathForTests().DirName());
    } else {
      // Set an observer to mark download as failed when requested.
      component_download_observer_.Observe(
          screen_ai::ScreenAIInstallState::GetInstance());
    }
  }

  // ScreenAIInstallState::Observer:
  void StateChanged(screen_ai::ScreenAIInstallState::State state) override {
    if (state == screen_ai::ScreenAIInstallState::State::kDownloading &&
        !IsLibraryAvailable()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce([]() {
            screen_ai::ScreenAIInstallState::GetInstance()->SetState(
                screen_ai::ScreenAIInstallState::State::kDownloadFailed);
          }));
    }
  }

 private:
  base::ScopedObservation<screen_ai::ScreenAIInstallState,
                          screen_ai::ScreenAIInstallState::Observer>
      component_download_observer_{this};
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(OpticalCharacterRecognizerTest,
                       CreateWithStatusCallback) {
  base::RunLoop run_loop;

  scoped_refptr<screen_ai::OpticalCharacterRecognizer> ocr =
      screen_ai::OpticalCharacterRecognizer::CreateWithStatusCallback(
          browser()->profile(), base::BindOnce(
                                    [](base::RunLoop* run_loop,
                                       bool expected_result, bool successful) {
                                      EXPECT_EQ(expected_result, successful);
                                      run_loop->Quit();
                                    },
                                    &run_loop, IsOcrAvailable()));
  run_loop.Run();
  EXPECT_TRUE(ocr);
}

// TODO(crbug.com/327181467): Add tests to cover other functions of
// `OpticalCharacterRecognizer`.

INSTANTIATE_TEST_SUITE_P(
    All,
    OpticalCharacterRecognizerTest,
    ::testing::Combine(testing::Bool(), testing::Bool()),
    [](const testing::TestParamInfo<std::tuple<bool, bool>>& info) {
      return base::StringPrintf(
          "OCR_%s_Library_%s", std::get<0>(info.param) ? "Enabled" : "Disabled",
          std::get<1>(info.param) ? "Available" : "Unavailable");
    });
