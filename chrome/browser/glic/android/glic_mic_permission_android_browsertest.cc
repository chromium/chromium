// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/android/glic_helper_android.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_api_test.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

namespace {

// Fakes Chrome's microphone dialog and the Android RECORD_AUDIO permission,
// which a browser test cannot interact with.
class FakeMicPermissionUi : public MicPermissionUi {
 public:
  // MicPermissionUi:
  void ShowMicPermissionDialog(
      ui::WindowAndroid* window_android,
      base::OnceCallback<void(bool)> callback) override {
    ++dialog_count_;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), dialog_result_));
  }
  bool HasMicOsPermission(ui::WindowAndroid* window_android) override {
    return has_os_permission_;
  }
  void RequestMicOsPermission(
      ui::WindowAndroid* window_android,
      base::OnceCallback<void(bool)> callback) override {
    ++os_prompt_count_;
    if (os_prompt_result_) {
      has_os_permission_ = true;
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), os_prompt_result_));
  }
  void ShowMicDisabledSnackbar(ui::WindowAndroid* window_android) override {
    ++snackbar_count_;
  }

  void set_dialog_result(bool allowed) { dialog_result_ = allowed; }
  void set_has_os_permission(bool granted) { has_os_permission_ = granted; }
  void set_os_prompt_result(bool granted) { os_prompt_result_ = granted; }

  int dialog_count() const { return dialog_count_; }
  int os_prompt_count() const { return os_prompt_count_; }
  int snackbar_count() const { return snackbar_count_; }

 private:
  bool dialog_result_ = true;
  bool has_os_permission_ = true;
  bool os_prompt_result_ = true;
  int dialog_count_ = 0;
  int os_prompt_count_ = 0;
  int snackbar_count_ = 0;
};

// Exercises the microphone permission flow end to end, from getUserMedia() in
// the Glic client through Chrome's dialog and the Android permission.
// Parameterized on GlicNoWebview.
class GlicMicPermissionAndroidBrowserTest
    : public GlicApiBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  GlicMicPermissionAndroidBrowserTest()
      : GlicApiBrowserTest(
            GlicTestJsPath("./glic_mic_permission_android_browsertest.js")) {
    if (GetParam()) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kGlicNoWebview, features::kGlicVoice},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kGlicVoice},
          /*disabled_features=*/{features::kGlicNoWebview});
    }
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    GlicApiBrowserTest::SetUpCommandLine(command_line);
    // The fake media stream UI would grant access before it reaches Glic.
    command_line->RemoveSwitch(switches::kUseFakeUIForMediaStream);
  }

  void SetUpOnMainThread() override {
    GlicApiBrowserTest::SetUpOnMainThread();
    GetProfile()->GetPrefs()->SetBoolean(prefs::kGlicMicrophoneEnabled, true);
    auto mic_permission_ui = std::make_unique<FakeMicPermissionUi>();
    mic_permission_ui_ = mic_permission_ui.get();
    GlicKeyedServiceFactory::GetGlicKeyedService(GetProfile())
        ->SetMicPermissionUiForTesting(std::move(mic_permission_ui));
  }

  void TearDownOnMainThread() override {
    mic_permission_ui_ = nullptr;
    GlicApiBrowserTest::TearDownOnMainThread();
  }

 protected:
  FakeMicPermissionUi& mic_permission_ui() { return *mic_permission_ui_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<FakeMicPermissionUi> mic_permission_ui_ = nullptr;
};

// Android permission granted: the mic works without any prompt.
IN_PROC_BROWSER_TEST_P(GlicMicPermissionAndroidBrowserTest,
                       testMicAllowedWithOsPermission) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  EXPECT_EQ(0, mic_permission_ui().dialog_count());
  EXPECT_EQ(0, mic_permission_ui().os_prompt_count());
}

// Android permission missing: Chrome's dialog, then the Android prompt.
IN_PROC_BROWSER_TEST_P(GlicMicPermissionAndroidBrowserTest,
                       testMicAllowedAfterDialogAndOsPromptAccepted) {
  mic_permission_ui().set_has_os_permission(false);
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  EXPECT_EQ(1, mic_permission_ui().dialog_count());
  EXPECT_EQ(1, mic_permission_ui().os_prompt_count());
  EXPECT_EQ(0, mic_permission_ui().snackbar_count());
}

// Declining Chrome's dialog fails the request without the Android prompt.
IN_PROC_BROWSER_TEST_P(GlicMicPermissionAndroidBrowserTest,
                       testMicDeniedAfterDialogDeclined) {
  mic_permission_ui().set_has_os_permission(false);
  mic_permission_ui().set_dialog_result(false);
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  EXPECT_EQ(1, mic_permission_ui().dialog_count());
  EXPECT_EQ(0, mic_permission_ui().os_prompt_count());
}

// Denying the Android prompt fails the request and shows the snackbar.
IN_PROC_BROWSER_TEST_P(GlicMicPermissionAndroidBrowserTest,
                       testMicDeniedAfterOsPromptDenied) {
  mic_permission_ui().set_has_os_permission(false);
  mic_permission_ui().set_os_prompt_result(false);
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  EXPECT_EQ(1, mic_permission_ui().dialog_count());
  EXPECT_EQ(1, mic_permission_ui().os_prompt_count());
  EXPECT_EQ(1, mic_permission_ui().snackbar_count());
}

// Glic microphone setting off, Android permission granted: Chrome's dialog is
// shown, the Android prompt is skipped, and the setting is turned on.
IN_PROC_BROWSER_TEST_P(GlicMicPermissionAndroidBrowserTest,
                       testMicAllowedAfterDialogWhenSettingDisabled) {
  GetProfile()->GetPrefs()->SetBoolean(prefs::kGlicMicrophoneEnabled, false);
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  EXPECT_EQ(1, mic_permission_ui().dialog_count());
  EXPECT_EQ(0, mic_permission_ui().os_prompt_count());
  EXPECT_EQ(0, mic_permission_ui().snackbar_count());
  EXPECT_TRUE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kGlicMicrophoneEnabled));
}

// Glic microphone setting off: declining Chrome's dialog fails the request and
// leaves the setting off.
IN_PROC_BROWSER_TEST_P(GlicMicPermissionAndroidBrowserTest,
                       testMicDeniedAfterDialogDeclinedWhenSettingDisabled) {
  GetProfile()->GetPrefs()->SetBoolean(prefs::kGlicMicrophoneEnabled, false);
  mic_permission_ui().set_dialog_result(false);
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  EXPECT_EQ(1, mic_permission_ui().dialog_count());
  EXPECT_EQ(0, mic_permission_ui().os_prompt_count());
  EXPECT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kGlicMicrophoneEnabled));
}

INSTANTIATE_TEST_SUITE_P(,
                         GlicMicPermissionAndroidBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "NoWebview" : "Webview";
                         });

}  // namespace

}  // namespace glic
