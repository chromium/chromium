// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_enumerator.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/grit/branded_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

class ResourcesTest : public ::testing::Test {
 protected:
  ResourcesTest() {
    ui::RegisterPathProvider();
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        "en-US", nullptr, ui::ResourceBundle::LOAD_COMMON_RESOURCES);
  }

  ~ResourcesTest() override { ui::ResourceBundle::CleanupSharedInstance(); }
};

// Trailing whitespace has been the cause of a bug in the past. Make sure this
// never happens again on messages where this matter.
TEST_F(ResourcesTest, CriticalMessagesContainNoExtraWhitespaces) {
  // Array of messages that should never contain extra whitespaces.
  const int messages_to_check[] = {IDS_APP_SHORTCUTS_SUBDIR_NAME};

  base::FilePath locales_dir;
  if (!base::PathService::Get(ui::DIR_LOCALES, &locales_dir)) {
#if BUILDFLAG(IS_MAC)
    // On macOS, the test does not have the locale resources.
    GTEST_SKIP() << "Locale pak files absent.";
#else
    FAIL() << "Locale pak files directory missing.";
#endif
  }

  // Enumerate through the existing locale (.pak) files.
  base::FileEnumerator file_enumerator(locales_dir, false,
                                       base::FileEnumerator::FILES,
                                       FILE_PATH_LITERAL("*.pak"));
  for (base::FilePath locale_file_path = file_enumerator.Next();
       !locale_file_path.empty(); locale_file_path = file_enumerator.Next()) {
    base::FilePath::StringType file_name = locale_file_path.BaseName().value();

    // Gender-specific .pak files are deduped against the base .pak file, so
    // these are not expected to contain the |messages_to_check|.
    if (base::EndsWith(file_name, FILE_PATH_LITERAL("_FEMININE.pak")) ||
        base::EndsWith(file_name, FILE_PATH_LITERAL("_MASCULINE.pak")) ||
        base::EndsWith(file_name, FILE_PATH_LITERAL("_NEUTER.pak"))) {
      continue;
    }

    // Load the current locale file.
    ui::ResourceBundle::GetSharedInstance().OverrideLocalePakForTest(
        locale_file_path);
    ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources("");

    for (int message : messages_to_check) {
      std::u16string message_str = l10n_util::GetStringUTF16(message);
      EXPECT_EQ(message_str, base::TrimWhitespace(message_str, base::TRIM_ALL));
    }
  }
}
