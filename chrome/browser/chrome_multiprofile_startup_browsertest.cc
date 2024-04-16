// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Matcher;
using ::testing::Mock;
using ::testing::Property;
using ::testing::ValuesIn;

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
#error Not supported on this platform.
#endif

namespace {

class MockMainExtraParts : public ChromeBrowserMainExtraParts {
 public:
  MOCK_METHOD(void, PreProfileInit, ());
  MOCK_METHOD(void, PostProfileInit, (Profile*, bool));
  MOCK_METHOD(void, PreBrowserStart, ());
  MOCK_METHOD(void, PostBrowserStart, ());
  MOCK_METHOD(void, PreMainMessageLoopRun, ());
};

const char kOtherProfileDirPath[] = "Other";

MATCHER_P(BaseNameEquals,
          basename,
          base::StrCat({negation ? "doesn't equal " : "equals ", basename})) {
  return arg == base::FilePath::FromASCII(basename);
}

Matcher<Profile*> HasBaseName(const char* basename) {
  return Property("basename", &Profile::GetBaseName, BaseNameEquals(basename));
}

struct MultiProfileStartupTestParam {
  // Whether the profile picker should be shown on startup.
  const bool should_show_profile_picker;

  struct PostInitExpectedCall {
    // Matcher for the expected `profile` argument to `PostProfileInit()`
    const Matcher<Profile*> profile_matcher;

    // Expected value for the `is_initial_profile` argument to
    // `PostProfileInit()`
    const bool is_initial_profile;
  };

  // Call expectations for the `PostProfileInit()` method. The expectations
  // should themselves be listed in the expected call order.
  //
  // The first one is checked in `CreatedBrowserMainParts()` as part of startup,
  // and the remaining ones in the test body.
  const std::vector<PostInitExpectedCall> expected_post_profile_init_call_args;
};

const MultiProfileStartupTestParam kTestParams[] = {
    {.should_show_profile_picker = false,
     .expected_post_profile_init_call_args =
         {{HasBaseName(chrome::kInitialProfile), true},
          {HasBaseName(kOtherProfileDirPath), false}}},
    {.should_show_profile_picker = true,
     .expected_post_profile_init_call_args = {
         {HasBaseName(chrome::kInitialProfile), true},
         {HasBaseName(kOtherProfileDirPath), false}}}};

// Creates a new profile to be picked up on the actual test.
void SetUpSecondaryProfileForPreTest(
    const base::FilePath& profile_dir_basename) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile_path =
      profile_manager->user_data_dir().Append(profile_dir_basename);

  profiles::testing::CreateProfileSync(profile_manager, profile_path);

  // Mark newly created profile as active.
  ProfileAttributesEntry* entry =
      profile_manager->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_path);
  ASSERT_NE(entry, nullptr);
  entry->SetActiveTimeToNow();
}

void CreateBrowserForProfileDir(const base::FilePath& profile_dir_basename) {
  profiles::testing::SwitchToProfileSync(
      g_browser_process->profile_manager()->user_data_dir().Append(
          profile_dir_basename));
}

}  // namespace

class ChromeMultiProfileStartupBrowserTestBase
    : public InProcessBrowserTest,
      public testing::WithParamInterface<MultiProfileStartupTestParam> {
 public:
  ChromeMultiProfileStartupBrowserTestBase() {
    // Avoid providing a URL for the browser to open, allows the profile picker
    // to be displayed on startup when it is enabled.
    set_open_about_blank_on_browser_launch(false);
  }

  void CreatedBrowserMainParts(content::BrowserMainParts* parts) override {
    InProcessBrowserTest::CreatedBrowserMainParts(parts);

    // Skip expectations preparation for the PRE_ step.
    if (GetTestPreCount() != 0)
      return;

    auto mock_part = std::make_unique<MockMainExtraParts>();
    mock_part_ = mock_part.get();
    static_cast<ChromeBrowserMainParts*>(parts)->AddParts(std::move(mock_part));

    // At least one entry for the initial call is needed.
    ASSERT_FALSE(GetParam().expected_post_profile_init_call_args.empty());

    // The basic callbacks should be called only once.
    EXPECT_CALL(*mock_part_, PreProfileInit()).Times(1);
    EXPECT_CALL(*mock_part_, PreBrowserStart()).Times(1);
    EXPECT_CALL(*mock_part_, PostBrowserStart()).Times(1);
    EXPECT_CALL(*mock_part_, PreMainMessageLoopRun()).Times(1);

    {
      const auto& call_args = GetParam().expected_post_profile_init_call_args;
      InSequence s;
      for (const auto& expected_args : call_args) {
        EXPECT_CALL(*mock_part_,
                    PostProfileInit(expected_args.profile_matcher,
                                    expected_args.is_initial_profile));
      }
    }
  }

  raw_ptr<MockMainExtraParts, AcrossTasksDanglingUntriaged> mock_part_;
};

IN_PROC_BROWSER_TEST_P(ChromeMultiProfileStartupBrowserTestBase,
                       PRE_PostProfileInitInvocation) {
  SetUpSecondaryProfileForPreTest(
      base::FilePath::FromASCII(kOtherProfileDirPath));
  g_browser_process->local_state()->SetBoolean(
      prefs::kBrowserShowProfilePickerOnStartup,
      GetParam().should_show_profile_picker);

  // Need to close the browser window manually so that the real test does not
  // treat it as session restore.
  CloseAllBrowsers();
}

// Make sure that the second profile creation causes `PostProfileInit()` to be
// called a second time.
IN_PROC_BROWSER_TEST_P(ChromeMultiProfileStartupBrowserTestBase,
                       PostProfileInitInvocation) {
  EXPECT_EQ(2u, g_browser_process->profile_manager()->GetNumberOfProfiles());
  if (GetParam().should_show_profile_picker) {
    EXPECT_EQ(0u, chrome::GetTotalBrowserCount());
    EXPECT_TRUE(ProfilePicker::IsOpen());
  } else {
    EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
    EXPECT_NE(base::FilePath::FromASCII(kOtherProfileDirPath),
              browser()->profile()->GetPath().BaseName());
    EXPECT_FALSE(ProfilePicker::IsOpen());
  }

  // TODO(crbug.com/40817107): In some cases, profile creation is
  // triggered by restoring the previously opened profile, and the test
  // expectations in terms of `PostProfileInit()` calls can
  // be met without opening browsers. We still open them for consistency, at
  // least until we can make the test behaviour stricter.
  if (GetParam().should_show_profile_picker) {
    // No browser was previously open, as verified at the beginning of the test.
    // So we start by opening the browser for the default profile.
    CreateBrowserForProfileDir(
        base::FilePath::FromASCII(chrome::kInitialProfile));
  }
  CreateBrowserForProfileDir(base::FilePath::FromASCII(kOtherProfileDirPath));

  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
}

INSTANTIATE_TEST_SUITE_P(All,
                         ChromeMultiProfileStartupBrowserTestBase,
                         ValuesIn(kTestParams));
