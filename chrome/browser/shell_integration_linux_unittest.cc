// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/shell_integration_linux.h"

#include <stddef.h>

#include <algorithm>
#include <cstdlib>
#include <map>
#include <optional>
#include <string_view>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/chrome_constants.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/public/ozone_platform.h"
#include "url/gurl.h"

using ::testing::ElementsAre;

namespace shell_integration_linux {

namespace {

// Provides mock environment variables values based on a stored map.
class MockEnvironment : public base::Environment {
 public:
  MockEnvironment() {}

  MockEnvironment(const MockEnvironment&) = delete;
  MockEnvironment& operator=(const MockEnvironment&) = delete;

  void Set(std::string_view name, const std::string& value) {
    variables_[std::string(name)] = value;
  }

  bool GetVar(std::string_view variable_name, std::string* result) override {
    if (base::Contains(variables_, std::string(variable_name))) {
      *result = variables_[std::string(variable_name)];
      return true;
    }

    return false;
  }

  bool SetVar(std::string_view variable_name,
              const std::string& new_value) override {
    ADD_FAILURE();
    return false;
  }

  bool UnSetVar(std::string_view variable_name) override {
    ADD_FAILURE();
    return false;
  }

 private:
  std::map<std::string, std::string> variables_;
};

}  // namespace

TEST(ShellIntegrationTest, GetExistingShortcutContents) {
  const char kTemplateFilename[] = "shortcut-test.desktop";
  base::FilePath kTemplateFilepath(kTemplateFilename);
  const char kTestData1[] = "a magical testing string";
  const char kTestData2[] = "a different testing string";

  content::BrowserTaskEnvironment task_environment;

  // Test that it searches $XDG_DATA_HOME/applications.
  {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

    MockEnvironment env;
    env.Set("XDG_DATA_HOME", temp_dir.GetPath().value());
    // Create a file in a non-applications directory. This should be ignored.
    ASSERT_TRUE(base::WriteFile(temp_dir.GetPath().Append(kTemplateFilename),
                                kTestData2));
    ASSERT_TRUE(
        base::CreateDirectory(temp_dir.GetPath().Append("applications")));
    ASSERT_TRUE(base::WriteFile(
        temp_dir.GetPath().Append("applications").Append(kTemplateFilename),
        kTestData1));
    std::string contents;
    ASSERT_TRUE(
        GetExistingShortcutContents(&env, kTemplateFilepath, &contents));
    EXPECT_EQ(kTestData1, contents);
  }

  // Test that it falls back to $HOME/.local/share/applications.
  {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

    MockEnvironment env;
    base::ScopedPathOverride home_override(base::DIR_HOME, temp_dir.GetPath(),
                                           true /* absolute? */,
                                           false /* create? */);
    ASSERT_TRUE(base::CreateDirectory(
        temp_dir.GetPath().Append(".local/share/applications")));
    ASSERT_TRUE(base::WriteFile(temp_dir.GetPath()
                                    .Append(".local/share/applications")
                                    .Append(kTemplateFilename),
                                kTestData1));
    std::string contents;
    ASSERT_TRUE(
        GetExistingShortcutContents(&env, kTemplateFilepath, &contents));
    EXPECT_EQ(kTestData1, contents);
  }

  // Test that it searches $XDG_DATA_DIRS/applications.
  {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

    MockEnvironment env;
    env.Set("XDG_DATA_DIRS", temp_dir.GetPath().value());
    ASSERT_TRUE(
        base::CreateDirectory(temp_dir.GetPath().Append("applications")));
    ASSERT_TRUE(base::WriteFile(
        temp_dir.GetPath().Append("applications").Append(kTemplateFilename),
        kTestData2));
    std::string contents;
    ASSERT_TRUE(
        GetExistingShortcutContents(&env, kTemplateFilepath, &contents));
    EXPECT_EQ(kTestData2, contents);
  }

  // Test that it searches $X/applications for each X in $XDG_DATA_DIRS.
  {
    base::ScopedTempDir temp_dir1;
    ASSERT_TRUE(temp_dir1.CreateUniqueTempDir());
    base::ScopedTempDir temp_dir2;
    ASSERT_TRUE(temp_dir2.CreateUniqueTempDir());

    MockEnvironment env;
    env.Set("XDG_DATA_DIRS",
            temp_dir1.GetPath().value() + ":" + temp_dir2.GetPath().value());
    // Create a file in a non-applications directory. This should be ignored.
    ASSERT_TRUE(base::WriteFile(temp_dir1.GetPath().Append(kTemplateFilename),
                                kTestData1));
    // Only create a findable desktop file in the second path.
    ASSERT_TRUE(
        base::CreateDirectory(temp_dir2.GetPath().Append("applications")));
    ASSERT_TRUE(base::WriteFile(
        temp_dir2.GetPath().Append("applications").Append(kTemplateFilename),
        kTestData2));
    std::string contents;
    ASSERT_TRUE(
        GetExistingShortcutContents(&env, kTemplateFilepath, &contents));
    EXPECT_EQ(kTestData2, contents);
  }
}

TEST(ShellIntegrationTest, GetExistingProfileShortcutFilenames) {
  base::FilePath kProfilePath("a/b/c/Profile Name?");
  const char kApp1Filename[] = "chrome-extension1-Profile_Name_.desktop";
  const char kApp2Filename[] = "chrome-extension2-Profile_Name_.desktop";
  const char kUnrelatedAppFilename[] = "chrome-extension-Other_Profile.desktop";

  content::BrowserTaskEnvironment task_environment;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(base::WriteFile(temp_dir.GetPath().Append(kApp1Filename), ""));
  ASSERT_TRUE(base::WriteFile(temp_dir.GetPath().Append(kApp2Filename), ""));
  // This file should not be returned in the results.
  ASSERT_TRUE(
      base::WriteFile(temp_dir.GetPath().Append(kUnrelatedAppFilename), ""));
  std::vector<base::FilePath> paths =
      GetExistingProfileShortcutFilenames(kProfilePath, temp_dir.GetPath());
  // Path order is arbitrary. Sort the output for consistency.
  std::sort(paths.begin(), paths.end());
  EXPECT_THAT(paths,
              ElementsAre(base::FilePath(kApp1Filename),
                          base::FilePath(kApp2Filename)));
}

TEST(ShellIntegrationTest, GetUniqueWebShortcutFilenameFromUrl) {
  std::vector<std::pair<std::string, GURL>> test_cases = {
      {"http___foo_.desktop", GURL("http://foo")},
      {"http___foo_bar_.desktop", GURL("http://foo/bar/")},
      {"http___foo_bar_a=b&c=d.desktop", GURL("http://foo/bar?a=b&c=d")},

      // Now we're starting to be more evil...
      {"http___foo_.desktop", GURL("http://foo/bar/baz/../../../../../")},
      {"http___foo_.desktop", GURL("http://foo/bar/././../baz/././../")},
      {"http___.._.desktop", GURL("http://../../../../")},
  };
  for (const auto& [expected, gurl_input] : test_cases) {
    std::optional<base::SafeBaseName> file_base_name =
        GetUniqueWebShortcutFilename(gurl_input.spec());
    ASSERT_TRUE(file_base_name);
    EXPECT_EQ(
        base::StrCat({chrome::kBrowserProcessExecutableName, "-", expected}),
        file_base_name->path().value())
        << " while testing " << gurl_input.spec();
  }
}

TEST(ShellIntegrationTest, GetUniqueWebShortcutFilename) {
  std::vector<std::pair<std::string, std::string>> test_cases = {
      {"Test_test.desktop", "Test test"},
      {"What_about__newlines.desktop", "What\nabout\n\rnewlines"},
      {"______.desktop", "\\//\\//"},
  };
  for (const auto& [expected, input] : test_cases) {
    std::optional<base::SafeBaseName> file_base_name =
        GetUniqueWebShortcutFilename(input);
    ASSERT_TRUE(file_base_name);
    EXPECT_EQ(
        base::StrCat({chrome::kBrowserProcessExecutableName, "-", expected}),
        file_base_name->path().value())
        << " while testing " << input;
  }
}
TEST(ShellIntegrationTest, GetUniqueWebShortcutUnique) {
  const std::string kTestName = "Test test";

  base::ScopedPathOverride profile_override(base::DIR_USER_DESKTOP);
  base::FilePath desktop_dir =
      base::PathService::CheckedGet(base::DIR_USER_DESKTOP);

  // Create the first file option.
  std::optional<base::SafeBaseName> file_base_name =
      GetUniqueWebShortcutFilename(kTestName);
  ASSERT_TRUE(file_base_name);
  std::string expected_name = base::StrCat(
      {chrome::kBrowserProcessExecutableName, "-Test_test.desktop"});
  EXPECT_EQ(expected_name, file_base_name->path().value());
  ASSERT_TRUE(
      base::WriteFile(desktop_dir.Append(file_base_name->path()), "test data"));

  // The second call should guarantee uniqueness, and change the name without a
  // whitespace.
  std::optional<base::SafeBaseName> second_file_base_name =
      GetUniqueWebShortcutFilename(kTestName);
  ASSERT_TRUE(second_file_base_name);
  std::string expected_second_name = base::StrCat(
      {chrome::kBrowserProcessExecutableName, "-Test_test_1.desktop"});
  EXPECT_EQ(expected_second_name, second_file_base_name->path().value());
}

TEST(ShellIntegrationTest, GetDesktopFileContents) {
  const base::FilePath kChromeExePath("/opt/google/chrome/google-chrome");
  const struct {
    const char* const url;
    const char* const title;
    const char* const icon_name;
    const char* const categories;
    const char* const mime_type;
    bool nodisplay;
    const char* const expected_output;
  } test_cases[] = {
      // Real-world case.
      {"http://gmail.com", "GMail", "chrome-http__gmail.com", "", "", false,

       "#!/usr/bin/env xdg-open\n"
       "[Desktop Entry]\n"
       "Version=1.0\n"
       "Terminal=false\n"
       "Type=Application\n"
       "Name=GMail\n"
       "Exec=/opt/google/chrome/google-chrome --app=http://gmail.com/\n"
       "Icon=chrome-http__gmail.com\n"
       "StartupWMClass=gmail.com\n"},

      // Make sure that empty icons are replaced by the chrome icon.
      {"http://gmail.com", "GMail", "", "", "", false,

       "#!/usr/bin/env xdg-open\n"
       "[Desktop Entry]\n"
       "Version=1.0\n"
       "Terminal=false\n"
       "Type=Application\n"
       "Name=GMail\n"
       "Exec=/opt/google/chrome/google-chrome --app=http://gmail.com/\n"
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
       "Icon=google-chrome\n"
#else
       "Icon=chromium-browser\n"
#endif
       "StartupWMClass=gmail.com\n"},

      // Test adding categories and NoDisplay=true.
      {"http://gmail.com", "GMail", "chrome-http__gmail.com",
       "Graphics;Education;", "", true,

       "#!/usr/bin/env xdg-open\n"
       "[Desktop Entry]\n"
       "Version=1.0\n"
       "Terminal=false\n"
       "Type=Application\n"
       "Name=GMail\n"
       "Exec=/opt/google/chrome/google-chrome --app=http://gmail.com/\n"
       "Icon=chrome-http__gmail.com\n"
       "Categories=Graphics;Education;\n"
       "NoDisplay=true\n"
       "StartupWMClass=gmail.com\n"},

      // Now we're starting to be more evil...
      {"http://evil.com/evil --join-the-b0tnet", "Ownz0red\nExec=rm -rf /",
       "chrome-http__evil.com_evil", "", "", false,

       "#!/usr/bin/env xdg-open\n"
       "[Desktop Entry]\n"
       "Version=1.0\n"
       "Terminal=false\n"
       "Type=Application\n"
       "Name=http://evil.com/evil%20--join-the-b0tnet\n"
       "Exec=/opt/google/chrome/google-chrome "
       "--app=http://evil.com/evil%20--join-the-b0tnet\n"
       "Icon=chrome-http__evil.com_evil\n"
       "StartupWMClass=evil.com__evil%20--join-the-b0tnet\n"},
      {"http://evil.com/evil; rm -rf /; \"; rm -rf $HOME >ownz0red",
       "Innocent Title", "chrome-http__evil.com_evil", "", "", false,

       "#!/usr/bin/env xdg-open\n"
       "[Desktop Entry]\n"
       "Version=1.0\n"
       "Terminal=false\n"
       "Type=Application\n"
       "Name=Innocent Title\n"
       "Exec=/opt/google/chrome/google-chrome "
       "\"--app=http://evil.com/evil;%20rm%20-rf%20/;%20%22;%20rm%20"
       // Note: $ is escaped as \$ within an arg to Exec, and then
       // the \ is escaped as \\ as all strings in a Desktop file should
       // be; finally, \\ becomes \\\\ when represented in a C++ string!
       "-rf%20\\\\$HOME%20%3Eownz0red\"\n"
       "Icon=chrome-http__evil.com_evil\n"
       "StartupWMClass=evil.com__evil;%20rm%20-rf%20_;%20%22;%20"
       "rm%20-rf%20$HOME%20%3Eownz0red\n"},
      {"http://evil.com/evil | cat `echo ownz0red` >/dev/null",
       "Innocent Title", "chrome-http__evil.com_evil", "", "", false,

       "#!/usr/bin/env xdg-open\n"
       "[Desktop Entry]\n"
       "Version=1.0\n"
       "Terminal=false\n"
       "Type=Application\n"
       "Name=Innocent Title\n"
       "Exec=/opt/google/chrome/google-chrome "
       "--app=http://evil.com/evil%20%7C%20cat%20%60echo%20ownz0red"
       "%60%20%3E/dev/null\n"
       "Icon=chrome-http__evil.com_evil\n"
       "StartupWMClass=evil.com__evil%20%7C%20cat%20%60echo%20ownz0red"
       "%60%20%3E_dev_null\n"},
      // Test setting mime type
      {"https://paint.app", "Paint", "chrome-https__paint.app", "Image",
       "image/png;image/jpg", false,

       "#!/usr/bin/env xdg-open\n"
       "[Desktop Entry]\n"
       "Version=1.0\n"
       "Terminal=false\n"
       "Type=Application\n"
       "Name=Paint\n"
       "MimeType=image/png;image/jpg\n"
       "Exec=/opt/google/chrome/google-chrome --app=https://paint.app/ %U\n"
       "Icon=chrome-https__paint.app\n"
       "Categories=Image\n"
       "StartupWMClass=paint.app\n"},

      // Test evil mime type.
      {"https://paint.app", "Evil Paint", "chrome-https__paint.app", "Image",
       "image/png\nExec=rm -rf /", false,

       "#!/usr/bin/env xdg-open\n"
       "[Desktop Entry]\n"
       "Version=1.0\n"
       "Terminal=false\n"
       "Type=Application\n"
       "Name=Evil Paint\n"
       "Exec=/opt/google/chrome/google-chrome --app=https://paint.app/\n"
       "Icon=chrome-https__paint.app\n"
       "Categories=Image\n"
       "StartupWMClass=paint.app\n"}};

  for (size_t i = 0; i < std::size(test_cases); i++) {
    SCOPED_TRACE(i);
    EXPECT_EQ(
        test_cases[i].expected_output,
        GetDesktopFileContents(
            kChromeExePath,
            web_app::GenerateApplicationNameFromURL(GURL(test_cases[i].url)),
            GURL(test_cases[i].url), std::string(),
            base::ASCIIToUTF16(test_cases[i].title), test_cases[i].icon_name,
            base::FilePath(), test_cases[i].categories, test_cases[i].mime_type,
            test_cases[i].nodisplay, "", {}));
  }
}

TEST(ShellIntegrationTest, GetDesktopFileContentsForApps) {
  const base::FilePath kChromeExePath("/opt/google/chrome/google-chrome");
  const struct {
    const char* const url;
    const char* const title;
    const char* const icon_name;
    bool nodisplay;
    std::set<web_app::DesktopActionInfo> action_info;
    const char* const expected_output;
  } test_cases[] = {
      // Test Shortcut Menu actions.
      {"https://example.app",
       "Lawful example",
       "IconName",
       false,
       {
           web_app::DesktopActionInfo("action1", "Action 1",
                                      GURL("https://example.com/action1")),
           web_app::DesktopActionInfo("action2", "Action 2",
                                      GURL("https://example.com/action2")),
           web_app::DesktopActionInfo("action3", "Action 3",
                                      GURL("https://example.com/action3")),
           web_app::DesktopActionInfo("action4", "Action 4",
                                      GURL("https://example.com/action4")),
           web_app::DesktopActionInfo("action5", "Action 5",
                                      GURL("https://example.com/action%205")),
       },

       "#!/usr/bin/env xdg-open\n"
       "[Desktop Entry]\n"
       "Version=1.0\n"
       "Terminal=false\n"
       "Type=Application\n"
       "Name=Lawful example\n"
       "Exec=/opt/google/chrome/google-chrome --app-id=TestAppId\n"
       "Icon=IconName\n"
       "StartupWMClass=example.app\n"
       "Actions=action1;action2;action3;action4;action5\n\n"
       "[Desktop Action action1]\n"
       "Name=Action 1\n"
       "Exec=/opt/google/chrome/google-chrome --app-id=TestAppId "
       "--app-launch-url-for-shortcuts-menu-item=https://example.com/"
       "action1\n\n"
       "[Desktop Action action2]\n"
       "Name=Action 2\n"
       "Exec=/opt/google/chrome/google-chrome --app-id=TestAppId "
       "--app-launch-url-for-shortcuts-menu-item=https://example.com/"
       "action2\n\n"
       "[Desktop Action action3]\n"
       "Name=Action 3\n"
       "Exec=/opt/google/chrome/google-chrome --app-id=TestAppId "
       "--app-launch-url-for-shortcuts-menu-item=https://example.com/"
       "action3\n\n"
       "[Desktop Action action4]\n"
       "Name=Action 4\n"
       "Exec=/opt/google/chrome/google-chrome --app-id=TestAppId "
       "--app-launch-url-for-shortcuts-menu-item=https://example.com/"
       "action4\n\n"
       "[Desktop Action action5]\n"
       "Name=Action 5\n"
       "Exec=/opt/google/chrome/google-chrome --app-id=TestAppId "
       "--app-launch-url-for-shortcuts-menu-item=https://example.com/"
       "action%%205\n"},
  };

  for (size_t i = 0; i < std::size(test_cases); i++) {
    SCOPED_TRACE(i);
    EXPECT_EQ(
        test_cases[i].expected_output,
        GetDesktopFileContents(
            kChromeExePath,
            web_app::GenerateApplicationNameFromURL(GURL(test_cases[i].url)),
            GURL(test_cases[i].url), "TestAppId",
            base::ASCIIToUTF16(test_cases[i].title), test_cases[i].icon_name,
            base::FilePath(), "", "", test_cases[i].nodisplay, "",
            test_cases[i].action_info));
  }
}

TEST(ShellIntegrationTest, GetDirectoryFileContents) {
  const struct {
    const char* const title;
    const char* const icon_name;
    const char* const expected_output;
  } test_cases[] = {
      // Real-world case.
      {"Chrome Apps", "chrome-apps",

       "[Desktop Entry]\n"
       "Version=1.0\n"
       "Type=Directory\n"
       "Name=Chrome Apps\n"
       "Icon=chrome-apps\n"},

      // Make sure that empty icons are replaced by the chrome icon.
      {"Chrome Apps", "",

       "[Desktop Entry]\n"
       "Version=1.0\n"
       "Type=Directory\n"
       "Name=Chrome Apps\n"
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
       "Icon=google-chrome\n"
#else
       "Icon=chromium-browser\n"
#endif
      },
  };

  for (size_t i = 0; i < std::size(test_cases); i++) {
    SCOPED_TRACE(i);
    EXPECT_EQ(test_cases[i].expected_output,
              GetDirectoryFileContents(base::ASCIIToUTF16(test_cases[i].title),
                                       test_cases[i].icon_name));
  }
}

TEST(ShellIntegrationTest, GetMimeTypesRegistrationFilename) {
  const struct {
    const char* const profile_path;
    const char* const app_id;
    const char* const expected_filename;
  } test_cases[] = {
      {"Default", "app-id", "-app-id-Default.xml"},
      {"Default Profile", "app-id", "-app-id-Default_Profile.xml"},
      {"foo/Default", "app-id", "-app-id-Default.xml"},
      {"Default*Profile", "app-id", "-app-id-Default_Profile.xml"}};
  std::string browser_name(chrome::kBrowserProcessExecutableName);

  for (const auto& test_case : test_cases) {
    const base::FilePath filename =
        GetMimeTypesRegistrationFilename(base::FilePath(test_case.profile_path),
                                         webapps::AppId(test_case.app_id));
    EXPECT_EQ(browser_name + test_case.expected_filename, filename.value());
  }
}

TEST(ShellIntegrationTest, GetMimeTypesRegistrationFileContents) {
  apps::FileHandlers file_handlers;
  {
    apps::FileHandler file_handler;
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foo";
      accept_entry.file_extensions.insert(".foo");
      file_handler.accept.push_back(accept_entry);
    }
    file_handler.display_name = u"FoO";
    file_handlers.push_back(file_handler);
  }
  {
    apps::FileHandler file_handler;
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foobar";
      accept_entry.file_extensions.insert(".foobar");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
  }
  {
    apps::FileHandler file_handler;
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/bar";
      // A name that has a reserved XML character.
      file_handler.display_name = u"ba<r";
      accept_entry.file_extensions.insert(".bar");
      accept_entry.file_extensions.insert(".baz");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
  }

  const std::string file_contents =
      GetMimeTypesRegistrationFileContents(file_handlers);
  const std::string expected_file_contents =
      "<?xml version=\"1.0\"?>\n"
      "<mime-info "
      "xmlns=\"http://www.freedesktop.org/standards/shared-mime-info\">\n"
      " <mime-type type=\"application/foo\">\n"
      "  <comment>FoO</comment>\n"
      "  <glob pattern=\"*.foo\"/>\n"
      " </mime-type>\n"
      " <mime-type type=\"application/foobar\">\n"
      "  <glob pattern=\"*.foobar\"/>\n"
      " </mime-type>\n"
      " <mime-type type=\"application/bar\">\n"
      "  <comment>ba&lt;r</comment>\n"
      "  <glob pattern=\"*.bar\"/>\n"
      "  <glob pattern=\"*.baz\"/>\n"
      " </mime-type>\n"
      "</mime-info>\n";

  EXPECT_EQ(file_contents, expected_file_contents);
}

// The WM class name may be either capitalised or not, depending on the
// platform.
void CheckProgramClassClass(const std::string& class_name) {
  if (ui::OzonePlatform::GetPlatformNameForTest() == "x11") {
    EXPECT_EQ("Foo", class_name);
  } else {
    EXPECT_EQ("foo", class_name);
  }
}

TEST(ShellIntegrationTest, WmClass) {
  base::CommandLine command_line((base::FilePath()));
  EXPECT_EQ("foo", internal::GetProgramClassName(command_line, "foo.desktop"));
  CheckProgramClassClass(
      internal::GetProgramClassClass(command_line, "foo.desktop"));

  command_line.AppendSwitchASCII("class", "baR");
  EXPECT_EQ("foo", internal::GetProgramClassName(command_line, "foo.desktop"));
  EXPECT_EQ("baR", internal::GetProgramClassClass(command_line, "foo.desktop"));

  command_line = base::CommandLine(base::FilePath());
  command_line.AppendSwitchASCII("user-data-dir", "/tmp/baz");
  EXPECT_EQ("foo (/tmp/baz)",
            internal::GetProgramClassName(command_line, "foo.desktop"));
  CheckProgramClassClass(
      internal::GetProgramClassClass(command_line, "foo.desktop"));
}

TEST(ShellIntegrationTest, GetDesktopEntryStringValueFromFromDesktopFile) {
  const char* const kDesktopFileContents =
      "#!/usr/bin/env xdg-open\n"
      "[Desktop Entry]\n"
      "Version=1.0\n"
      "Terminal=false\n"
      "Type=Application\n"
      "Name=Lawful example\n"
      "Exec=/opt/google/chrome/google-chrome --app-id=TestAppId\n"
      "Icon=IconName\n"
      "StartupWMClass=example.app\n"
      "Actions=action1\n\n"
      "[Desktop Action action1]\n"
      "Name=Action 1\n"
      "Exec=/opt/google/chrome/google-chrome --app-id=TestAppId --Test"
      "Action1=Value";

  // Verify basic strings return the right value.
  EXPECT_EQ("Lawful example",
            shell_integration_linux::internal::
                GetDesktopEntryStringValueFromFromDesktopFileForTest(
                    "Name", kDesktopFileContents));
  EXPECT_EQ("example.app",
            shell_integration_linux::internal::
                GetDesktopEntryStringValueFromFromDesktopFileForTest(
                    "StartupWMClass", kDesktopFileContents));
  // Verify that booleans are returned correctly.
  EXPECT_EQ("false", shell_integration_linux::internal::
                         GetDesktopEntryStringValueFromFromDesktopFileForTest(
                             "Terminal", kDesktopFileContents));
  // Verify that numbers are returned correctly.
  EXPECT_EQ("1.0", shell_integration_linux::internal::
                       GetDesktopEntryStringValueFromFromDesktopFileForTest(
                           "Version", kDesktopFileContents));
  // Verify that a non-existent key returns an empty string.
  EXPECT_EQ("", shell_integration_linux::internal::
                    GetDesktopEntryStringValueFromFromDesktopFileForTest(
                        "DoesNotExistKey", kDesktopFileContents));
  // Verify that a non-existent key in [Desktop Entry] section returns an empty
  // string.
  EXPECT_EQ("", shell_integration_linux::internal::
                    GetDesktopEntryStringValueFromFromDesktopFileForTest(
                        "Action1", kDesktopFileContents));
}

}  // namespace shell_integration_linux
