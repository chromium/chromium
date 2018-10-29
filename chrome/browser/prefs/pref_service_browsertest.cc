// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/gfx/geometry/rect.h"

typedef InProcessBrowserTest PreservedWindowPlacement;

IN_PROC_BROWSER_TEST_F(PreservedWindowPlacement, PRE_Test) {
  browser()->window()->SetBounds(gfx::Rect(20, 30, 600, 600));
}

// Fails on Chrome OS as the browser thinks it is restarting after a crash, see
// http://crbug.com/168044
#if defined(OS_CHROMEOS)
#define MAYBE_Test DISABLED_Test
#else
#define MAYBE_Test Test
#endif
IN_PROC_BROWSER_TEST_F(PreservedWindowPlacement, MAYBE_Test) {
  gfx::Rect bounds = browser()->window()->GetBounds();
  gfx::Rect expected_bounds(gfx::Rect(20, 30, 600, 600));
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

#if defined(OS_WIN)
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

#if defined(OS_WIN) || defined(OS_MACOSX)
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

  base::DictionaryValue* root_dict =
      static_cast<base::DictionaryValue*>(root.get());

  // Retrieve the screen rect for the launched window
  gfx::Rect bounds = browser()->window()->GetRestoredBounds();

  // Retrieve the expected rect values from "Preferences"
  int bottom = 0;
  std::string kBrowserWindowPlacement(prefs::kBrowserWindowPlacement);
  EXPECT_TRUE(root_dict->GetInteger(kBrowserWindowPlacement + ".bottom",
      &bottom));
  EXPECT_EQ(bottom, bounds.y() + bounds.height());

  int top = 0;
  EXPECT_TRUE(root_dict->GetInteger(kBrowserWindowPlacement + ".top",
      &top));
  EXPECT_EQ(top, bounds.y());

  int left = 0;
  EXPECT_TRUE(root_dict->GetInteger(kBrowserWindowPlacement + ".left",
      &left));
  EXPECT_EQ(left, bounds.x());

  int right = 0;
  EXPECT_TRUE(root_dict->GetInteger(kBrowserWindowPlacement + ".right",
      &right));
  EXPECT_EQ(right, bounds.x() + bounds.width());

  // Find if launched window is maximized.
  bool is_window_maximized = browser()->window()->IsMaximized();
  bool is_maximized = false;
  EXPECT_TRUE(root_dict->GetBoolean(kBrowserWindowPlacement + ".maximized",
      &is_maximized));
  EXPECT_EQ(is_maximized, is_window_maximized);
}
#endif
