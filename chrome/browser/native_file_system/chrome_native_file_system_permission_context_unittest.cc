// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/native_file_system/chrome_native_file_system_permission_context.h"

#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_path_override.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using content::BrowserContext;
using HandleType = ChromeNativeFileSystemPermissionContext::HandleType;
using PathType = ChromeNativeFileSystemPermissionContext::PathType;
using UserAction = ChromeNativeFileSystemPermissionContext::UserAction;
using PermissionStatus =
    content::NativeFileSystemPermissionGrant::PermissionStatus;
using PermissionRequestOutcome =
    content::NativeFileSystemPermissionGrant::PermissionRequestOutcome;
using SensitiveDirectoryResult =
    ChromeNativeFileSystemPermissionContext::SensitiveDirectoryResult;

class TestNativeFileSystemPermissionContext
    : public ChromeNativeFileSystemPermissionContext {
 public:
  explicit TestNativeFileSystemPermissionContext(
      content::BrowserContext* context)
      : ChromeNativeFileSystemPermissionContext(context) {}
  ~TestNativeFileSystemPermissionContext() override = default;

  // content::NativeFileSystemPermissionContext:
  scoped_refptr<content::NativeFileSystemPermissionGrant>
  GetReadPermissionGrant(const url::Origin& origin,
                         const base::FilePath& path,
                         HandleType handle_type,
                         UserAction user_action) override {
    NOTREACHED();
    return nullptr;
  }
  scoped_refptr<content::NativeFileSystemPermissionGrant>
  GetWritePermissionGrant(const url::Origin& origin,
                          const base::FilePath& path,
                          HandleType handle_type,
                          UserAction user_action) override {
    NOTREACHED();
    return nullptr;
  }

  // ChromeNativeFileSystemPermissionContext:
  Grants GetPermissionGrants(const url::Origin& origin) override {
    NOTREACHED();
    return {};
  }
  void RevokeGrants(const url::Origin& origin) override { NOTREACHED(); }

 private:
  base::WeakPtr<ChromeNativeFileSystemPermissionContext> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  base::WeakPtrFactory<TestNativeFileSystemPermissionContext> weak_factory_{
      this};
};

class ChromeNativeFileSystemPermissionContextTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    permission_context_ =
        std::make_unique<TestNativeFileSystemPermissionContext>(
            browser_context());
  }

  void TearDown() override {
    ASSERT_TRUE(temp_dir_.Delete());
  }

  SensitiveDirectoryResult ConfirmSensitiveDirectoryAccessSync(
      ChromeNativeFileSystemPermissionContext* context,
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

  ChromeNativeFileSystemPermissionContext* permission_context() {
    return permission_context_.get();
  }
  BrowserContext* browser_context() { return &profile_; }
  TestingProfile* profile() { return &profile_; }

 protected:
  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kTestOrigin2 =
      url::Origin::Create(GURL("https://test.com"));
  const base::FilePath kTestPath =
      base::FilePath(FILE_PATH_LITERAL("/foo/bar"));
  const url::Origin kChromeOrigin = url::Origin::Create(GURL("chrome://test"));

  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ChromeNativeFileSystemPermissionContext> permission_context_;
  TestingProfile profile_;
};

#if !defined(OS_ANDROID)

TEST_F(ChromeNativeFileSystemPermissionContextTest,
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

TEST_F(ChromeNativeFileSystemPermissionContextTest,
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

TEST_F(ChromeNativeFileSystemPermissionContextTest,
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

TEST_F(ChromeNativeFileSystemPermissionContextTest,
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

TEST_F(ChromeNativeFileSystemPermissionContextTest,
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

TEST_F(ChromeNativeFileSystemPermissionContextTest,
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

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       CanObtainWritePermission_ContentSettingAsk) {
  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_ASK);
  EXPECT_TRUE(permission_context()->CanObtainWritePermission(kTestOrigin));
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       CanObtainWritePermission_ContentSettingsBlock) {
  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(permission_context()->CanObtainWritePermission(kTestOrigin));
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       CanObtainWritePermission_ContentSettingAllow) {
  // Note, chrome:// scheme is whitelisted. But we can't set default content
  // setting here because ALLOW is not an acceptable option.
  EXPECT_TRUE(permission_context()->CanObtainWritePermission(kChromeOrigin));
}

TEST_F(ChromeNativeFileSystemPermissionContextTest, PolicyReadGuardPermission) {
  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedDefaultFileSystemReadGuardSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));

  EXPECT_FALSE(permission_context()->CanObtainReadPermission(kTestOrigin));
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       PolicyWriteGuardPermission) {
  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedDefaultFileSystemWriteGuardSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));

  EXPECT_FALSE(permission_context()->CanObtainWritePermission(kTestOrigin));
}

TEST_F(ChromeNativeFileSystemPermissionContextTest, PolicyReadAskForUrls) {
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

TEST_F(ChromeNativeFileSystemPermissionContextTest, PolicyReadBlockedForUrls) {
  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedFileSystemReadBlockedForUrls,
                        base::JSONReader::ReadDeprecated(
                            "[\"" + kTestOrigin.Serialize() + "\"]"));

  EXPECT_FALSE(permission_context()->CanObtainReadPermission(kTestOrigin));
  EXPECT_TRUE(permission_context()->CanObtainReadPermission(kTestOrigin2));
}

TEST_F(ChromeNativeFileSystemPermissionContextTest, PolicyWriteAskForUrls) {
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

TEST_F(ChromeNativeFileSystemPermissionContextTest, PolicyWriteBlockedForUrls) {
  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedFileSystemWriteBlockedForUrls,
                        base::JSONReader::ReadDeprecated(
                            "[\"" + kTestOrigin.Serialize() + "\"]"));

  EXPECT_FALSE(permission_context()->CanObtainWritePermission(kTestOrigin));
  EXPECT_TRUE(permission_context()->CanObtainWritePermission(kTestOrigin2));
}

TEST_F(ChromeNativeFileSystemPermissionContextTest, GetLastPickedDirectory) {
  auto file_info = permission_context()->GetLastPickedDirectory(kTestOrigin);
  EXPECT_EQ(file_info.path, base::FilePath());
  EXPECT_EQ(file_info.type, PathType::kLocal);
}

TEST_F(ChromeNativeFileSystemPermissionContextTest, SetLastPickedDirectory) {
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin).path,
            base::FilePath());

  auto type = PathType::kLocal;
  permission_context()->SetLastPickedDirectory(kTestOrigin, kTestPath, type);
  auto path_info = permission_context()->GetLastPickedDirectory(kTestOrigin);
  EXPECT_EQ(path_info.path, kTestPath);
  EXPECT_EQ(path_info.type, type);

  auto new_path = path_info.path.AppendASCII("baz");
  auto new_type = PathType::kExternal;
  permission_context()->SetLastPickedDirectory(kTestOrigin, new_path, new_type);
  auto new_path_info =
      permission_context()->GetLastPickedDirectory(kTestOrigin);
  EXPECT_EQ(new_path_info.path, new_path);
  EXPECT_EQ(new_path_info.type, new_type);
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       SetLastPickedDirectory_NewPermissionContext) {
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin).path,
            base::FilePath());

  const base::FilePath path = base::FilePath(FILE_PATH_LITERAL("/baz/bar"));

  permission_context()->SetLastPickedDirectory(kTestOrigin, path,
                                               PathType::kLocal);
  ASSERT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin).path,
            path);

  TestNativeFileSystemPermissionContext new_permission_context(
      browser_context());
  EXPECT_EQ(new_permission_context.GetLastPickedDirectory(kTestOrigin).path,
            path);

  auto new_path = path.AppendASCII("foo");
  new_permission_context.SetLastPickedDirectory(kTestOrigin, new_path,
                                                PathType::kLocal);
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin).path,
            new_path);
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       GetCommonDirectoryPath_Base_OK) {
  base::ScopedPathOverride user_desktop_override(
      base::DIR_USER_DESKTOP, temp_dir_.GetPath(), true, true);
  EXPECT_EQ(permission_context_->GetCommonDirectoryPath(
                blink::mojom::CommonDirectory::kDirDesktop),
            temp_dir_.GetPath());
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       GetCommonDirectoryPath_Chrome_OK) {
  base::ScopedPathOverride user_documents_override(
      chrome::DIR_USER_DOCUMENTS, temp_dir_.GetPath(), true, true);
  EXPECT_EQ(permission_context_->GetCommonDirectoryPath(
                blink::mojom::CommonDirectory::kDirDocuments),
            temp_dir_.GetPath());
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       GetCommonDirectoryPath_Default) {
  base::ScopedPathOverride user_documents_override(
      chrome::DIR_USER_DOCUMENTS, temp_dir_.GetPath(), true, true);
  EXPECT_EQ(permission_context_->GetCommonDirectoryPath(
                blink::mojom::CommonDirectory::kDefault),
            temp_dir_.GetPath());
}

#endif  // !defined(OS_ANDROID)
