// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"

#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/values_util.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_path_override.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_file_util.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/file_system_access/file_system_access_permission_request_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/permission_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/webui/webui_allowlist.h"
#include "url/gurl.h"
#include "url/origin.h"

using content::BrowserContext;
using content::WebContents;
using content::WebContentsTester;
using permissions::PermissionAction;
using GrantType = ChromeFileSystemAccessPermissionContext::GrantType;
using HandleType = ChromeFileSystemAccessPermissionContext::HandleType;
using PathType = ChromeFileSystemAccessPermissionContext::PathType;
using UserAction = ChromeFileSystemAccessPermissionContext::UserAction;
using PermissionStatus =
    content::FileSystemAccessPermissionGrant::PermissionStatus;
using PersistedPermissionOptions =
    ChromeFileSystemAccessPermissionContext::PersistedPermissionOptions;
using PermissionRequestOutcome =
    content::FileSystemAccessPermissionGrant::PermissionRequestOutcome;
using SensitiveDirectoryResult =
    ChromeFileSystemAccessPermissionContext::SensitiveEntryResult;
using UserActivationState =
    content::FileSystemAccessPermissionGrant::UserActivationState;

class TestFileSystemAccessPermissionContext
    : public ChromeFileSystemAccessPermissionContext {
 public:
  explicit TestFileSystemAccessPermissionContext(
      content::BrowserContext* context,
      const base::Clock* clock)
      : ChromeFileSystemAccessPermissionContext(context, clock) {
    EXPECT_EQ(
        base::FeatureList::IsEnabled(
            features::kFileSystemAccessPersistentPermissions),
        periodic_sweep_persisted_permissions_timer_for_testing().IsRunning());
    periodic_sweep_persisted_permissions_timer_for_testing().Stop();
  }
  ~TestFileSystemAccessPermissionContext() override = default;

 private:
  base::WeakPtrFactory<TestFileSystemAccessPermissionContext> weak_factory_{
      this};
};

class ChromeFileSystemAccessPermissionContextTest : public testing::Test {
 public:
  ChromeFileSystemAccessPermissionContextTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kFileSystemAccessPersistentPermissions);
  }
  void SetUp() override {
    // Create a scoped directory under %TEMP% instead of using
    // `base::ScopedTempDir::CreateUniqueTempDir`.
    // `base::ScopedTempDir::CreateUniqueTempDir` creates a path under
    // %ProgramFiles% on Windows when running as Admin, which is a blocked path
    // (`kBlockedPaths`). This can fail some of the tests.
    ASSERT_TRUE(
        temp_dir_.CreateUniqueTempDirUnderPath(base::GetTempDirForTesting()));

    DownloadCoreServiceFactory::GetForBrowserContext(profile())
        ->SetDownloadManagerDelegateForTesting(
            std::make_unique<ChromeDownloadManagerDelegate>(profile()));

    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    FileSystemAccessPermissionRequestManager::CreateForWebContents(
        web_contents());
    content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(kTestOrigin.GetURL());

    FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
        ->set_auto_response_for_test(PermissionAction::DISMISSED);
    permission_context_ =
        std::make_unique<TestFileSystemAccessPermissionContext>(
            browser_context(), task_environment_.GetMockClock());
  }

  void TearDown() override {
    task_environment_.RunUntilIdle();
    ASSERT_TRUE(temp_dir_.Delete());
    web_contents_.reset();
  }

  SensitiveDirectoryResult ConfirmSensitiveEntryAccessSync(
      ChromeFileSystemAccessPermissionContext* context,
      PathType path_type,
      const base::FilePath& path,
      HandleType handle_type,
      UserAction user_action) {
    base::test::TestFuture<
        ChromeFileSystemAccessPermissionContext::SensitiveEntryResult>
        future;
    permission_context_->ConfirmSensitiveEntryAccess(
        kTestOrigin, path_type, path, handle_type, user_action,
        content::GlobalRenderFrameHostId(), future.GetCallback());
    return future.Get();
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

  void ExpectUmaEntryPersistedPermissionAge(base::TimeDelta age, int count) {
    histogram_tester_.ExpectTimeBucketCount(
        "Storage.FileSystemAccess.PersistedPermissions.Age.NonPWA", age, count);
  }

  ChromeFileSystemAccessPermissionContext* permission_context() {
    return permission_context_.get();
  }
  BrowserContext* browser_context() { return &profile_; }
  TestingProfile* profile() { return &profile_; }
  WebContents* web_contents() { return web_contents_.get(); }

  int process_id() {
    return web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();
  }

  content::GlobalRenderFrameHostId frame_id() {
    return content::GlobalRenderFrameHostId(
        process_id(), web_contents()->GetPrimaryMainFrame()->GetRoutingID());
  }

  base::Time Now() const { return task_environment_.GetMockClock()->Now(); }
  void Advance(base::TimeDelta delta) { task_environment_.AdvanceClock(delta); }

 protected:
  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kTestOrigin2 =
      url::Origin::Create(GURL("https://test.com"));
  const url::Origin kPdfOrigin = url::Origin::Create(
      GURL("chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/index.html"));
  const std::string kTestStartingDirectoryId = "test_id";
  const base::FilePath kTestPath =
      base::FilePath(FILE_PATH_LITERAL("/foo/bar"));
  const url::Origin kChromeOrigin = url::Origin::Create(GURL("chrome://test"));

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ChromeFileSystemAccessPermissionContext> permission_context_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  TestingProfile profile_;
  std::unique_ptr<WebContents> web_contents_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
};

class ChromeFileSystemAccessPermissionContextNoPersistenceTest
    : public ChromeFileSystemAccessPermissionContextTest {
 public:
  ChromeFileSystemAccessPermissionContextNoPersistenceTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kFileSystemAccessPersistentPermissions);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#if !BUILDFLAG(IS_ANDROID)

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_NoSpecialPath) {
  const base::FilePath kTestPath =
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
      base::FilePath(FILE_PATH_LITERAL("c:\\foo\\bar"));
#else
      base::FilePath(FILE_PATH_LITERAL("/foo/bar"));
#endif

  // Path outside any special directories should be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAllowed,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal, kTestPath,
                HandleType::kFile, UserAction::kOpen));
  EXPECT_EQ(SensitiveDirectoryResult::kAllowed,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal, kTestPath,
                HandleType::kDirectory, UserAction::kOpen));

  // External (relative) paths should also be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAllowed,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kExternal,
                base::FilePath(FILE_PATH_LITERAL("foo/bar")), HandleType::kFile,
                UserAction::kOpen));

  // Path outside any special directories via no user action should be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAllowed,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal, kTestPath,
                HandleType::kDirectory, UserAction::kNone));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_DontBlockAllChildren) {
  base::FilePath home_dir = temp_dir_.GetPath().AppendASCII("home");
  base::ScopedPathOverride home_override(base::DIR_HOME, home_dir, true, true);

  // Home directory itself should not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal, home_dir,
                HandleType::kDirectory, UserAction::kOpen));
  // Parent of home directory should also not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal, temp_dir_.GetPath(),
                HandleType::kDirectory, UserAction::kOpen));
  // Paths inside home directory should be allowed.
  EXPECT_EQ(
      SensitiveDirectoryResult::kAllowed,
      ConfirmSensitiveEntryAccessSync(permission_context(), PathType::kLocal,
                                      home_dir.AppendASCII("foo"),
                                      HandleType::kFile, UserAction::kOpen));
  EXPECT_EQ(
      SensitiveDirectoryResult::kAllowed,
      ConfirmSensitiveEntryAccessSync(
          permission_context(), PathType::kLocal, home_dir.AppendASCII("foo"),
          HandleType::kDirectory, UserAction::kOpen));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_BlockAllChildren) {
  base::FilePath app_dir = temp_dir_.GetPath().AppendASCII("app");
  base::ScopedPathOverride app_override(base::DIR_EXE, app_dir, true, true);

  // App directory itself should not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal, app_dir,
                HandleType::kDirectory, UserAction::kOpen));
  // Parent of App directory should also not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal, temp_dir_.GetPath(),
                HandleType::kDirectory, UserAction::kOpen));
  // Paths inside App directory should also not be allowed.
  EXPECT_EQ(
      SensitiveDirectoryResult::kAbort,
      ConfirmSensitiveEntryAccessSync(permission_context(), PathType::kLocal,
                                      app_dir.AppendASCII("foo"),
                                      HandleType::kFile, UserAction::kOpen));
  EXPECT_EQ(
      SensitiveDirectoryResult::kAbort,
      ConfirmSensitiveEntryAccessSync(
          permission_context(), PathType::kLocal, app_dir.AppendASCII("foo"),
          HandleType::kDirectory, UserAction::kOpen));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_BlockChildrenNested) {
  base::FilePath user_data_dir = temp_dir_.GetPath().AppendASCII("user");
  base::ScopedPathOverride user_data_override(chrome::DIR_USER_DATA,
                                              user_data_dir, true, true);
  base::FilePath download_dir = user_data_dir.AppendASCII("downloads");
  base::ScopedPathOverride download_override(chrome::DIR_DEFAULT_DOWNLOADS,
                                             download_dir, true, true);

  // User Data directory itself should not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal, user_data_dir,
                HandleType::kDirectory, UserAction::kOpen));
  // Parent of User Data directory should also not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal, temp_dir_.GetPath(),
                HandleType::kDirectory, UserAction::kOpen));
  // The nested Download directory itself should not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal, download_dir,
                HandleType::kDirectory, UserAction::kOpen));
  // Paths inside the nested Download directory should be allowed.
  EXPECT_EQ(
      SensitiveDirectoryResult::kAllowed,
      ConfirmSensitiveEntryAccessSync(permission_context(), PathType::kLocal,
                                      download_dir.AppendASCII("foo"),
                                      HandleType::kFile, UserAction::kOpen));
  EXPECT_EQ(SensitiveDirectoryResult::kAllowed,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                download_dir.AppendASCII("foo"), HandleType::kDirectory,
                UserAction::kOpen));

#if BUILDFLAG(IS_WIN)
  // DIR_IE_INTERNET_CACHE is an example of a directory where nested directories
  // are blocked, but nested files should be allowed.
  base::FilePath internet_cache = user_data_dir.AppendASCII("INetCache");
  base::ScopedPathOverride internet_cache_override(base::DIR_IE_INTERNET_CACHE,
                                                   internet_cache, true, true);

  // The nested INetCache directory itself should not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal, internet_cache,
                HandleType::kDirectory, UserAction::kOpen));
  // Files inside the nested INetCache directory should be allowed.
  EXPECT_EQ(
      SensitiveDirectoryResult::kAllowed,
      ConfirmSensitiveEntryAccessSync(permission_context(), PathType::kLocal,
                                      internet_cache.AppendASCII("foo"),
                                      HandleType::kFile, UserAction::kOpen));
  // But directories should be blocked.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                internet_cache.AppendASCII("foo"), HandleType::kDirectory,
                UserAction::kOpen));
#endif
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_RelativePathBlock) {
  base::FilePath home_dir = temp_dir_.GetPath().AppendASCII("home");
  base::ScopedPathOverride home_override(base::DIR_HOME, home_dir, true, true);

  // ~/.ssh should be blocked
  EXPECT_EQ(
      SensitiveDirectoryResult::kAbort,
      ConfirmSensitiveEntryAccessSync(
          permission_context(), PathType::kLocal, home_dir.AppendASCII(".ssh"),
          HandleType::kDirectory, UserAction::kOpen));
  // And anything inside ~/.ssh should also be blocked
  EXPECT_EQ(
      SensitiveDirectoryResult::kAbort,
      ConfirmSensitiveEntryAccessSync(permission_context(), PathType::kLocal,
                                      home_dir.AppendASCII(".ssh/id_rsa"),
                                      HandleType::kFile, UserAction::kOpen));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_ExplicitPathBlock) {
// Linux is the only OS where we have some blocked directories with explicit
// paths (as opposed to PathService provided paths).
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // /dev should be blocked.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                base::FilePath(FILE_PATH_LITERAL("/dev")),
                HandleType::kDirectory, UserAction::kOpen));
  // As well as children of /dev.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                base::FilePath(FILE_PATH_LITERAL("/dev/foo")),
                HandleType::kDirectory, UserAction::kOpen));
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                base::FilePath(FILE_PATH_LITERAL("/dev/foo")),
                HandleType::kFile, UserAction::kOpen));
  // Even if user action is none, a blocklisted path should be blocked.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                base::FilePath(FILE_PATH_LITERAL("/dev")),
                HandleType::kDirectory, UserAction::kNone));
#elif BUILDFLAG(IS_WIN)
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                base::FilePath(FILE_PATH_LITERAL("c:\\Program Files")),
                HandleType::kDirectory, UserAction::kOpen));
#endif
}

#if BUILDFLAG(IS_MAC)
TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_DontBlockAllChildren_Overlapping) {
  base::FilePath home_dir = temp_dir_.GetPath().AppendASCII("home");
  base::ScopedPathOverride home_override(base::DIR_HOME, home_dir, true, true);

  // Home directory itself should not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal, home_dir,
                HandleType::kDirectory, UserAction::kOpen));
  // $HOME/Library should be blocked.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                home_dir.AppendASCII("Library"), HandleType::kDirectory,
                UserAction::kOpen));
  // $HOME/Library/Mobile Documents should be blocked.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                home_dir.AppendASCII("Library/Mobile Documents"),
                HandleType::kDirectory, UserAction::kOpen));
  // Paths within $HOME/Library/Mobile Documents should not be blocked.
  EXPECT_EQ(SensitiveDirectoryResult::kAllowed,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                home_dir.AppendASCII("Library/Mobile Documents/foo"),
                HandleType::kDirectory, UserAction::kOpen));
  // Except for $HOME/Library/Mobile Documents/com~apple~CloudDocs, which should
  // be blocked.
  EXPECT_EQ(
      SensitiveDirectoryResult::kAbort,
      ConfirmSensitiveEntryAccessSync(
          permission_context(), PathType::kLocal,
          home_dir.AppendASCII("Library/Mobile Documents/com~apple~CloudDocs"),
          HandleType::kDirectory, UserAction::kOpen));
  // Paths within $HOME/Library/Mobile Documents/com~apple~CloudDocs should not
  // be blocked.
  EXPECT_EQ(SensitiveDirectoryResult::kAllowed,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                home_dir.AppendASCII(
                    "Library/Mobile Documents/com~apple~CloudDocs/foo"),
                HandleType::kDirectory, UserAction::kOpen));
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_UNCPath) {
  if (!base::FeatureList::IsEnabled(
          features::kFileSystemAccessLocalUNCPathBlock)) {
    return;
  }

  EXPECT_EQ(
      SensitiveDirectoryResult::kAllowed,
      ConfirmSensitiveEntryAccessSync(
          permission_context(), PathType::kLocal,
          base::FilePath(FILE_PATH_LITERAL("\\\\server\\share\\foo\\bar")),
          HandleType::kDirectory, UserAction::kOpen));

  EXPECT_EQ(SensitiveDirectoryResult::kAllowed,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                base::FilePath(FILE_PATH_LITERAL("c:\\\\foo\\bar")),
                HandleType::kDirectory, UserAction::kOpen));

  EXPECT_EQ(
      SensitiveDirectoryResult::kAbort,
      ConfirmSensitiveEntryAccessSync(
          permission_context(), PathType::kLocal,
          base::FilePath(FILE_PATH_LITERAL("\\\\localhost\\c$\\foo\\bar")),
          HandleType::kDirectory, UserAction::kOpen));

  EXPECT_EQ(
      SensitiveDirectoryResult::kAbort,
      ConfirmSensitiveEntryAccessSync(
          permission_context(), PathType::kLocal,
          base::FilePath(FILE_PATH_LITERAL("\\\\LOCALHOST\\c$\\foo\\bar")),
          HandleType::kDirectory, UserAction::kOpen));

  EXPECT_EQ(
      SensitiveDirectoryResult::kAbort,
      ConfirmSensitiveEntryAccessSync(
          permission_context(), PathType::kLocal,
          base::FilePath(FILE_PATH_LITERAL("\\\\127.0.0.1\\c$\\foo\\bar")),
          HandleType::kDirectory, UserAction::kOpen));

  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                base::FilePath(FILE_PATH_LITERAL("\\\\.\\c:\\foo\\bar")),
                HandleType::kDirectory, UserAction::kOpen));

  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                base::FilePath(FILE_PATH_LITERAL("\\\\?\\c:\\foo\\bar")),
                HandleType::kDirectory, UserAction::kOpen));

  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                base::FilePath(FILE_PATH_LITERAL(
                    "\\\\;LanmanRedirector\\localhost\\c$\\foo\\bar")),
                HandleType::kDirectory, UserAction::kOpen));

  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                base::FilePath(
                    FILE_PATH_LITERAL("\\\\.\\UNC\\LOCALHOST\\c:\\foo\\bar")),
                HandleType::kDirectory, UserAction::kOpen));

  EXPECT_EQ(
      SensitiveDirectoryResult::kAbort,
      ConfirmSensitiveEntryAccessSync(
          permission_context(), PathType::kLocal,
          base::FilePath(FILE_PATH_LITERAL("\\\\myhostname\\c$\\foo\\bar")),
          HandleType::kDirectory, UserAction::kOpen));
}
#endif

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_DangerousFile) {
  // Saving files with a harmless extension should be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAllowed,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                temp_dir_.GetPath().AppendASCII("test.txt"), HandleType::kFile,
                UserAction::kSave));
  // Saving files with a dangerous extension should show a prompt.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                temp_dir_.GetPath().AppendASCII("test.swf"), HandleType::kFile,
                UserAction::kSave));
  // Files with a dangerous extension from no user action should be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAllowed,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                temp_dir_.GetPath().AppendASCII("test.swf"), HandleType::kFile,
                UserAction::kNone));
  // Opening files with a dangerous extension should be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAllowed,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                temp_dir_.GetPath().AppendASCII("test.swf"), HandleType::kFile,
                UserAction::kOpen));
  // Opening files with a dangerous compound extension should show a prompt.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                temp_dir_.GetPath().AppendASCII("test.txt.swf"),
                HandleType::kFile, UserAction::kSave));
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
  prefs->SetManagedPref(
      prefs::kManagedFileSystemReadAskForUrls,
      base::test::ParseJsonList("[\"" + kTestOrigin.Serialize() + "\"]"));

  EXPECT_TRUE(permission_context()->CanObtainReadPermission(kTestOrigin));
  EXPECT_FALSE(permission_context()->CanObtainReadPermission(kTestOrigin2));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, PolicyReadBlockedForUrls) {
  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(
      prefs::kManagedFileSystemReadBlockedForUrls,
      base::test::ParseJsonList("[\"" + kTestOrigin.Serialize() + "\"]"));

  EXPECT_FALSE(permission_context()->CanObtainReadPermission(kTestOrigin));
  EXPECT_TRUE(permission_context()->CanObtainReadPermission(kTestOrigin2));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, PolicyWriteAskForUrls) {
  // Set the default to "block" so that the policy being tested overrides it.
  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedDefaultFileSystemWriteGuardSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  prefs->SetManagedPref(
      prefs::kManagedFileSystemWriteAskForUrls,
      base::test::ParseJsonList("[\"" + kTestOrigin.Serialize() + "\"]"));

  EXPECT_TRUE(permission_context()->CanObtainWritePermission(kTestOrigin));
  EXPECT_FALSE(permission_context()->CanObtainWritePermission(kTestOrigin2));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, PolicyWriteBlockedForUrls) {
  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(
      prefs::kManagedFileSystemWriteBlockedForUrls,
      base::test::ParseJsonList("[\"" + kTestOrigin.Serialize() + "\"]"));

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
  Advance(base::Minutes(1));
  permission_context()->SetLastPickedDirectory(kTestOrigin, id2, path2,
                                               PathType::kLocal);
  Advance(base::Minutes(1));
  permission_context()->SetLastPickedDirectory(kTestOrigin, id3, path3,
                                               PathType::kLocal);
  Advance(base::Minutes(1));
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
  Advance(base::Minutes(1));
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
  Advance(base::Minutes(1));
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
      browser_context(), task_environment_.GetMockClock());
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

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWellKnownDirectoryPath_Base_OK) {
  base::ScopedPathOverride user_desktop_override(
      base::DIR_USER_DESKTOP, temp_dir_.GetPath(), true, true);
  EXPECT_EQ(permission_context()->GetWellKnownDirectoryPath(
                blink::mojom::WellKnownDirectory::kDirDesktop, kTestOrigin),
            temp_dir_.GetPath());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWellKnownDirectoryPath_Chrome_OK) {
  base::ScopedPathOverride user_documents_override(
      chrome::DIR_USER_DOCUMENTS, temp_dir_.GetPath(), true, true);
  EXPECT_EQ(permission_context()->GetWellKnownDirectoryPath(
                blink::mojom::WellKnownDirectory::kDirDocuments, kTestOrigin),
            temp_dir_.GetPath());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWellKnownDirectoryPath_Default) {
  base::ScopedPathOverride user_documents_override(
      chrome::DIR_USER_DOCUMENTS, temp_dir_.GetPath(), true, true);
  EXPECT_EQ(permission_context()->GetWellKnownDirectoryPath(
                blink::mojom::WellKnownDirectory::kDefault, kTestOrigin),
            temp_dir_.GetPath());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWellKnownDirectoryPath_Pdf_Downloads) {
  DownloadPrefs::FromBrowserContext(browser_context())
      ->SkipSanitizeDownloadTargetPathForTesting();
  DownloadPrefs::FromBrowserContext(browser_context())
      ->SetDownloadPath(temp_dir_.GetPath());
  EXPECT_EQ(permission_context()->GetWellKnownDirectoryPath(
                blink::mojom::WellKnownDirectory::kDirDownloads, kPdfOrigin),
            temp_dir_.GetPath());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetReadPermissionGrant_InitialState_LoadFromStorage) {
  auto grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kLoadFromStorage);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kRead));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetReadPermissionGrant_InitialState_Open_File) {
  auto grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kRead));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetReadPermissionGrant_InitialState_Open_Directory) {
  auto grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kRead));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_InitialState_LoadFromStorage) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kLoadFromStorage);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_InitialState_Open_File) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_InitialState_Open_Directory) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_InitialState_WritableImplicitState) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  // The existing grant should not change if the permission is blocked globally,
  // and the persisted permission should not be accessible.
  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
  // The persistent permission is inaccessible because of the BLOCK, but will
  // still exist until it expires.
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  // Getting a grant for the same file again should also not change the grant,
  // even now asking for more permissions is blocked globally.
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_WriteGrantedChangesExistingGrant) {
  auto grant1 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  auto grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  auto grant3 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  // All grants should be the same grant, and be granted and persisted.
  EXPECT_EQ(grant1, grant2);
  EXPECT_EQ(grant1, grant3);
  EXPECT_EQ(PermissionStatus::GRANTED, grant1->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(
    ChromeFileSystemAccessPermissionContextNoPersistenceTest,
    GetWritePermissionGrant_GrantIsRevokedWhenNoLongerUsed_NoPersistentPermissions) {
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
       GetWritePermissionGrant_GrantIsAutoGrantedViaPersistentPermissions) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
  // A valid persisted permission should be created.
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  grant.reset();

  // Permission should not be granted for |kOpen|.
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());

  // Permission should be auto-granted here via the persisted permission.
  base::test::TestFuture<PermissionRequestOutcome> future;
  grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                           future.GetCallback());
  EXPECT_EQ(PermissionRequestOutcome::kGrantedByPersistentPermission,
            future.Get());
  ExpectUmaEntryPersistedPermissionAge(base::Seconds(0), 1);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetPermissionGrants_GrantsAreRetainedViaPersistedPermissions) {
  auto kTestPath2 = kTestPath.AppendASCII("baz");
  auto file_write_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  auto file_read_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  auto file_read_only_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath2, HandleType::kFile, UserAction::kSave);

  // `GetPermissionGrants` returns `grants` as expected.
  auto grants = permission_context()->GetPermissionGrants(kTestOrigin);
  std::vector<base::FilePath> expected_file_write_grants = {kTestPath};
  std::vector<base::FilePath> expected_file_read_grants = {kTestPath,
                                                           kTestPath2};

  EXPECT_EQ(grants.file_write_grants, expected_file_write_grants);
  EXPECT_EQ(grants.file_read_grants, expected_file_read_grants);

  // Persisted permissions are retained after resetting the active grants.
  file_write_grant.reset();
  file_read_grant.reset();
  file_read_only_grant.reset();
  grants = permission_context()->GetPermissionGrants(kTestOrigin);
  EXPECT_EQ(grants.file_write_grants, expected_file_write_grants);
  EXPECT_EQ(grants.file_read_grants, expected_file_read_grants);
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
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
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

  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  SetContentSettingValueForOrigin(kTestOrigin,
                                  ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                  CONTENT_SETTING_ASK);

  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
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
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
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
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(
    ChromeFileSystemAccessPermissionContextNoPersistenceTest,
    GetWritePermissionGrant_GrantIsRevokedWhenNoLongerUsed_GlobalGuardBlockedAfterNewGrant_NoPersistentPermissions) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
  // Revoke active and persisted permissions.
  permission_context()->RevokeGrants(
      kTestOrigin, PersistedPermissionOptions::kUpdatePersistedPermission);
  grant.reset();
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  // After reset grant should go away, but the new grant request should be in
  // ASK state.
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());

  auto grants = permission_context()->GetPermissionGrants(kTestOrigin);
  EXPECT_TRUE(grants.file_write_grants.empty());

  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);

  // After the guard is blocked, the permission status for |grant| should remain
  // unchanged.
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(
    ChromeFileSystemAccessPermissionContextTest,
    GetWritePermissionGrant_GrantIsRevokedWhenNoLongerUsed_GlobalGuardBlockedAfterNewGrant_HasPersistentPermissions) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
  // Revoke active and persisted permissions.
  permission_context()->RevokeGrants(
      kTestOrigin, PersistedPermissionOptions::kUpdatePersistedPermission);
  grant.reset();
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

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
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetReadPermissionGrant_InheritFromAncestor) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  auto dir_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, dir_grant->GetStatus());
  base::test::TestFuture<PermissionRequestOutcome> future;
  dir_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                               future.GetCallback());
  EXPECT_EQ(PermissionRequestOutcome::kUserGranted, future.Get());
  EXPECT_EQ(PermissionStatus::GRANTED, dir_grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kRead));

  // A file in |dir_path|'s directory should be auto-granted permissions.
  auto file_path = kTestPath.AppendASCII("baz");
  auto file_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, file_path, HandleType::kFile, UserAction::kLoadFromStorage);
  EXPECT_EQ(PermissionStatus::GRANTED, file_grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, file_path, HandleType::kFile, GrantType::kRead));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_InheritFromAncestor) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  auto dir_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, dir_grant->GetStatus());
  base::test::TestFuture<PermissionRequestOutcome> future;
  dir_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                               future.GetCallback());
  EXPECT_EQ(PermissionRequestOutcome::kUserGranted, future.Get());
  EXPECT_EQ(PermissionStatus::GRANTED, dir_grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kWrite));

  // A file in |dir_path|'s directory should be auto-granted permissions.
  auto file_path = kTestPath.AppendASCII("baz");
  auto file_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, file_path, HandleType::kFile, UserAction::kLoadFromStorage);
  EXPECT_EQ(PermissionStatus::GRANTED, file_grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, file_path, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       DoNotInheritFromAncestorOfOppositeType) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  auto dir_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, dir_grant->GetStatus());
  base::test::TestFuture<PermissionRequestOutcome> future;
  dir_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                               future.GetCallback());
  EXPECT_EQ(PermissionRequestOutcome::kUserGranted, future.Get());
  EXPECT_EQ(PermissionStatus::GRANTED, dir_grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kRead));

  // |dir_path| has read permission while we're asking for write permission, so
  // do not auto-grant the permission.
  auto file_path = kTestPath.AppendASCII("baz");
  auto file_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, file_path, HandleType::kFile, UserAction::kLoadFromStorage);
  EXPECT_EQ(PermissionStatus::ASK, file_grant->GetStatus());
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, file_path, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetReadPermissionGrant_InheritFromPersistedAncestor) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  auto dir_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, dir_grant->GetStatus());
  base::test::TestFuture<PermissionRequestOutcome> future;
  dir_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                               future.GetCallback());
  EXPECT_EQ(PermissionRequestOutcome::kUserGranted, future.Get());
  EXPECT_EQ(PermissionStatus::GRANTED, dir_grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kRead));

  // Remove the active grant, but not the persisted permission.
  dir_grant.reset();

  // A file in |dir_path|'s directory should not be granted permission until
  // permission is explicitly requested.
  auto file_path = kTestPath.AppendASCII("baz");
  auto file_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, file_path, HandleType::kFile, UserAction::kLoadFromStorage);
  EXPECT_EQ(PermissionStatus::ASK, file_grant->GetStatus());
  base::test::TestFuture<PermissionRequestOutcome> future2;
  file_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                                future2.GetCallback());
  EXPECT_EQ(PermissionRequestOutcome::kGrantedByAncestorPersistentPermission,
            future2.Get());
  // Age should not be recorded if granted via an ancestor's permission.
  ExpectUmaEntryPersistedPermissionAge(base::Seconds(0), 0);
  EXPECT_EQ(PermissionStatus::GRANTED, file_grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, file_path, HandleType::kFile, GrantType::kRead));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_InheritFromPersistedAncestor) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  auto dir_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, dir_grant->GetStatus());
  base::test::TestFuture<PermissionRequestOutcome> future;
  dir_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                               future.GetCallback());
  EXPECT_EQ(PermissionRequestOutcome::kUserGranted, future.Get());
  EXPECT_EQ(PermissionStatus::GRANTED, dir_grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kWrite));

  // Remove the active grant, but not the persisted permission.
  dir_grant.reset();

  // A file in |dir_path|'s directory should not be granted permission until
  // permission is explicitly requested.
  auto file_path = kTestPath.AppendASCII("baz");
  auto file_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, file_path, HandleType::kFile, UserAction::kLoadFromStorage);
  EXPECT_EQ(PermissionStatus::ASK, file_grant->GetStatus());
  base::test::TestFuture<PermissionRequestOutcome> future2;
  file_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                                future2.GetCallback());
  EXPECT_EQ(PermissionRequestOutcome::kGrantedByAncestorPersistentPermission,
            future2.Get());
  // Age should not be recorded if granted via an ancestor's permission.
  ExpectUmaEntryPersistedPermissionAge(base::Seconds(0), 0);
  EXPECT_EQ(PermissionStatus::GRANTED, file_grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, file_path, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       DoNotInheritFromPersistedAncestorOfOppositeType) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  auto dir_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, dir_grant->GetStatus());
  base::test::TestFuture<PermissionRequestOutcome> future;
  dir_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                               future.GetCallback());
  EXPECT_EQ(PermissionRequestOutcome::kUserGranted, future.Get());
  EXPECT_EQ(PermissionStatus::GRANTED, dir_grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kRead));

  // Remove the active grant, but not the persisted permission.
  dir_grant.reset();

  // |dir_path| has read permission while we're asking for write permission, so
  // do not auto-grant the permission and do not grant via persisted permission.
  auto file_path = kTestPath.AppendASCII("baz");
  auto file_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, file_path, HandleType::kFile, UserAction::kLoadFromStorage);
  EXPECT_EQ(PermissionStatus::ASK, file_grant->GetStatus());
  base::test::TestFuture<PermissionRequestOutcome> future2;
  file_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                                future2.GetCallback());
  EXPECT_EQ(PermissionRequestOutcome::kUserGranted, future2.Get());
  EXPECT_EQ(PermissionStatus::GRANTED, file_grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, file_path, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       PersistedPermission_GrantExpired) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  grant.reset();

  // Advance the clock far enough that all permissions should be expired.
  Advance(ChromeFileSystemAccessPermissionContext::
              kPersistentPermissionExpirationTimeoutDefault +
          base::Minutes(1));

  // Permission should not be granted for |kOpen|.
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());

  // Once a permission grant has expired, it should not auto-grant
  base::test::TestFuture<PermissionRequestOutcome> future;
  grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                           future.GetCallback());
  EXPECT_EQ(PermissionRequestOutcome::kUserDismissed, future.Get());
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       PersistedPermission_RevokeOnlyActiveGrants) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
  // Revoke active grant, but not persisted permission.
  permission_context()->RevokeGrants(
      kTestOrigin, PersistedPermissionOptions::kDoNotUpdatePersistedPermission);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  ChromeFileSystemAccessPermissionContext::Grants grants =
      permission_context()->GetPermissionGrants(kTestOrigin);
  std::vector<base::FilePath> expected_res = {kTestPath};
  EXPECT_EQ(grants.file_write_grants, expected_res);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       PersistedPermission_RevokeGrantByFilePath) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
  // Revoke active grant by file path, but not persisted permission.
  permission_context()->RevokeGrant(
      kTestOrigin, kTestPath,
      PersistedPermissionOptions::kDoNotUpdatePersistedPermission);
  auto updated_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kNone);
  EXPECT_EQ(PermissionStatus::ASK, updated_grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  auto kTestPath2 = kTestPath.AppendASCII("foo");
  auto grant2 = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath2, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, grant2->GetStatus());
  // Revoke active grant by file path, and reset persisted permission.
  permission_context()->RevokeGrant(
      kTestOrigin, kTestPath2,
      PersistedPermissionOptions::kUpdatePersistedPermission);
  auto updated_grant2 = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath2, HandleType::kFile, UserAction::kNone);
  EXPECT_EQ(PermissionStatus::ASK, updated_grant2->GetStatus());
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath2, HandleType::kFile, GrantType::kRead));
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath2, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       PersistedPermission_NotAccessibleIfContentSettingBlock) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
  grant.reset();
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  // After reset grant should go away, but the new grant request should be in
  // ASK state.
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());

  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);

  // After the guard is blocked, the permission status for |grant| should remain
  // unchanged, but the persisted permission should not be accessible.
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       PersistedPermission_UpdatePermissions_Write) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  Advance(ChromeFileSystemAccessPermissionContext::
              kPersistentPermissionExpirationTimeoutDefault +
          base::Seconds(1));
  auto advance_once = Now();
  // The active grant exists, so its timestamp should have been updated.
  permission_context()->UpdatePersistedPermissionsForTesting();
  auto objects = permission_context()->GetAllGrantedOrExpiredObjects();
  ASSERT_EQ(objects.size(), 1u);
  EXPECT_EQ(objects[0]->origin, kTestOrigin.GetURL());
  EXPECT_EQ(base::ValueToTime(objects[0]->value.Find("time")), advance_once);

  grant.reset();

  // Do not advance far enough to expire the persisted permission. The timestamp
  // should NOT have been updated, since the active permission no longer exists.
  Advance(ChromeFileSystemAccessPermissionContext::
              kPersistentPermissionExpirationTimeoutDefault -
          base::Seconds(1));
  permission_context()->UpdatePersistedPermissionsForTesting();
  objects = permission_context()->GetAllGrantedOrExpiredObjects();
  ASSERT_EQ(objects.size(), 1u);
  EXPECT_EQ(objects[0]->origin, kTestOrigin.GetURL());
  EXPECT_EQ(base::ValueToTime(objects[0]->value.Find("time")), advance_once);

  // |grant| should now be expired, but not revokable until after grace period.
  Advance(ChromeFileSystemAccessPermissionContext::
              kPersistentPermissionExpirationTimeoutDefault +
          base::Seconds(1));
  permission_context()->UpdatePersistedPermissionsForTesting();
  objects = permission_context()->GetAllGrantedOrExpiredObjects();
  EXPECT_EQ(objects.size(), 1u);

  // Get(All)GrantedObjects should not list permissions which are expired,
  // including those in the grace period.
  objects = permission_context()->GetAllGrantedObjects();
  EXPECT_EQ(objects.size(), 0u);
  objects = permission_context()->GetGrantedObjects(kTestOrigin);
  EXPECT_EQ(objects.size(), 0u);

  Advance(ChromeFileSystemAccessPermissionContext::
              kPersistentPermissionGracePeriod +
          base::Seconds(1));
  permission_context()->UpdatePersistedPermissionsForTesting();
  objects = permission_context()->GetAllGrantedOrExpiredObjects();
  EXPECT_EQ(objects.size(), 0u);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       PersistedPermission_UpdatePermissions_Read) {
  auto grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kRead));

  Advance(ChromeFileSystemAccessPermissionContext::
              kPersistentPermissionExpirationTimeoutDefault +
          base::Seconds(1));
  auto advance_once = Now();
  // The active grant exists, so its timestamp should have been updated.
  permission_context()->UpdatePersistedPermissionsForTesting();
  auto objects = permission_context()->GetAllGrantedOrExpiredObjects();
  ASSERT_EQ(objects.size(), 1u);
  EXPECT_EQ(objects[0]->origin, kTestOrigin.GetURL());
  EXPECT_EQ(base::ValueToTime(objects[0]->value.Find("time")), advance_once);

  grant.reset();

  // Do not advance far enough to expire the persisted permission. The timestamp
  // should NOT have been updated, since the active permission no longer exists.
  Advance(ChromeFileSystemAccessPermissionContext::
              kPersistentPermissionExpirationTimeoutDefault -
          base::Seconds(1));
  permission_context()->UpdatePersistedPermissionsForTesting();
  objects = permission_context()->GetAllGrantedOrExpiredObjects();
  ASSERT_EQ(objects.size(), 1u);
  EXPECT_EQ(objects[0]->origin, kTestOrigin.GetURL());
  EXPECT_EQ(base::ValueToTime(objects[0]->value.Find("time")), advance_once);

  // |grant| should now be expired, but not revokable until after grace period.
  Advance(ChromeFileSystemAccessPermissionContext::
              kPersistentPermissionExpirationTimeoutDefault +
          base::Seconds(1));
  permission_context()->UpdatePersistedPermissionsForTesting();
  objects = permission_context()->GetAllGrantedOrExpiredObjects();
  EXPECT_EQ(objects.size(), 1u);

  // Get(All)GrantedObjects should not list permissions which are expired,
  // including those in the grace period.
  objects = permission_context()->GetAllGrantedObjects();
  EXPECT_EQ(objects.size(), 0u);
  objects = permission_context()->GetGrantedObjects(kTestOrigin);
  EXPECT_EQ(objects.size(), 0u);

  Advance(ChromeFileSystemAccessPermissionContext::
              kPersistentPermissionGracePeriod +
          base::Seconds(1));
  permission_context()->UpdatePersistedPermissionsForTesting();
  objects = permission_context()->GetAllGrantedOrExpiredObjects();
  EXPECT_EQ(objects.size(), 0u);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       PersistedPermission_UpdateWhenRequested) {
  auto initial_time = Now();

  auto grant1 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, grant1->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  auto grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin2, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, grant2->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin2, kTestPath, HandleType::kFile, GrantType::kWrite));

  grant1.reset();
  grant2.reset();

  Advance(ChromeFileSystemAccessPermissionContext::
              kPersistentPermissionExpirationTimeoutDefault -
          base::Seconds(1));

  // Both grants are still valid.
  permission_context()->UpdatePersistedPermissionsForTesting();
  auto objects = permission_context()->GetAllGrantedObjects();
  ASSERT_EQ(objects.size(), 2u);

  // Requesting permission for |grant2| should update its timestamp.
  grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin2, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, grant2->GetStatus());
  base::test::TestFuture<PermissionRequestOutcome> future;
  grant2->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                            future.GetCallback());
  EXPECT_EQ(PermissionRequestOutcome::kGrantedByPersistentPermission,
            future.Get());
  ExpectUmaEntryPersistedPermissionAge(Now() - initial_time, 1);
  EXPECT_EQ(PermissionStatus::GRANTED, grant2->GetStatus());

  // |grant1| should now be expired, but not revoked.
  Advance(base::Seconds(2));
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
  objects = permission_context()->GetAllGrantedOrExpiredObjects();
  EXPECT_EQ(objects.size(), 2u);

  // Get(All)GrantedObjects only list permissions which are not expired.
  objects = permission_context()->GetAllGrantedObjects();
  EXPECT_EQ(objects.size(), 1u);
  objects = permission_context()->GetGrantedObjects(kTestOrigin);
  EXPECT_EQ(objects.size(), 0u);
  objects = permission_context()->GetGrantedObjects(kTestOrigin2);
  EXPECT_EQ(objects.size(), 1u);

  // |grant1| should not be revoked until after the grace period.
  Advance(ChromeFileSystemAccessPermissionContext::
              kPersistentPermissionGracePeriod);

  // Clean up |grant1|'s expired entry from HostContentSettingsMap.
  permission_context()->UpdatePersistedPermissionsForTesting();

  // Only |grant2| should be persisted.
  objects = permission_context()->GetAllGrantedOrExpiredObjects();
  ASSERT_EQ(objects.size(), 1u);
  EXPECT_EQ(objects[0]->origin, kTestOrigin2.GetURL());
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin2, kTestPath, HandleType::kFile, GrantType::kWrite));
  objects = permission_context()->GetAllGrantedObjects();
  EXPECT_EQ(objects.size(), 1u);
  objects = permission_context()->GetGrantedObjects(kTestOrigin);
  EXPECT_EQ(objects.size(), 0u);
  objects = permission_context()->GetGrantedObjects(kTestOrigin2);
  EXPECT_EQ(objects.size(), 1u);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       PersistedPermission_RenewWhenActivePermissionExists) {
  auto initial_time = Now();

  auto grant1 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, grant1->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  auto grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin2, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, grant2->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin2, kTestPath, HandleType::kFile, GrantType::kWrite));

  Advance(ChromeFileSystemAccessPermissionContext::
              kPersistentPermissionExpirationTimeoutDefault -
          base::Seconds(1));

  // Auto-grant because active permissions exist. This should update the
  // timestamp of the persisted permission for |grant2|.
  base::test::TestFuture<PermissionRequestOutcome> future;
  grant2->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                            future.GetCallback());
  EXPECT_EQ(PermissionRequestOutcome::kRequestAborted, future.Get());

  // Only |grant2|'s timestamp should have been updated.
  auto objects = permission_context()->GetAllGrantedOrExpiredObjects();
  ASSERT_EQ(objects.size(), 2u);
  EXPECT_EQ(objects[0]->origin, kTestOrigin.GetURL());
  EXPECT_EQ(objects[1]->origin, kTestOrigin2.GetURL());
  EXPECT_EQ(base::ValueToTime(objects[0]->value.Find("time")), initial_time);
  EXPECT_EQ(base::ValueToTime(objects[1]->value.Find("time")), Now());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       PersistedPermission_SweepOnRestart) {
  auto initial_time = Now();

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  grant.reset();

  // Permissions should still be valid.
  Advance(ChromeFileSystemAccessPermissionContext::
              kPersistentPermissionExpirationTimeoutDefault -
          base::Minutes(1));

  // Resetting the permission context should kick off a sweep.
  permission_context_ = std::make_unique<TestFileSystemAccessPermissionContext>(
      browser_context(), task_environment_.GetMockClock());
  task_environment_.RunUntilIdle();

  // The timestamp should not have been updated.
  auto objects = permission_context()->GetAllGrantedOrExpiredObjects();
  ASSERT_EQ(objects.size(), 1u);
  EXPECT_EQ(objects[0]->origin, kTestOrigin.GetURL());
  EXPECT_EQ(base::ValueToTime(objects[0]->value.Find("time")), initial_time);

  // Permissions should now be expired and can be revoked.
  Advance(ChromeFileSystemAccessPermissionContext::
              kPersistentPermissionExpirationTimeoutDefault +
          ChromeFileSystemAccessPermissionContext::
              kPersistentPermissionGracePeriod +
          base::Minutes(1));

  // Resetting the permission context should kick off a sweep.
  permission_context_ = std::make_unique<TestFileSystemAccessPermissionContext>(
      browser_context(), task_environment_.GetMockClock());
  task_environment_.RunUntilIdle();

  // The timestamp should not have been updated.
  objects = permission_context()->GetAllGrantedOrExpiredObjects();
  ASSERT_EQ(objects.size(), 0u);

  // The two sweeps should have been recorded in UMA.
  auto* uma_prefix = "Storage.FileSystemAccess.PersistedPermissions.";
  base::HistogramTester::CountsMap expected_counts;
  expected_counts[base::StrCat({uma_prefix, "SweepTime.All"})] = 2;
  expected_counts[base::StrCat({uma_prefix, "Count"})] = 2;
  EXPECT_THAT(histogram_tester_.GetTotalCountsForPrefix(uma_prefix),
              testing::ContainerEq(expected_counts));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       PersistedPermission_SharedFateReadAndWrite) {
  auto read_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, read_grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kRead));

  auto write_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, write_grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  read_grant.reset();

  Advance(ChromeFileSystemAccessPermissionContext::
              kPersistentPermissionExpirationTimeoutDefault -
          base::Seconds(1));

  // Auto-grant because active permissions exist. This should update the
  // timestamp of the persisted permission for |write_grant|.
  base::test::TestFuture<PermissionRequestOutcome> future;
  write_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                                 future.GetCallback());
  EXPECT_EQ(PermissionRequestOutcome::kRequestAborted, future.Get());

  // Though only |write_grant| was accessed, we should not lose read access.
  Advance(ChromeFileSystemAccessPermissionContext::
              kPersistentPermissionExpirationTimeoutDefault -
          base::Seconds(1));
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kRead));
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RequestPermission_Dismissed) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::DISMISSED);
  content::RenderFrameHostTester::For(web_contents_->GetPrimaryMainFrame())
      ->SimulateUserActivation();

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);

  base::test::TestFuture<PermissionRequestOutcome> future;
  grant->RequestPermission(frame_id(), UserActivationState::kRequired,
                           future.GetCallback());
  EXPECT_EQ(PermissionRequestOutcome::kUserDismissed, future.Get());
  // Dismissed, so status should not change.
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, RequestPermission_Granted) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);
  content::RenderFrameHostTester::For(web_contents_->GetPrimaryMainFrame())
      ->SimulateUserActivation();

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);

  base::test::TestFuture<PermissionRequestOutcome> future;
  grant->RequestPermission(frame_id(), UserActivationState::kRequired,
                           future.GetCallback());
  EXPECT_EQ(PermissionRequestOutcome::kUserGranted, future.Get());
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, RequestPermission_Denied) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::DENIED);
  content::RenderFrameHostTester::For(web_contents_->GetPrimaryMainFrame())
      ->SimulateUserActivation();

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);

  base::test::TestFuture<PermissionRequestOutcome> future;
  grant->RequestPermission(frame_id(), UserActivationState::kRequired,
                           future.GetCallback());
  EXPECT_EQ(PermissionRequestOutcome::kUserDenied, future.Get());
  EXPECT_EQ(PermissionStatus::DENIED, grant->GetStatus());
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RequestPermission_NoUserActivation) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);

  base::test::TestFuture<PermissionRequestOutcome> future;
  grant->RequestPermission(frame_id(), UserActivationState::kRequired,
                           future.GetCallback());
  EXPECT_EQ(PermissionRequestOutcome::kNoUserActivation, future.Get());
  // No user activation, so status should not change.
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RequestPermission_NoUserActivation_UserActivationNotRequired) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);

  base::test::TestFuture<PermissionRequestOutcome> future;
  grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                           future.GetCallback());
  EXPECT_EQ(PermissionRequestOutcome::kUserGranted, future.Get());
  // No user activation, so status should not change.
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RequestPermission_AlreadyGranted) {
  // If the permission has already been granted, a call to RequestPermission()
  // should call the passed-in callback and return immediately without showing a
  // prompt.
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);

  base::test::TestFuture<PermissionRequestOutcome> future;
  grant->RequestPermission(frame_id(), UserActivationState::kRequired,
                           future.GetCallback());
  EXPECT_EQ(PermissionRequestOutcome::kRequestAborted, future.Get());
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
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

  base::test::TestFuture<PermissionRequestOutcome> future1;
  grant->RequestPermission(frame_id(), UserActivationState::kRequired,
                           future1.GetCallback());
  EXPECT_EQ(PermissionRequestOutcome::kRequestAborted, future1.Get());
  EXPECT_EQ(PermissionStatus::DENIED, grant->GetStatus());
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  auto grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin2, kTestPath, HandleType::kFile, UserAction::kOpen);

  base::test::TestFuture<PermissionRequestOutcome> future2;
  grant2->RequestPermission(frame_id(), UserActivationState::kRequired,
                            future2.GetCallback());
  EXPECT_EQ(PermissionRequestOutcome::kRequestAborted, future2.Get());
  EXPECT_EQ(PermissionStatus::DENIED, grant2->GetStatus());
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin2, kTestPath, HandleType::kFile, GrantType::kWrite));

  grant2.reset();
  SetContentSettingValueForOrigin(kTestOrigin2,
                                  ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                  CONTENT_SETTING_ASK);

  grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin2, kTestPath, HandleType::kFile, UserAction::kOpen);

  base::test::TestFuture<PermissionRequestOutcome> future3;
  grant2->RequestPermission(frame_id(), UserActivationState::kRequired,
                            future3.GetCallback());
  EXPECT_EQ(PermissionRequestOutcome::kNoUserActivation, future3.Get());
  EXPECT_EQ(PermissionStatus::ASK, grant2->GetStatus());
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin2, kTestPath, HandleType::kFile, GrantType::kWrite));
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

  base::test::TestFuture<PermissionRequestOutcome> future1;
  grant->RequestPermission(frame_id(), UserActivationState::kRequired,
                           future1.GetCallback());
  EXPECT_EQ(PermissionRequestOutcome::kBlockedByContentSetting, future1.Get());
  EXPECT_EQ(PermissionStatus::DENIED, grant->GetStatus());
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  base::test::TestFuture<PermissionRequestOutcome> future2;
  grant2->RequestPermission(frame_id(), UserActivationState::kRequired,
                            future2.GetCallback());
  EXPECT_EQ(PermissionRequestOutcome::kBlockedByContentSetting, future2.Get());
  EXPECT_EQ(PermissionStatus::DENIED, grant2->GetStatus());
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin2, kTestPath, HandleType::kFile, GrantType::kWrite));

  grant.reset();
  grant2.reset();

  SetContentSettingValueForOrigin(kTestOrigin,
                                  ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                  CONTENT_SETTING_ASK);
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin2, kTestPath, HandleType::kFile, UserAction::kOpen);

  base::test::TestFuture<PermissionRequestOutcome> future3;
  grant->RequestPermission(frame_id(), UserActivationState::kRequired,
                           future3.GetCallback());
  EXPECT_EQ(PermissionRequestOutcome::kNoUserActivation, future3.Get());
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  base::test::TestFuture<PermissionRequestOutcome> future4;
  grant2->RequestPermission(frame_id(), UserActivationState::kRequired,
                            future4.GetCallback());
  EXPECT_EQ(PermissionRequestOutcome::kRequestAborted, future4.Get());
  EXPECT_EQ(PermissionStatus::DENIED, grant2->GetStatus());
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin2, kTestPath, HandleType::kFile, GrantType::kWrite));
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
  // Permissions are not persisted for allowlisted origins.
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kChromeOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  auto grant2 = permission_context()->GetWritePermissionGrant(
      kChromeOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::GRANTED, grant2->GetStatus());
  // Permissions are not persisted for allowlisted origins.
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kChromeOrigin, kTestPath, HandleType::kDirectory, GrantType::kWrite));

  // Other origin should gets blocked.
  auto grant3 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::DENIED, grant3->GetStatus());
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  auto grant4 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::DENIED, grant4->GetStatus());
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kWrite));
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
  // Permissions are not persisted for allowlisted origins.
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kChromeOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  // Existing grant (file).
  auto grant2 = permission_context()->GetWritePermissionGrant(
      kChromeOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::GRANTED, grant2->GetStatus());

  // Initial grant (directory).
  auto grant3 = permission_context()->GetWritePermissionGrant(
      kChromeOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::GRANTED, grant3->GetStatus());
  // Permissions are not persisted for allowlisted origins.
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kChromeOrigin, kTestPath, HandleType::kDirectory, GrantType::kWrite));

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
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kRead));

  auto directory_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, directory_grant->GetStatus());
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kRead));

  // Requesting a permission grant for a directory which was previously a file
  // should have revoked the original file permission.
  EXPECT_EQ(PermissionStatus::DENIED, file_grant->GetStatus());
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kRead));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_FileBecomesDirectory) {
  auto file_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, file_grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  auto directory_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, directory_grant->GetStatus());
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kWrite));

  // Requesting a permission grant for a directory which was previously a file
  // should have revoked the original file permission.
  EXPECT_EQ(PermissionStatus::DENIED, file_grant->GetStatus());
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, NotifyEntryMoved_File) {
  auto file_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, file_grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  const auto new_path = kTestPath.DirName().AppendASCII("new_name.txt");
  permission_context()->NotifyEntryMoved(kTestOrigin, kTestPath, new_path);

  // Permissions to the old path should have been revoked.
  auto file_grant_at_old_path = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, file_grant_at_old_path->GetStatus());
  EXPECT_FALSE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  // Permissions to the new path should have been updated.
  EXPECT_EQ(PermissionStatus::GRANTED, file_grant->GetStatus());
  EXPECT_EQ(new_path, file_grant->GetPath());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, new_path, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       NotifyEntryMoved_ChildFileObtainedLater) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  auto parent_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  base::test::TestFuture<PermissionRequestOutcome> future;
  parent_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                                  future.GetCallback());
  EXPECT_EQ(PermissionRequestOutcome::kUserGranted, future.Get());
  EXPECT_EQ(PermissionStatus::GRANTED, parent_grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kWrite));

  // The child file should inherit write permission from its parent.
  const auto old_file_path = kTestPath.AppendASCII("old_name.txt");
  auto file_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, old_file_path, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::GRANTED, file_grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, old_file_path, HandleType::kFile, GrantType::kWrite));

  const auto new_path = old_file_path.DirName().AppendASCII("new_name.txt");
  permission_context()->NotifyEntryMoved(kTestOrigin, old_file_path, new_path);

  // Permissions to the parent should not have been affected.
  auto parent_grant_copy = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::GRANTED, parent_grant_copy->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kWrite));

  // Permissions to the old file path should not have been affected.
  auto file_grant_at_old_path = permission_context()->GetWritePermissionGrant(
      kTestOrigin, old_file_path, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::GRANTED, file_grant_at_old_path->GetStatus());
  EXPECT_EQ(old_file_path, file_grant_at_old_path->GetPath());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, old_file_path, HandleType::kFile, GrantType::kWrite));

  // Should still have permission at the new path.
  EXPECT_EQ(PermissionStatus::GRANTED, file_grant->GetStatus());
  EXPECT_EQ(new_path, file_grant->GetPath());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, new_path, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       NotifyEntryMoved_ChildFileObtainedFirst) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  // Acquire permission to the child file's path.
  const auto old_file_path = kTestPath.AppendASCII("old_name.txt");
  auto file_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, old_file_path, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, file_grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, old_file_path, HandleType::kFile, GrantType::kWrite));

  // Later, acquire permission to the child parent.
  auto parent_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  base::test::TestFuture<PermissionRequestOutcome> future;
  parent_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                                  future.GetCallback());
  EXPECT_EQ(PermissionRequestOutcome::kUserGranted, future.Get());
  EXPECT_EQ(PermissionStatus::GRANTED, parent_grant->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kWrite));

  const auto new_path = old_file_path.DirName().AppendASCII("new_name.txt");
  permission_context()->NotifyEntryMoved(kTestOrigin, old_file_path, new_path);

  // Permissions to the parent should not have been affected.
  auto parent_grant_copy = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::GRANTED, parent_grant_copy->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kWrite));

  // Permissions to the old file path should not have been affected.
  auto file_grant_at_old_path = permission_context()->GetWritePermissionGrant(
      kTestOrigin, old_file_path, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::GRANTED, file_grant_at_old_path->GetStatus());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, old_file_path, HandleType::kFile, GrantType::kWrite));

  // Should still have permission at the new path.
  EXPECT_EQ(PermissionStatus::GRANTED, file_grant->GetStatus());
  EXPECT_EQ(new_path, file_grant->GetPath());
  EXPECT_TRUE(permission_context()->HasPersistedPermissionForTesting(
      kTestOrigin, new_path, HandleType::kFile, GrantType::kWrite));
}

#endif  // !BUILDFLAG(IS_ANDROID)
