// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/test/test_file_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/rect.h"

typedef InProcessBrowserTest PreservedWindowPlacement;
using ::testing::Optional;

namespace {

const gfx::Rect window_frame = gfx::Rect(20, 40, 600, 600);

}  // namespace

IN_PROC_BROWSER_TEST_F(PreservedWindowPlacement, PRE_Test) {
  browser()->window()->SetBounds(window_frame);
}

// Fails on Chrome OS as the browser thinks it is restarting after a crash, see
// http://crbug.com/168044
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_Test DISABLED_Test
#else
#define MAYBE_Test Test
#endif
IN_PROC_BROWSER_TEST_F(PreservedWindowPlacement, MAYBE_Test) {
  gfx::Rect bounds = browser()->window()->GetBounds();
  gfx::Rect expected_bounds(window_frame);
  ASSERT_EQ(expected_bounds.ToString(), bounds.ToString());
}

class PreferenceServiceTest : public InProcessBrowserTest {
 public:
  bool SetUpUserDataDirectory() override {
    base::FilePath user_data_directory;
    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_directory);

    original_pref_file_ = ui_test_utils::GetTestFilePath(
        base::FilePath()
            .AppendASCII("profiles")
            .AppendASCII("window_placement")
            .AppendASCII("Default"),
        base::FilePath().Append(chrome::kPreferencesFilename));
    tmp_pref_file_ =
        user_data_directory.AppendASCII(TestingProfile::kTestUserProfileDir);
    EXPECT_TRUE(base::CreateDirectory(tmp_pref_file_));
    tmp_pref_file_ = tmp_pref_file_.Append(chrome::kPreferencesFilename);

    EXPECT_TRUE(base::PathExists(original_pref_file_));
    EXPECT_TRUE(base::CopyFile(original_pref_file_, tmp_pref_file_));

#if BUILDFLAG(IS_WIN)
    // Make the copy writable.  On POSIX we assume the umask allows files
    // we create to be writable.
    EXPECT_TRUE(::SetFileAttributesW(tmp_pref_file_.value().c_str(),
                                     FILE_ATTRIBUTE_NORMAL));
#endif
    return true;
  }

 protected:
  base::FilePath original_pref_file_;
  base::FilePath tmp_pref_file_;
};

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
// This test verifies that the window position from the prefs file is restored
// when the app restores.  This doesn't really make sense on Linux, where
// the window manager might fight with you over positioning.  However, we
// might be able to make this work on buildbots.
// TODO(port): revisit this.

IN_PROC_BROWSER_TEST_F(PreferenceServiceTest, Test) {
  // The window should open with the new reference profile, with window
  // placement values stored in the user data directory.
  JSONFileValueDeserializer deserializer(original_pref_file_);
  std::unique_ptr<base::Value> root;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    root = deserializer.Deserialize(NULL, NULL);
  }

  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->is_dict());
  base::Value::Dict& root_dict = root->GetDict();

  // Retrieve the screen rect for the launched window
  gfx::Rect bounds = browser()->window()->GetRestoredBounds();

  // Retrieve the expected rect values from "Preferences"
  std::string kBrowserWindowPlacement(prefs::kBrowserWindowPlacement);
  EXPECT_THAT(
      root_dict.FindIntByDottedPath(kBrowserWindowPlacement + ".bottom"),
      Optional(bounds.y() + bounds.height()));

  EXPECT_THAT(root_dict.FindIntByDottedPath(kBrowserWindowPlacement + ".top"),
              Optional(bounds.y()));

  EXPECT_THAT(root_dict.FindIntByDottedPath(kBrowserWindowPlacement + ".left"),
              Optional(bounds.x()));

  EXPECT_THAT(root_dict.FindIntByDottedPath(kBrowserWindowPlacement + ".right"),
              Optional(bounds.x() + bounds.width()));

  // Find if launched window is maximized.
  bool is_window_maximized = browser()->window()->IsMaximized();
  EXPECT_THAT(
      root_dict.FindBoolByDottedPath(kBrowserWindowPlacement + ".maximized"),
      Optional(is_window_maximized));
}
#endif
