// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"

#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_path_override.h"
#include "base/util/values/values_util.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/file_system_access/file_system_access_permission_request_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "ui/webui/webui_allowlist.h"
#include "url/gurl.h"
#include "url/origin.h"

using content::BrowserContext;
using content::WebContents;
using content::WebContentsTester;
using permissions::PermissionAction;
using HandleType = ChromeFileSystemAccessPermissionContext::HandleType;
using PathType = ChromeFileSystemAccessPermissionContext::PathType;
using UserAction = ChromeFileSystemAccessPermissionContext::UserAction;
using PermissionStatus =
    content::FileSystemAccessPermissionGrant::PermissionStatus;
using PermissionRequestOutcome =
    content::FileSystemAccessPermissionGrant::PermissionRequestOutcome;
using SensitiveDirectoryResult =
    ChromeFileSystemAccessPermissionContext::SensitiveDirectoryResult;
using UserActivationState =
    content::FileSystemAccessPermissionGrant::UserActivationState;

class TestFileSystemAccessPermissionContext
    : public ChromeFileSystemAccessPermissionContext {
 public:
  explicit TestFileSystemAccessPermissionContext(
      content::BrowserContext* context)
      : ChromeFileSystemAccessPermissionContext(context) {}
  ~TestFileSystemAccessPermissionContext() override = default;

 private:
  base::WeakPtrFactory<TestFileSystemAccessPermissionContext> weak_factory_{
      this};
};

class ChromeFileSystemAccessPermissionContextTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    FileSystemAccessPermissionRequestManager::CreateForWebContents(
        web_contents_.get());
    content::WebContentsTester::For(web_contents_.get())
        ->NavigateAndCommit(kTestOrigin.GetURL());

    FileSystemAccessPermissionRequestManager::FromWebContents(
        web_contents_.get())
        ->set_auto_response_for_test(PermissionAction::DISMISSED);
    permission_context_ =
        std::make_unique<TestFileSystemAccessPermissionContext>(
            browser_context());
  }

  void TearDown() override {
    ASSERT_TRUE(temp_dir_.Delete());
    web_contents_.reset();
  }

  SensitiveDirectoryResult ConfirmSensitiveDirectoryAccessSync(
      ChromeFileSystemAccessPermissionContext* context,
      PathType path_type,
      const base::FilePath& path,
      HandleType handle_type) {
    base::RunLoop loop;
    SensitiveDirectoryResult out_result;
    permission_context_->ConfirmSensitiveDirectoryAccess(
        kTestOrigin, path_type, path, handle_type,
        content::GlobalFrameRoutingId(),
        base::BindLambdaForTesting([&](SensitiveDirectoryResult result) {
          out_result = result;
          loop.Quit();
        }));
    loop.Run();
    return out_result;
  }

  void SetDefaultContentSettingValue(ContentSettingsType type,
                                     ContentSetting value) {
    HostContentSettingsMap* content_settings =
        HostContentSettingsMapFactory::GetForProfile(&profile_);
    content_settings->SetDefaultContentSetting(type, value);
  }

  void SetContentSettingValueForOrigin(url::Origin origin,
                                       ContentSettingsType type,
                                       ContentSetting value) {
    HostContentSettingsMap* content_settings =
        HostContentSettingsMapFactory::GetForProfile(&profile_);
    content_settings->SetContentSettingDefaultScope(
        origin.GetURL(), origin.GetURL(), type, value);
  }

  ChromeFileSystemAccessPermissionContext* permission_context() {
    return permission_context_.get();
  }
  BrowserContext* browser_context() { return &profile_; }
  TestingProfile* profile() { return &profile_; }
  WebContents* web_contents() { return web_contents_.get(); }

  int process_id() {
    return web_contents()->GetMainFrame()->GetProcess()->GetID();
  }

  content::GlobalFrameRoutingId frame_id() {
    return content::GlobalFrameRoutingId(
        process_id(), web_contents()->GetMainFrame()->GetRoutingID());
  }

 protected:
  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kTestOrigin2 =
      url::Origin::Create(GURL("https://test.com"));
  const std::string kTestStartingDirectoryId = "test_id";
  const base::FilePath kTestPath =
      base::FilePath(FILE_PATH_LITERAL("/foo/bar"));
  const url::Origin kChromeOrigin = url::Origin::Create(GURL("chrome://test"));

  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ChromeFileSystemAccessPermissionContext> permission_context_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  TestingProfile profile_;
  std::unique_ptr<WebContents> web_contents_;
};

#if !defined(OS_ANDROID)

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveDirectoryAccess_NoSpecialPath) {
  const base::FilePath kTestPath =
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
      base::FilePath(FILE_PATH_LITERAL("c:\\foo\\bar"));
#else
      base::FilePath(FILE_PATH_LITERAL("/foo/bar"));
#endif

  // Path outside any special directories should be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAllowed,
            ConfirmSensitiveDirectoryAccessSync(permission_context(),
                                                PathType::kLocal, kTestPath,
                                                HandleType::kFile));
  EXPECT_EQ(SensitiveDirectoryResult::kAllowed,
            ConfirmSensitiveDirectoryAccessSync(permission_context(),
                                                PathType::kLocal, kTestPath,
                                                HandleType::kDirectory));

  // External (relative) paths should also be allowed.
  EXPECT_EQ(
      SensitiveDirectoryResult::kAllowed,
      ConfirmSensitiveDirectoryAccessSync(
          permission_context(), PathType::kExternal,
          base::FilePath(FILE_PATH_LITERAL("foo/bar")), HandleType::kFile));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveDirectoryAccess_DontBlockAllChildren) {
  base::FilePath home_dir = temp_dir_.GetPath().AppendASCII("home");
  base::ScopedPathOverride home_override(base::DIR_HOME, home_dir, true, true);

  // Home directory itself should not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(permission_context(),
                                                PathType::kLocal, home_dir,
                                                HandleType::kDirectory));
  // Parent of home directory should also not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(
                permission_context(), PathType::kLocal, temp_dir_.GetPath(),
                HandleType::kDirectory));
  // Paths inside home directory should be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAllowed,
            ConfirmSensitiveDirectoryAccessSync(
                permission_context(), PathType::kLocal,
                home_dir.AppendASCII("foo"), HandleType::kFile));
  EXPECT_EQ(SensitiveDirectoryResult::kAllowed,
            ConfirmSensitiveDirectoryAccessSync(
                permission_context(), PathType::kLocal,
                home_dir.AppendASCII("foo"), HandleType::kDirectory));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveDirectoryAccess_BlockAllChildren) {
  base::FilePath app_dir = temp_dir_.GetPath().AppendASCII("app");
  base::ScopedPathOverride app_override(chrome::DIR_APP, app_dir, true, true);

  // App directory itself should not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(permission_context(),
                                                PathType::kLocal, app_dir,
                                                HandleType::kDirectory));
  // Parent of App directory should also not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(
                permission_context(), PathType::kLocal, temp_dir_.GetPath(),
                HandleType::kDirectory));
  // Paths inside App directory should also not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(
                permission_context(), PathType::kLocal,
                app_dir.AppendASCII("foo"), HandleType::kFile));
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(
                permission_context(), PathType::kLocal,
                app_dir.AppendASCII("foo"), HandleType::kDirectory));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveDirectoryAccess_BlockChildrenNested) {
  base::FilePath user_data_dir = temp_dir_.GetPath().AppendASCII("user");
  base::ScopedPathOverride user_data_override(chrome::DIR_USER_DATA,
                                              user_data_dir, true, true);
  base::FilePath download_dir = user_data_dir.AppendASCII("downloads");
  base::ScopedPathOverride download_override(chrome::DIR_DEFAULT_DOWNLOADS,
                                             download_dir, true, true);

  // User Data directory itself should not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(permission_context(),
                                                PathType::kLocal, user_data_dir,
                                                HandleType::kDirectory));
  // Parent of User Data directory should also not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(
                permission_context(), PathType::kLocal, temp_dir_.GetPath(),
                HandleType::kDirectory));
  // The nested Download directory itself should not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(permission_context(),
                                                PathType::kLocal, download_dir,
                                                HandleType::kDirectory));
  // Paths inside the nested Download directory should be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAllowed,
            ConfirmSensitiveDirectoryAccessSync(
                permission_context(), PathType::kLocal,
                download_dir.AppendASCII("foo"), HandleType::kFile));
  EXPECT_EQ(SensitiveDirectoryResult::kAllowed,
            ConfirmSensitiveDirectoryAccessSync(
                permission_context(), PathType::kLocal,
                download_dir.AppendASCII("foo"), HandleType::kDirectory));

#if defined(OS_WIN)
  // DIR_IE_INTERNET_CACHE is an example of a directory where nested directories
  // are blocked, but nested files should be allowed.
  base::FilePath internet_cache = user_data_dir.AppendASCII("INetCache");
  base::ScopedPathOverride internet_cache_override(base::DIR_IE_INTERNET_CACHE,
                                                   internet_cache, true, true);

  // The nested INetCache directory itself should not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(
                permission_context(), PathType::kLocal, internet_cache,
                HandleType::kDirectory));
  // Files inside the nested INetCache directory should be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAllowed,
            ConfirmSensitiveDirectoryAccessSync(
                permission_context(), PathType::kLocal,
                internet_cache.AppendASCII("foo"), HandleType::kFile));
  // But directories should be blocked.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(
                permission_context(), PathType::kLocal,
                internet_cache.AppendASCII("foo"), HandleType::kDirectory));
#endif
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveDirectoryAccess_RelativePathBlock) {
  base::FilePath home_dir = temp_dir_.GetPath().AppendASCII("home");
  base::ScopedPathOverride home_override(base::DIR_HOME, home_dir, true, true);

  // ~/.ssh should be blocked
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(
                permission_context(), PathType::kLocal,
                home_dir.AppendASCII(".ssh"), HandleType::kDirectory));
  // And anything inside ~/.ssh should also be blocked
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(
                permission_context(), PathType::kLocal,
                home_dir.AppendASCII(".ssh/id_rsa"), HandleType::kFile));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveDirectoryAccess_ExplicitPathBlock) {
// Linux is the only OS where we have some blocked directories with explicit
// paths (as opposed to PathService provided paths).
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // /dev should be blocked.
  EXPECT_EQ(
      SensitiveDirectoryResult::kAbort,
      ConfirmSensitiveDirectoryAccessSync(
          permission_context(), PathType::kLocal,
          base::FilePath(FILE_PATH_LITERAL("/dev")), HandleType::kDirectory));
  // As well as children of /dev.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(
                permission_context(), PathType::kLocal,
                base::FilePath(FILE_PATH_LITERAL("/dev/foo")),
                HandleType::kDirectory));
  EXPECT_EQ(
      SensitiveDirectoryResult::kAbort,
      ConfirmSensitiveDirectoryAccessSync(
          permission_context(), PathType::kLocal,
          base::FilePath(FILE_PATH_LITERAL("/dev/foo")), HandleType::kFile));
#endif
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       CanObtainWritePermission_ContentSettingAsk) {
  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_ASK);
  EXPECT_TRUE(permission_context()->CanObtainWritePermission(kTestOrigin));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       CanObtainWritePermission_ContentSettingsBlock) {
  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(permission_context()->CanObtainWritePermission(kTestOrigin));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       CanObtainWritePermission_ContentSettingAllow) {
  // Note, chrome:// scheme is whitelisted. But we can't set default content
  // setting here because ALLOW is not an acceptable option.
  EXPECT_TRUE(permission_context()->CanObtainWritePermission(kChromeOrigin));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, PolicyReadGuardPermission) {
  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedDefaultFileSystemReadGuardSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));

  EXPECT_FALSE(permission_context()->CanObtainReadPermission(kTestOrigin));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       PolicyWriteGuardPermission) {
  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedDefaultFileSystemWriteGuardSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));

  EXPECT_FALSE(permission_context()->CanObtainWritePermission(kTestOrigin));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, PolicyReadAskForUrls) {
  // Set the default to "block" so that the policy being tested overrides it.
  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedDefaultFileSystemReadGuardSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  prefs->SetManagedPref(prefs::kManagedFileSystemReadAskForUrls,
                        base::JSONReader::ReadDeprecated(
                            "[\"" + kTestOrigin.Serialize() + "\"]"));

  EXPECT_TRUE(permission_context()->CanObtainReadPermission(kTestOrigin));
  EXPECT_FALSE(permission_context()->CanObtainReadPermission(kTestOrigin2));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, PolicyReadBlockedForUrls) {
  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedFileSystemReadBlockedForUrls,
                        base::JSONReader::ReadDeprecated(
                            "[\"" + kTestOrigin.Serialize() + "\"]"));

  EXPECT_FALSE(permission_context()->CanObtainReadPermission(kTestOrigin));
  EXPECT_TRUE(permission_context()->CanObtainReadPermission(kTestOrigin2));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, PolicyWriteAskForUrls) {
  // Set the default to "block" so that the policy being tested overrides it.
  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedDefaultFileSystemWriteGuardSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  prefs->SetManagedPref(prefs::kManagedFileSystemWriteAskForUrls,
                        base::JSONReader::ReadDeprecated(
                            "[\"" + kTestOrigin.Serialize() + "\"]"));

  EXPECT_TRUE(permission_context()->CanObtainWritePermission(kTestOrigin));
  EXPECT_FALSE(permission_context()->CanObtainWritePermission(kTestOrigin2));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, PolicyWriteBlockedForUrls) {
  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedFileSystemWriteBlockedForUrls,
                        base::JSONReader::ReadDeprecated(
                            "[\"" + kTestOrigin.Serialize() + "\"]"));

  EXPECT_FALSE(permission_context()->CanObtainWritePermission(kTestOrigin));
  EXPECT_TRUE(permission_context()->CanObtainWritePermission(kTestOrigin2));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, GetLastPickedDirectory) {
  auto file_info = permission_context()->GetLastPickedDirectory(
      kTestOrigin, kTestStartingDirectoryId);
  EXPECT_EQ(file_info.path, base::FilePath());
  EXPECT_EQ(file_info.type, PathType::kLocal);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, SetLastPickedDirectory) {
  EXPECT_EQ(permission_context()
                ->GetLastPickedDirectory(kTestOrigin, kTestStartingDirectoryId)
                .path,
            base::FilePath());

  auto type = PathType::kLocal;
  permission_context()->SetLastPickedDirectory(
      kTestOrigin, kTestStartingDirectoryId, kTestPath, type);
  auto path_info = permission_context()->GetLastPickedDirectory(
      kTestOrigin, kTestStartingDirectoryId);
  EXPECT_EQ(path_info.path, kTestPath);
  EXPECT_EQ(path_info.type, type);

  auto new_path = path_info.path.AppendASCII("baz");
  auto new_type = PathType::kExternal;
  permission_context()->SetLastPickedDirectory(
      kTestOrigin, kTestStartingDirectoryId, new_path, new_type);
  auto new_path_info = permission_context()->GetLastPickedDirectory(
      kTestOrigin, kTestStartingDirectoryId);
  EXPECT_EQ(new_path_info.path, new_path);
  EXPECT_EQ(new_path_info.type, new_type);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       SetLastPickedDirectory_DefaultId) {
  EXPECT_EQ(permission_context()
                ->GetLastPickedDirectory(kTestOrigin, kTestStartingDirectoryId)
                .path,
            base::FilePath());

  // SetLastPickedDirectory with `kTestStartingDirectoryId`.
  auto type = PathType::kLocal;
  permission_context()->SetLastPickedDirectory(
      kTestOrigin, kTestStartingDirectoryId, kTestPath, type);
  auto path_info = permission_context()->GetLastPickedDirectory(
      kTestOrigin, kTestStartingDirectoryId);
  EXPECT_EQ(path_info.path, kTestPath);
  EXPECT_EQ(path_info.type, type);

  // SetLastPickedDirectory with an empty (default) ID.
  auto new_id = std::string();
  auto new_path = path_info.path.AppendASCII("baz");
  auto new_type = PathType::kExternal;
  permission_context()->SetLastPickedDirectory(kTestOrigin, new_id, new_path,
                                               new_type);
  auto new_path_info =
      permission_context()->GetLastPickedDirectory(kTestOrigin, new_id);
  EXPECT_EQ(new_path_info.path, new_path);
  EXPECT_EQ(new_path_info.type, new_type);

  // Confirm that the original ID can still be retrieved as before.
  auto old_path_info = permission_context()->GetLastPickedDirectory(
      kTestOrigin, kTestStartingDirectoryId);
  EXPECT_EQ(old_path_info.path, kTestPath);
  EXPECT_EQ(old_path_info.type, type);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, LimitNumberOfIds) {
  EXPECT_EQ(permission_context()
                ->GetLastPickedDirectory(kTestOrigin, kTestStartingDirectoryId)
                .path,
            base::FilePath());

  permission_context()->SetMaxIdsPerOriginForTesting(3);

  // Default path should NOT be evicted.
  auto default_id = std::string();
  auto default_path = base::FilePath::FromUTF8Unsafe("default");

  std::string id1("1");
  auto path1 = base::FilePath::FromUTF8Unsafe("path1");
  std::string id2("2");
  auto path2 = base::FilePath::FromUTF8Unsafe("path2");
  std::string id3("3");
  auto path3 = base::FilePath::FromUTF8Unsafe("path3");
  std::string id4("4");
  auto path4 = base::FilePath::FromUTF8Unsafe("path4");

  // Set the path using the default ID. This should NOT be evicted.
  permission_context()->SetLastPickedDirectory(kTestOrigin, default_id,
                                               default_path, PathType::kLocal);
  EXPECT_EQ(permission_context()
                ->GetLastPickedDirectory(kTestOrigin, default_id)
                .path,
            default_path);

  // Set the maximum number of IDs. Only set IDs should return non-empty paths.
  permission_context()->SetLastPickedDirectory(kTestOrigin, id1, path1,
                                               PathType::kLocal);
  permission_context()->SetLastPickedDirectory(kTestOrigin, id2, path2,
                                               PathType::kLocal);
  permission_context()->SetLastPickedDirectory(kTestOrigin, id3, path3,
                                               PathType::kLocal);
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id1).path,
            path1);
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id2).path,
            path2);
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id3).path,
            path3);
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id4).path,
            base::FilePath());  // Unset.

  // Once the 4th id has been set, only `id1` should have been evicted.
  permission_context()->SetLastPickedDirectory(kTestOrigin, id4, path4,
                                               PathType::kLocal);
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id1).path,
            base::FilePath());  // Unset.
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id2).path,
            path2);
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id3).path,
            path3);
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id4).path,
            path4);

  // Re-set `id1`, evicting `id2`.
  permission_context()->SetLastPickedDirectory(kTestOrigin, id1, path1,
                                               PathType::kLocal);
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id1).path,
            path1);
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id2).path,
            base::FilePath());  // Unset.
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id3).path,
            path3);
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id4).path,
            path4);

  // Ensure the default path was never evicted.
  EXPECT_EQ(permission_context()
                ->GetLastPickedDirectory(kTestOrigin, default_id)
                .path,
            default_path);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       SetLastPickedDirectory_NewPermissionContext) {
  EXPECT_EQ(permission_context()
                ->GetLastPickedDirectory(kTestOrigin, kTestStartingDirectoryId)
                .path,
            base::FilePath());

  const base::FilePath path = base::FilePath(FILE_PATH_LITERAL("/baz/bar"));

  permission_context()->SetLastPickedDirectory(
      kTestOrigin, kTestStartingDirectoryId, path, PathType::kLocal);
  ASSERT_EQ(permission_context()
                ->GetLastPickedDirectory(kTestOrigin, kTestStartingDirectoryId)
                .path,
            path);

  TestFileSystemAccessPermissionContext new_permission_context(
      browser_context());
  EXPECT_EQ(new_permission_context
                .GetLastPickedDirectory(kTestOrigin, kTestStartingDirectoryId)
                .path,
            path);

  auto new_path = path.AppendASCII("foo");
  new_permission_context.SetLastPickedDirectory(
      kTestOrigin, kTestStartingDirectoryId, new_path, PathType::kLocal);
  EXPECT_EQ(permission_context()
                ->GetLastPickedDirectory(kTestOrigin, kTestStartingDirectoryId)
                .path,
            new_path);
}

// TODO(https://crbug.com/1177334): Remove test when removing migration logic.
TEST_F(ChromeFileSystemAccessPermissionContextTest,
       Migrate_LastPickedDirectory) {
  EXPECT_EQ(permission_context()
                ->GetLastPickedDirectory(kTestOrigin, kTestStartingDirectoryId)
                .path,
            base::FilePath());

  // Set keys using the old method.
  const char kDeprecatedLastPickedDirectoryKey[] = "default-path";
  const char kDeprecatedLastPickedDirectoryTypeKey[] = "default-path-type";
  const base::FilePath path = base::FilePath(FILE_PATH_LITERAL("/baz/bar"));
  const auto type = PathType::kExternal;
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey(kDeprecatedLastPickedDirectoryKey, util::FilePathToValue(path));
  dict.SetIntKey(kDeprecatedLastPickedDirectoryTypeKey, static_cast<int>(type));
  HostContentSettingsMapFactory::GetForProfile(&profile_)
      ->SetWebsiteSettingDefaultScope(
          kTestOrigin.GetURL(), kTestOrigin.GetURL(),
          ContentSettingsType::FILE_SYSTEM_LAST_PICKED_DIRECTORY,
          base::Value::ToUniquePtrValue(std::move(dict)));

  // Retrieve key using the new method. Information should have been migrated.
  auto result = permission_context()->GetLastPickedDirectory(
      kTestOrigin, /*id=*/std::string());
  EXPECT_EQ(result.path, path);
  EXPECT_EQ(result.type, type);

  // Confirm that the old keys have been removed.
  std::unique_ptr<base::Value> value =
      HostContentSettingsMapFactory::GetForProfile(&profile_)
          ->GetWebsiteSetting(
              kTestOrigin.GetURL(), kTestOrigin.GetURL(),
              ContentSettingsType::FILE_SYSTEM_LAST_PICKED_DIRECTORY,
              /*info=*/nullptr);
  EXPECT_FALSE(value->FindKey(kDeprecatedLastPickedDirectoryKey));
  EXPECT_FALSE(value->FindIntKey(kDeprecatedLastPickedDirectoryKey));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWellKnownDirectoryPath_Base_OK) {
  base::ScopedPathOverride user_desktop_override(
      base::DIR_USER_DESKTOP, temp_dir_.GetPath(), true, true);
  EXPECT_EQ(permission_context_->GetWellKnownDirectoryPath(
                blink::mojom::WellKnownDirectory::kDirDesktop),
            temp_dir_.GetPath());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWellKnownDirectoryPath_Chrome_OK) {
  base::ScopedPathOverride user_documents_override(
      chrome::DIR_USER_DOCUMENTS, temp_dir_.GetPath(), true, true);
  EXPECT_EQ(permission_context_->GetWellKnownDirectoryPath(
                blink::mojom::WellKnownDirectory::kDirDocuments),
            temp_dir_.GetPath());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWellKnownDirectoryPath_Default) {
  base::ScopedPathOverride user_documents_override(
      chrome::DIR_USER_DOCUMENTS, temp_dir_.GetPath(), true, true);
  EXPECT_EQ(permission_context_->GetWellKnownDirectoryPath(
                blink::mojom::WellKnownDirectory::kDefault),
            temp_dir_.GetPath());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetReadPermissionGrant_InitialState_LoadFromStorage) {
  auto grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kLoadFromStorage);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetReadPermissionGrant_InitialState_Open_File) {
  auto grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetReadPermissionGrant_InitialState_Open_Directory) {
  auto grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_InitialState_LoadFromStorage) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kLoadFromStorage);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_InitialState_Open_File) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_InitialState_Open_Directory) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_InitialState_WritableImplicitState) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());

  // The existing grant should not change if the permission is blocked globally.
  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());

  // Getting a grant for the same file again should also not change the grant,
  // even now asking for more permissions is blocked globally.
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_WriteGrantedChangesExistingGrant) {
  auto grant1 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  auto grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  auto grant3 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  // All grants should be the same grant, and be granted.
  EXPECT_EQ(grant1, grant2);
  EXPECT_EQ(grant1, grant3);
  EXPECT_EQ(PermissionStatus::GRANTED, grant1->GetStatus());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_GrantIsRevokedWhenNoLongerUsed) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
  grant.reset();

  // After reset grant should go away, so new grant request should be in ASK
  // state.
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_InitialState_OpenAction_GlobalGuardBlocked) {
  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::DENIED, grant->GetStatus());
  grant.reset();

  SetContentSettingValueForOrigin(kTestOrigin,
                                  ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                  CONTENT_SETTING_ASK);

  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
}

TEST_F(
    ChromeFileSystemAccessPermissionContextTest,
    GetWritePermissionGrant_InitialState_WritableImplicitState_GlobalGuardBlocked) {
  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::DENIED, grant->GetStatus());
  grant.reset();

  SetContentSettingValueForOrigin(kTestOrigin,
                                  ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                  CONTENT_SETTING_ASK);

  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
}

TEST_F(
    ChromeFileSystemAccessPermissionContextTest,
    GetWritePermissionGrant_WriteGrantedChangesExistingGrant_GlobalGuardBlocked) {
  SetContentSettingValueForOrigin(kTestOrigin,
                                  ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                  CONTENT_SETTING_BLOCK);

  auto grant1 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  auto grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  auto grant3 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  // All grants should be the same grant, and be denied.
  EXPECT_EQ(grant1, grant2);
  EXPECT_EQ(grant1, grant3);
  EXPECT_EQ(PermissionStatus::DENIED, grant1->GetStatus());
}

TEST_F(
    ChromeFileSystemAccessPermissionContextTest,
    GetWritePermissionGrant_GrantIsRevokedWhenNoLongerUsed_GlobalGuardBlockedBeforeNewGrant) {
  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::DENIED, grant->GetStatus());
  grant.reset();

  // After reset grant should go away, but the new grant request should be in
  // DENIED state.
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::DENIED, grant->GetStatus());
}

TEST_F(
    ChromeFileSystemAccessPermissionContextTest,
    GetWritePermissionGrant_GrantIsRevokedWhenNoLongerUsed_GlobalGuardBlockedAfterNewGrant) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
  grant.reset();

  // After reset grant should go away, but the new grant request should be in
  // ASK state.
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());

  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);

  // After the guard is blocked, the permission status for |grant| should remain
  // unchanged.
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RequestPermission_Dismissed) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents_.get())
      ->set_auto_response_for_test(PermissionAction::DISMISSED);
  content::RenderFrameHostTester::For(web_contents_->GetMainFrame())
      ->SimulateUserActivation();

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);

  base::RunLoop loop;
  grant->RequestPermission(
      frame_id(), UserActivationState::kRequired,
      base::BindLambdaForTesting([&](PermissionRequestOutcome outcome) {
        EXPECT_EQ(PermissionRequestOutcome::kUserDismissed, outcome);
        loop.Quit();
      }));
  loop.Run();
  // Dismissed, so status should not change.
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, RequestPermission_Granted) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents_.get())
      ->set_auto_response_for_test(PermissionAction::GRANTED);
  content::RenderFrameHostTester::For(web_contents_->GetMainFrame())
      ->SimulateUserActivation();

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);

  base::RunLoop loop;
  grant->RequestPermission(
      frame_id(), UserActivationState::kRequired,
      base::BindLambdaForTesting([&](PermissionRequestOutcome outcome) {
        EXPECT_EQ(PermissionRequestOutcome::kUserGranted, outcome);
        loop.Quit();
      }));
  loop.Run();
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, RequestPermission_Denied) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents_.get())
      ->set_auto_response_for_test(PermissionAction::DENIED);
  content::RenderFrameHostTester::For(web_contents_->GetMainFrame())
      ->SimulateUserActivation();

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);

  base::RunLoop loop;
  grant->RequestPermission(
      frame_id(), UserActivationState::kRequired,
      base::BindLambdaForTesting([&](PermissionRequestOutcome outcome) {
        EXPECT_EQ(PermissionRequestOutcome::kUserDenied, outcome);
        loop.Quit();
      }));
  loop.Run();
  EXPECT_EQ(PermissionStatus::DENIED, grant->GetStatus());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RequestPermission_NoUserActivation) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents_.get())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);

  base::RunLoop loop;
  grant->RequestPermission(
      frame_id(), UserActivationState::kRequired,
      base::BindLambdaForTesting([&](PermissionRequestOutcome outcome) {
        EXPECT_EQ(PermissionRequestOutcome::kNoUserActivation, outcome);
        loop.Quit();
      }));
  loop.Run();
  // No user activation, so status should not change.
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RequestPermission_NoUserActivation_UserActivationNotRequired) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents_.get())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);

  base::RunLoop loop;
  grant->RequestPermission(
      frame_id(), UserActivationState::kNotRequired,
      base::BindLambdaForTesting([&](PermissionRequestOutcome outcome) {
        EXPECT_EQ(PermissionRequestOutcome::kUserGranted, outcome);
        loop.Quit();
      }));
  loop.Run();
  // No user activation, so status should not change.
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RequestPermission_AlreadyGranted) {
  // If the permission has already been granted, a call to RequestPermission()
  // should call the passed-in callback and return immediately without showing a
  // prompt.
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);

  base::RunLoop loop;
  grant->RequestPermission(
      frame_id(), UserActivationState::kRequired,
      base::BindLambdaForTesting([&](PermissionRequestOutcome outcome) {
        EXPECT_EQ(PermissionRequestOutcome::kRequestAborted, outcome);
        loop.Quit();
      }));
  loop.Run();
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RequestPermission_GlobalGuardBlockedBeforeOpenGrant) {
  // If the guard content setting is blocked, a call to RequestPermission()
  // should update the PermissionStatus to DENIED, call the passed-in
  // callback, and return immediately without showing a prompt.
  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);

  base::RunLoop loop;
  grant->RequestPermission(
      frame_id(), UserActivationState::kRequired,
      base::BindLambdaForTesting([&](PermissionRequestOutcome outcome) {
        EXPECT_EQ(PermissionRequestOutcome::kRequestAborted, outcome);
        loop.Quit();
      }));
  loop.Run();
  EXPECT_EQ(PermissionStatus::DENIED, grant->GetStatus());

  auto grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin2, kTestPath, HandleType::kFile, UserAction::kOpen);

  base::RunLoop loop2;
  grant2->RequestPermission(
      frame_id(), UserActivationState::kRequired,
      base::BindLambdaForTesting([&](PermissionRequestOutcome outcome) {
        EXPECT_EQ(PermissionRequestOutcome::kRequestAborted, outcome);
        loop2.Quit();
      }));
  loop2.Run();
  EXPECT_EQ(PermissionStatus::DENIED, grant2->GetStatus());

  grant2.reset();
  SetContentSettingValueForOrigin(kTestOrigin2,
                                  ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                  CONTENT_SETTING_ASK);

  grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin2, kTestPath, HandleType::kFile, UserAction::kOpen);

  base::RunLoop loop3;
  grant2->RequestPermission(
      frame_id(), UserActivationState::kRequired,
      base::BindLambdaForTesting([&](PermissionRequestOutcome outcome) {
        EXPECT_EQ(PermissionRequestOutcome::kNoUserActivation, outcome);
        loop3.Quit();
      }));
  loop3.Run();
  EXPECT_EQ(PermissionStatus::ASK, grant2->GetStatus());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RequestPermission_GlobalGuardBlockedAfterOpenGrant) {
  // If the guard content setting is blocked, a call to RequestPermission()
  // should update the PermissionStatus to DENIED, call the passed-in
  // callback, and return immediately without showing a prompt.
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  auto grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin2, kTestPath, HandleType::kFile, UserAction::kOpen);

  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);

  base::RunLoop loop;
  grant->RequestPermission(
      frame_id(), UserActivationState::kRequired,
      base::BindLambdaForTesting([&](PermissionRequestOutcome outcome) {
        EXPECT_EQ(PermissionRequestOutcome::kBlockedByContentSetting, outcome);
        loop.Quit();
      }));
  loop.Run();
  EXPECT_EQ(PermissionStatus::DENIED, grant->GetStatus());

  base::RunLoop loop2;
  grant2->RequestPermission(
      frame_id(), UserActivationState::kRequired,
      base::BindLambdaForTesting([&](PermissionRequestOutcome outcome) {
        EXPECT_EQ(PermissionRequestOutcome::kBlockedByContentSetting, outcome);
        loop2.Quit();
      }));
  loop2.Run();
  EXPECT_EQ(PermissionStatus::DENIED, grant2->GetStatus());

  grant.reset();
  grant2.reset();

  SetContentSettingValueForOrigin(kTestOrigin,
                                  ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                  CONTENT_SETTING_ASK);
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin2, kTestPath, HandleType::kFile, UserAction::kOpen);

  base::RunLoop loop3;
  grant->RequestPermission(
      frame_id(), UserActivationState::kRequired,
      base::BindLambdaForTesting([&](PermissionRequestOutcome outcome) {
        EXPECT_EQ(PermissionRequestOutcome::kNoUserActivation, outcome);
        loop3.Quit();
      }));
  loop3.Run();
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());

  base::RunLoop loop4;
  grant2->RequestPermission(
      frame_id(), UserActivationState::kRequired,
      base::BindLambdaForTesting([&](PermissionRequestOutcome outcome) {
        EXPECT_EQ(PermissionRequestOutcome::kRequestAborted, outcome);
        loop4.Quit();
      }));
  loop4.Run();
  EXPECT_EQ(PermissionStatus::DENIED, grant2->GetStatus());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_AllowlistedOrigin_InitialState) {
  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);

  auto* allowlist = WebUIAllowlist::GetOrCreate(browser_context());
  allowlist->RegisterAutoGrantedPermission(
      kChromeOrigin, ContentSettingsType::FILE_SYSTEM_READ_GUARD);
  allowlist->RegisterAutoGrantedPermission(
      kChromeOrigin, ContentSettingsType::FILE_SYSTEM_WRITE_GUARD);

  // Allowlisted origin automatically gets write permission.
  auto grant1 = permission_context()->GetWritePermissionGrant(
      kChromeOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::GRANTED, grant1->GetStatus());

  auto grant2 = permission_context()->GetWritePermissionGrant(
      kChromeOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::GRANTED, grant2->GetStatus());

  // Other origin should gets blocked.
  auto grant3 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::DENIED, grant3->GetStatus());
  auto grant4 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::DENIED, grant4->GetStatus());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_AllowlistedOrigin_ExistingGrant) {
  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);

  auto* allowlist = WebUIAllowlist::GetOrCreate(browser_context());
  allowlist->RegisterAutoGrantedPermission(
      kChromeOrigin, ContentSettingsType::FILE_SYSTEM_READ_GUARD);
  allowlist->RegisterAutoGrantedPermission(
      kChromeOrigin, ContentSettingsType::FILE_SYSTEM_WRITE_GUARD);

  // Initial grant (file).
  auto grant1 = permission_context()->GetWritePermissionGrant(
      kChromeOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::GRANTED, grant1->GetStatus());

  // Existing grant (file).
  auto grant2 = permission_context()->GetWritePermissionGrant(
      kChromeOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::GRANTED, grant2->GetStatus());

  // Initial grant (directory).
  auto grant3 = permission_context()->GetWritePermissionGrant(
      kChromeOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::GRANTED, grant3->GetStatus());

  // Existing grant (directory).
  auto grant4 = permission_context()->GetWritePermissionGrant(
      kChromeOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::GRANTED, grant4->GetStatus());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetReadPermissionGrant_FileBecomesDirectory) {
  auto file_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::GRANTED, file_grant->GetStatus());

  auto directory_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, directory_grant->GetStatus());

  // Requesting a permission grant for a directory which was previously a file
  // should have revoked the original file permission.
  EXPECT_EQ(PermissionStatus::DENIED, file_grant->GetStatus());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_FileBecomesDirectory) {
  auto file_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, file_grant->GetStatus());

  auto directory_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, directory_grant->GetStatus());

  // Requesting a permission grant for a directory which was previously a file
  // should have revoked the original file permission.
  EXPECT_EQ(PermissionStatus::DENIED, file_grant->GetStatus());
}

#endif  // !defined(OS_ANDROID)
