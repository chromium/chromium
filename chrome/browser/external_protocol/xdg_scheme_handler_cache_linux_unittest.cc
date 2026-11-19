// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/external_protocol/xdg_scheme_handler_cache_linux.h"

#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/nix/xdg_util.h"
#include "base/run_loop.h"
#include "base/scoped_environment_variable_override.h"
#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

class XdgSchemeHandlerCacheScanTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    user_config_ = temp_dir_.GetPath().Append("config");
    system_config_ = temp_dir_.GetPath().Append("xdg");
    user_apps_ = temp_dir_.GetPath().Append("local/applications");
    system_apps_ = temp_dir_.GetPath().Append("usr/applications");
    for (const base::FilePath& dir :
         {user_config_, system_config_, user_apps_, system_apps_}) {
      ASSERT_TRUE(base::CreateDirectory(dir));
    }
  }

  void Write(const base::FilePath& path, std::string_view contents) {
    ASSERT_TRUE(base::CreateDirectory(path.DirName()));
    ASSERT_TRUE(base::WriteFile(path, contents));
  }

  void AddDesktopFile(const base::FilePath& dir, std::string_view name) {
    Write(dir.Append(name), "[Desktop Entry]\nType=Application\n");
  }

  base::flat_set<std::string> Scan(std::vector<std::string> desktops = {}) {
    return XdgSchemeHandlerCache::ScanForSchemeHandlers(
        {user_config_, system_config_}, {user_apps_, system_apps_}, desktops);
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath user_config_;
  base::FilePath system_config_;
  base::FilePath user_apps_;
  base::FilePath system_apps_;
};

TEST_F(XdgSchemeHandlerCacheScanTest, NoFiles) {
  EXPECT_THAT(Scan(), IsEmpty());
}

TEST_F(XdgSchemeHandlerCacheScanTest, DefaultApplication) {
  AddDesktopFile(user_apps_, "foo.desktop");
  Write(user_config_.Append("mimeapps.list"),
        "[Default Applications]\n"
        "x-scheme-handler/foo=foo.desktop\n"
        "text/html=foo.desktop\n");
  EXPECT_THAT(Scan(), ElementsAre("foo"));
}

TEST_F(XdgSchemeHandlerCacheScanTest, AddedAssociationAndMimeCache) {
  AddDesktopFile(system_apps_, "bar.desktop");
  AddDesktopFile(system_apps_, "baz.desktop");
  Write(system_config_.Append("mimeapps.list"),
        "# comment\n"
        "[Added Associations]\n"
        "X-Scheme-Handler/Bar = bar.desktop;\n");
  Write(system_apps_.Append("mimeinfo.cache"),
        "[MIME Cache]\n"
        "x-scheme-handler/baz=other.desktop;baz.desktop;\n");
  EXPECT_THAT(Scan(), ElementsAre("bar", "baz"));
}

TEST_F(XdgSchemeHandlerCacheScanTest, IgnoresMissingDesktopFiles) {
  Write(user_config_.Append("mimeapps.list"),
        "[Default Applications]\n"
        "x-scheme-handler/foo=uninstalled.desktop\n");
  EXPECT_THAT(Scan(), IsEmpty());
}

TEST_F(XdgSchemeHandlerCacheScanTest, IgnoresOtherGroupsAndBadIds) {
  AddDesktopFile(user_apps_, "foo.desktop");
  Write(user_config_.Append("mimeapps.list"),
        "[Desktop Entry]\n"
        "x-scheme-handler/foo=foo.desktop\n"
        "[Added Associations]\n"
        "x-scheme-handler/=foo.desktop\n"
        "x-scheme-handler/bar=../applications/foo.desktop\n"
        "x-scheme-handler/baz=/abs/foo.desktop\n");
  EXPECT_THAT(Scan(), IsEmpty());
}

TEST_F(XdgSchemeHandlerCacheScanTest, DesktopFileInSubdirectory) {
  AddDesktopFile(system_apps_.Append("kde4"), "foo.desktop");
  Write(system_apps_.Append("mimeinfo.cache"),
        "[MIME Cache]\n"
        "x-scheme-handler/foo=kde4-foo.desktop;\n");
  EXPECT_THAT(Scan(), ElementsAre("foo"));
}

// Desktop IDs must not escape the applications dir after '-' becomes '/'.
TEST_F(XdgSchemeHandlerCacheScanTest, IgnoresIdsLeavingApplicationsDir) {
  AddDesktopFile(system_apps_.DirName(), "foo.desktop");
  Write(system_apps_.Append("mimeinfo.cache"),
        "[MIME Cache]\n"
        "x-scheme-handler/foo=..-foo.desktop;\n"
        "x-scheme-handler/bar=-foo.desktop;\n"
        "x-scheme-handler/baz=a-..-..-foo.desktop;\n");
  EXPECT_THAT(Scan(), IsEmpty());
}

TEST_F(XdgSchemeHandlerCacheScanTest, IgnoresNonDesktopIdsAndDirectories) {
  ASSERT_TRUE(base::CreateDirectory(system_apps_.Append("kde4")));
  ASSERT_TRUE(base::CreateDirectory(system_apps_.Append("a")));
  ASSERT_TRUE(base::CreateDirectory(system_apps_.Append("dir.desktop")));
  Write(system_apps_.Append("plain"), "not a desktop file\n");
  ASSERT_TRUE(base::CreateSymbolicLink(system_apps_.Append("kde4"),
                                       system_apps_.Append("dirlink.desktop")));
  ASSERT_TRUE(base::CreateSymbolicLink(temp_dir_.GetPath().Append("missing"),
                                       system_apps_.Append("broken.desktop")));

  std::string contents =
      "[MIME Cache]\n"
      "x-scheme-handler/dot=.;\n"
      "x-scheme-handler/subdir=kde4;\n"
      "x-scheme-handler/subdot=a-.;\n"
      "x-scheme-handler/plain=plain;\n"
      "x-scheme-handler/dir=dir.desktop;\n"
      "x-scheme-handler/dirlink=dirlink.desktop;\n"
      "x-scheme-handler/broken=broken.desktop;\n";
  contents.append("x-scheme-handler/nul=plain");
  contents.push_back('\0');
  contents.append(".desktop;\n");
  Write(system_apps_.Append("mimeinfo.cache"), contents);
  EXPECT_THAT(Scan(), IsEmpty());
}

TEST_F(XdgSchemeHandlerCacheScanTest, FollowsSymlinkToDesktopFile) {
  base::FilePath target = temp_dir_.GetPath().Append("target.desktop");
  Write(target, "[Desktop Entry]\nType=Application\n");
  ASSERT_TRUE(
      base::CreateSymbolicLink(target, system_apps_.Append("linked.desktop")));
  Write(system_apps_.Append("mimeinfo.cache"),
        "[MIME Cache]\n"
        "x-scheme-handler/linked=linked.desktop;\n");
  EXPECT_THAT(Scan(), ElementsAre("linked"));
}

TEST_F(XdgSchemeHandlerCacheScanTest, IgnoresRelativeDirsAndBadDesktops) {
  AddDesktopFile(user_apps_, "foo.desktop");
  Write(user_config_.Append("mimeapps.list"),
        "[Default Applications]\n"
        "x-scheme-handler/foo=foo.desktop\n");
  Write(user_config_.Append("bar"),
        "[Default Applications]\n"
        "x-scheme-handler/bar=foo.desktop\n");
  EXPECT_THAT(XdgSchemeHandlerCache::ScanForSchemeHandlers(
                  {base::FilePath("config"), user_config_},
                  {base::FilePath("applications"), user_apps_},
                  {"../kde", "/kde", "", std::string("bar\0x", 5)}),
              ElementsAre("foo"));
}

TEST_F(XdgSchemeHandlerCacheScanTest, RemovedAssociationHidesLowerPrecedence) {
  AddDesktopFile(system_apps_, "foo.desktop");
  Write(system_apps_.Append("mimeinfo.cache"),
        "[MIME Cache]\n"
        "x-scheme-handler/foo=foo.desktop;\n");
  Write(user_config_.Append("mimeapps.list"),
        "[Removed Associations]\n"
        "x-scheme-handler/foo=foo.desktop;\n");
  EXPECT_THAT(Scan(), IsEmpty());
}

TEST_F(XdgSchemeHandlerCacheScanTest, RemovedAssociationDoesNotHideHigher) {
  AddDesktopFile(user_apps_, "foo.desktop");
  Write(user_config_.Append("mimeapps.list"),
        "[Added Associations]\n"
        "x-scheme-handler/foo=foo.desktop;\n");
  Write(system_config_.Append("mimeapps.list"),
        "[Removed Associations]\n"
        "x-scheme-handler/foo=foo.desktop;\n");
  EXPECT_THAT(Scan(), ElementsAre("foo"));
}

TEST_F(XdgSchemeHandlerCacheScanTest, DesktopSpecificList) {
  AddDesktopFile(user_apps_, "foo.desktop");
  Write(user_config_.Append("kde-mimeapps.list"),
        "[Default Applications]\n"
        "x-scheme-handler/foo=foo.desktop\n");
  EXPECT_THAT(Scan(), IsEmpty());
  EXPECT_THAT(Scan({"gnome"}), IsEmpty());
  EXPECT_THAT(Scan({"kde"}), ElementsAre("foo"));
}

class XdgSchemeHandlerCacheTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    config_home_ = temp_dir_.GetPath().Append("config");
    data_home_ = temp_dir_.GetPath().Append("data");
    apps_ = data_home_.Append("applications");
    ASSERT_TRUE(base::CreateDirectory(config_home_));
    ASSERT_TRUE(base::CreateDirectory(apps_));
    ASSERT_TRUE(base::WriteFile(apps_.Append("foo.desktop"),
                                "[Desktop Entry]\nType=Application\n"));
    env_overrides_.emplace_back(base::nix::kXdgConfigHomeEnvVar,
                                config_home_.value());
    env_overrides_.emplace_back(
        "XDG_CONFIG_DIRS",
        base::StrCat({ExtraEnvEntries(),
                      temp_dir_.GetPath().Append("no-config").value()}));
    env_overrides_.emplace_back("XDG_DATA_HOME", data_home_.value());
    env_overrides_.emplace_back(
        "XDG_DATA_DIRS",
        base::StrCat({ExtraEnvEntries(),
                      temp_dir_.GetPath().Append("no-data").value()}));
    env_overrides_.emplace_back(base::nix::kXdgCurrentDesktopEnvVar,
                                ExtraEnvEntries());
  }

  // Prepended to the colon-separated XDG variables.
  virtual std::string ExtraEnvEntries() { return ""; }

  // Waits for the next completed scan.
  void WaitForScan(XdgSchemeHandlerCache& cache) {
    base::RunLoop run_loop;
    cache.SetOnUpdatedCallbackForTesting(run_loop.QuitClosure());
    run_loop.Run();
    cache.SetOnUpdatedCallbackForTesting(base::RepeatingClosure());
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath config_home_;
  base::FilePath data_home_;
  base::FilePath apps_;
  std::vector<base::ScopedEnvironmentVariableOverride> env_overrides_;
};

TEST_F(XdgSchemeHandlerCacheTest, LoadsAndUpdates) {
  ASSERT_TRUE(base::WriteFile(config_home_.Append("mimeapps.list"),
                              "[Default Applications]\n"
                              "x-scheme-handler/foo=foo.desktop\n"));
  scoped_refptr<XdgSchemeHandlerCache> cache =
      XdgSchemeHandlerCache::GetOrCreate();
  EXPECT_EQ(cache, XdgSchemeHandlerCache::GetOrCreate());
  EXPECT_FALSE(cache->HasHandler("foo"));

  WaitForScan(*cache);
  EXPECT_TRUE(cache->HasHandler("foo"));
  EXPECT_TRUE(cache->HasHandler("FOO"));
  EXPECT_FALSE(cache->HasHandler("bar"));

  // A newly registered handler is picked up without restarting.
  ASSERT_TRUE(base::WriteFile(apps_.Append("mimeinfo.cache"),
                              "[MIME Cache]\n"
                              "x-scheme-handler/bar=foo.desktop;\n"));
  WaitForScan(*cache);
  EXPECT_TRUE(cache->HasHandler("foo"));
  EXPECT_TRUE(cache->HasHandler("bar"));
}

// Relative paths in the XDG variables are ignored, as the XDG Base Directory
// specification requires.
class XdgSchemeHandlerCacheRelativeDirsTest : public XdgSchemeHandlerCacheTest {
 protected:
  std::string ExtraEnvEntries() override {
    return "relative:../up:/nonexistent:";
  }
};

TEST_F(XdgSchemeHandlerCacheRelativeDirsTest, IgnoresRelativeDirs) {
  ASSERT_TRUE(base::WriteFile(config_home_.Append("mimeapps.list"),
                              "[Default Applications]\n"
                              "x-scheme-handler/foo=foo.desktop\n"));
  scoped_refptr<XdgSchemeHandlerCache> cache =
      XdgSchemeHandlerCache::GetOrCreate();
  WaitForScan(*cache);
  EXPECT_TRUE(cache->HasHandler("foo"));
}

}  // namespace
