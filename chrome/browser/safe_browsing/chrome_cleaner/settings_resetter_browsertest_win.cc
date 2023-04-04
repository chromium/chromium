// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/settings_resetter_win.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile_resetter/brandcoded_default_settings.h"
#include "chrome/browser/profile_resetter/profile_resetter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace {

using ::testing::_;
using ::testing::StrictMock;

// Creates a new profile from the UI thread.
Profile* CreateProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return &profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GenerateNextProfileDirectoryPath());
}

// Returns true if |profile| is tagged for settings reset.
bool ProfileIsTagged(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(prefs::kChromeCleanerResetPending);
}

// Saves |value| in the registry at the value name corresponding to the cleanup
// completed state.
void SetCompletedState(DWORD value) {
  std::wstring cleaner_key_path(
      chrome_cleaner::kSoftwareRemovalToolRegistryKey);
  cleaner_key_path.append(L"\\").append(chrome_cleaner::kCleanerSubKey);

  LONG result =
      base::win::RegKey(HKEY_CURRENT_USER, cleaner_key_path.c_str(),
                        KEY_SET_VALUE)
          .WriteValue(chrome_cleaner::kCleanupCompletedValueName, value);
  ASSERT_EQ(ERROR_SUCCESS, result);
}

using ChromeCleanerTagForResettingTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(ChromeCleanerTagForResettingTest, Run) {
  Browser* browser = chrome::FindLastActive();
  ASSERT_TRUE(browser);
  Profile* profile = browser->profile();
  ASSERT_TRUE(profile);

  PostCleanupSettingsResetter resetter;
  resetter.TagForResetting(profile);
  EXPECT_TRUE(ProfileIsTagged(profile));
}

class SettingsResetterTestDelegate
    : public PostCleanupSettingsResetter::Delegate {
 public:
  explicit SettingsResetterTestDelegate(int* num_resets)
      : num_resets_(num_resets) {}

  SettingsResetterTestDelegate(const SettingsResetterTestDelegate&) = delete;
  SettingsResetterTestDelegate& operator=(const SettingsResetterTestDelegate&) =
      delete;

  ~SettingsResetterTestDelegate() override = default;

  void FetchDefaultSettings(
      DefaultSettingsFetcher::SettingsCallback callback) override {
    std::move(callback).Run(std::make_unique<BrandcodedDefaultSettings>());
  }

  // Returns a MockProfileResetter that requires Reset() be called.
  std::unique_ptr<ProfileResetter> GetProfileResetter(
      Profile* profile) override {
    ++(*num_resets_);
    auto mock_profile_resetter =
        std::make_unique<StrictMock<MockProfileResetter>>(profile);
    EXPECT_CALL(*mock_profile_resetter, MockReset(_, _, _));
    return std::move(mock_profile_resetter);
  }

 private:
  raw_ptr<int> num_resets_;
};

// Indicates the possible values to be written to the registry for cleanup
// completion.
enum class CleanupCompletionState {
  // No value will be written to the registry; cleanup should be considered
  // as not completed.
  kNotAvailable,
  // Value 0 will be written to the registry; cleanup should be considered as
  // not completed.
  kNotCompleted,
  // Value 1 will be written to the registry; cleanup should be considered
  // as completed.
  kCompleted,
  // A non-zero value different than 1 will be written to the registry; cleanup
  // should be considered as not completed.
  kInvalidValue,
};

// Param for this test:
//  - CleanupCompletionState completion_state: indicates the value to be
//        written to the registry for cleanup completion.
class ChromeCleanerResetTaggedProfilesTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<CleanupCompletionState> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    completion_state_ = GetParam();
    ASSERT_TRUE(completion_state_ >= CleanupCompletionState::kNotAvailable &&
                completion_state_ <= CleanupCompletionState::kInvalidValue);
  }

 protected:
  CleanupCompletionState completion_state_;

  registry_util::RegistryOverrideManager registry_override_manager_;
};

IN_PROC_BROWSER_TEST_P(ChromeCleanerResetTaggedProfilesTest, Run) {
  ASSERT_NO_FATAL_FAILURE(
      registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
  switch (completion_state_) {
    case CleanupCompletionState::kNotAvailable:
      // No value written to the registry.
      break;

    case CleanupCompletionState::kNotCompleted:
      SetCompletedState(0);
      break;

    case CleanupCompletionState::kCompleted:
      SetCompletedState(1);
      break;

    case CleanupCompletionState::kInvalidValue:
      SetCompletedState(42);
  }

  // Profile objects are owned by ProfileManager.
  Profile* profile1 = CreateProfile();
  ASSERT_TRUE(profile1);
  Profile* profile2 = CreateProfile();
  ASSERT_TRUE(profile2);
  Profile* profile3 = CreateProfile();
  ASSERT_TRUE(profile3);

  profile1->GetPrefs()->SetBoolean(prefs::kChromeCleanerResetPending, true);
  profile3->GetPrefs()->SetBoolean(prefs::kChromeCleanerResetPending, true);

  int num_resets = 0;
  auto delegate = std::make_unique<SettingsResetterTestDelegate>(&num_resets);

  PostCleanupSettingsResetter resetter;
  base::RunLoop run_loop_for_reset;
  resetter.ResetTaggedProfiles({profile1, profile2, profile3},
                               run_loop_for_reset.QuitClosure(),
                               std::move(delegate));
  run_loop_for_reset.Run();

  // Profiles 1 and 3 should be reset only if completion_state_ is kCompleted.
  // Profile 2 should remain not-tagged by the operation.
  bool reset_expected = completion_state_ == CleanupCompletionState::kCompleted;
  EXPECT_EQ(reset_expected ? 2 : 0, num_resets);
  EXPECT_EQ(!reset_expected, ProfileIsTagged(profile1));
  EXPECT_EQ(false, ProfileIsTagged(profile2));
  EXPECT_EQ(!reset_expected, ProfileIsTagged(profile3));
}

INSTANTIATE_TEST_SUITE_P(
    Default,
    ChromeCleanerResetTaggedProfilesTest,
    testing::Values(CleanupCompletionState::kNotAvailable,
                    CleanupCompletionState::kNotCompleted,
                    CleanupCompletionState::kCompleted,
                    CleanupCompletionState::kInvalidValue));

}  // namespace
}  // namespace safe_browsing
