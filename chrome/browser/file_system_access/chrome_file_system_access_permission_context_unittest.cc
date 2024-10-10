// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"

#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
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
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/file_system_access/file_system_access_permission_request_manager.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_uma_util.h"
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

#if BUILDFLAG(IS_ANDROID)
#include "base/android/path_utils.h"
#else
#include "chrome/browser/permissions/one_time_permissions_tracker_observer.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#endif

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_delegate.h"
#include "chrome/browser/policy/dm_token_utils.h"
#endif

using content::BrowserContext;
using content::PathInfo;
using content::PathType;
using content::WebContents;
using content::WebContentsTester;
using permissions::PermissionAction;
using PersistedGrantStatus =
    ChromeFileSystemAccessPermissionContext::PersistedGrantStatus;
using GrantType = ChromeFileSystemAccessPermissionContext::GrantType;
using HandleType = ChromeFileSystemAccessPermissionContext::HandleType;
using PersistedGrantType =
    ChromeFileSystemAccessPermissionContext::PersistedGrantType;
using UserAction = ChromeFileSystemAccessPermissionContext::UserAction;
using PermissionRequestOutcome =
    content::FileSystemAccessPermissionGrant::PermissionRequestOutcome;
using PermissionStatus =
    content::FileSystemAccessPermissionGrant::PermissionStatus;
using RestorePermissionPromptOutcome =
    ChromeFileSystemAccessPermissionContext::RestorePermissionPromptOutcome;
using SensitiveDirectoryResult =
    ChromeFileSystemAccessPermissionContext::SensitiveEntryResult;
using UserActivationState =
    content::FileSystemAccessPermissionGrant::UserActivationState;

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
using ContentAnalysisDelegate = enterprise_connectors::ContentAnalysisDelegate;
using FakeContentAnalysisDelegate =
    enterprise_connectors::test::FakeContentAnalysisDelegate;
using ContentAnalysisResponse = enterprise_connectors::ContentAnalysisResponse;
#endif

namespace {

enum class CreateSymbolicLinkResult {
  // The symbolic link creation failed because the platform does not support it.
  // On Windows, that may be due to the lack of the required privilege.
  kUnsupported = -1,

  // The symbolic link creation failed.
  kFailed,

  // The symbolic link was created successfully.
  kSucceeded,
};

// Observes `grant`'s permission status and destroys itself when it changes.
class SelfDestructingPermissionGrantObserver
    : content::FileSystemAccessPermissionGrant::Observer {
 public:
  static base::WeakPtr<SelfDestructingPermissionGrantObserver> Create(
      scoped_refptr<content::FileSystemAccessPermissionGrant> grant) {
    SelfDestructingPermissionGrantObserver* observer =
        new SelfDestructingPermissionGrantObserver(std::move(grant));
    return observer->weak_factory_.GetWeakPtr();
  }

  ~SelfDestructingPermissionGrantObserver() override {
    grant_->RemoveObserver(this);
  }

  scoped_refptr<content::FileSystemAccessPermissionGrant>& grant() {
    return grant_;
  }

 private:
  explicit SelfDestructingPermissionGrantObserver(
      scoped_refptr<content::FileSystemAccessPermissionGrant> grant)
      : grant_(std::move(grant)) {
    grant_->AddObserver(this);
  }

  // FileSystemAccessPermissionGrant::Observer override.
  void OnPermissionStatusChanged() override { self.reset(); }

  std::unique_ptr<SelfDestructingPermissionGrantObserver> self{this};
  scoped_refptr<content::FileSystemAccessPermissionGrant> grant_;

  base::WeakPtrFactory<SelfDestructingPermissionGrantObserver> weak_factory_{
      this};
};

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
constexpr char kDummyDmToken[] = "dm_token";

void EnableEnterpriseAnalysis(Profile* profile) {
  static constexpr char kEnabled[] = R"(
    {
        "service_provider": "google",
        "enable": [
          {
            "url_list": ["*"],
            "tags": ["dlp"]
          }
        ],
        "block_until_verdict": 1
    })";
  enterprise_connectors::test::SetAnalysisConnector(
      profile->GetPrefs(), enterprise_connectors::FILE_ATTACHED, kEnabled);
  enterprise_connectors::ContentAnalysisDelegate::DisableUIForTesting();
  policy::SetDMTokenForTesting(
      policy::DMToken::CreateValidToken(kDummyDmToken));
}

bool CreateNonEmptyFile(const base::FilePath& path) {
  return base::WriteFile(path, "data");
}
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)

#if BUILDFLAG(IS_WIN)
CreateSymbolicLinkResult CreateWinSymbolicLink(const base::FilePath& target,
                                               const base::FilePath& symlink,
                                               bool is_directory) {
  // Creating symbolic links on Windows requires Administrator privileges.
  // However, recent versions of Windows introduced the
  // SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE flag, which allows the
  // creation of symbolic links by processes with lower privileges, provided
  // that Developer Mode is enabled.
  //
  // On older versions of Windows where the
  // SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE flag does not exist, the OS
  // will return the error code ERROR_INVALID_PARAMETER when attempting to
  // create a symbolic link without sufficient privileges.
  if (base::win::GetVersion() < base::win::Version::WIN10_RS3) {
    return CreateSymbolicLinkResult::kUnsupported;
  }

  DWORD flags = is_directory ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;

  if (!::CreateSymbolicLink(
          symlink.value().c_str(), target.value().c_str(),
          flags | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)) {
    // SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE works only if Developer Mode
    // is enabled.
    if (::GetLastError() == ERROR_PRIVILEGE_NOT_HELD) {
      return CreateSymbolicLinkResult::kUnsupported;
    }
    return CreateSymbolicLinkResult::kFailed;
  }

  return CreateSymbolicLinkResult::kSucceeded;
}
#endif  // BUILDFLAG(IS_WIN)

CreateSymbolicLinkResult CreateSymbolicLinkForTesting(
    const base::FilePath& target,
    const base::FilePath& symlink) {
#if BUILDFLAG(IS_WIN)
  return CreateWinSymbolicLink(target, symlink, /*is_directory=*/true);
#else
  if (!base::CreateSymbolicLink(target, symlink)) {
    return CreateSymbolicLinkResult::kFailed;
  }
  return CreateSymbolicLinkResult::kSucceeded;
#endif  // BUILDFLAG(IS_WIN)
}

}  // namespace

class TestFileSystemAccessPermissionContext
    : public ChromeFileSystemAccessPermissionContext {
 public:
  explicit TestFileSystemAccessPermissionContext(
      content::BrowserContext* context,
      const base::Clock* clock)
      : ChromeFileSystemAccessPermissionContext(context, clock) {}
  ~TestFileSystemAccessPermissionContext() override = default;

 private:
  base::WeakPtrFactory<TestFileSystemAccessPermissionContext> weak_factory_{
      this};
};

class ChromeFileSystemAccessPermissionContextTest : public testing::Test {
 public:
  ChromeFileSystemAccessPermissionContextTest() {
// TODO(crbug.com/40101963): Enable when android persisted permissions are
// implemented.
#if BUILDFLAG(IS_ANDROID)
    scoped_feature_list_.InitWithFeatures(
        {}, {features::kFileSystemAccessPersistentPermissions});
#else
    scoped_feature_list_.InitWithFeatures(
        {features::kFileSystemAccessPersistentPermissions}, {});
#endif
  }
  void SetUp() override {
    // Create a scoped directory under %TEMP% instead of using
    // `base::ScopedTempDir::CreateUniqueTempDir`.
    // `base::ScopedTempDir::CreateUniqueTempDir` creates a path under
    // %ProgramFiles% on Windows when running as Admin, which is a blocked path
    // (`kBlockedPaths`). This can fail some of the tests.
    ASSERT_TRUE(
        temp_dir_.CreateUniqueTempDirUnderPath(base::GetTempDirForTesting()));

    // Normalize the file path since the blocklist checks normalizes file paths
    // before running its checks.
    base::FilePath normalized_temp_dir;
    base::NormalizeFilePath(temp_dir_.Take(), &normalized_temp_dir);
    ASSERT_TRUE(temp_dir_.Set(normalized_temp_dir));

    // We create a custom user data directory and place the the testing profile
    // directory inside of it. This simulates a non-default --user-data-dir.
    // Without this step, the testing profile directory is placed directly in
    // the test's temp dir, but we need it to be nested one more level from
    // there to simulate a real environment.
    base::FilePath user_data_dir =
        base::CreateUniqueTempDirectoryScopedToTest();
    base::FilePath profile_dir;
    base::CreateTemporaryDirInDir(user_data_dir, {}, &profile_dir);
    profile_ = TestingProfile::Builder().SetPath(profile_dir).Build();

    DownloadCoreServiceFactory::GetForBrowserContext(profile())
        ->SetDownloadManagerDelegateForTesting(
            std::make_unique<ChromeDownloadManagerDelegate>(profile()));

    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
    FileSystemAccessPermissionRequestManager::CreateForWebContents(
        web_contents());
    content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(kTestOrigin.GetURL());

    FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
        ->set_auto_response_for_test(PermissionAction::DISMISSED);
    permission_context_ =
        std::make_unique<TestFileSystemAccessPermissionContext>(
            browser_context(), task_environment_.GetMockClock());
#if !BUILDFLAG(IS_ANDROID)
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
#endif
  }

  void TearDown() override {
#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
    enterprise_connectors::test::ClearAnalysisConnector(
        profile()->GetPrefs(), enterprise_connectors::FILE_ATTACHED);
#endif
    task_environment_.RunUntilIdle();
    ASSERT_TRUE(temp_dir_.Delete());
    web_contents_.reset();
  }

  SensitiveDirectoryResult ConfirmSensitiveEntryAccessSync(
      ChromeFileSystemAccessPermissionContext* context,
      const PathInfo& path_info,
      HandleType handle_type,
      UserAction user_action) {
    base::test::TestFuture<
        ChromeFileSystemAccessPermissionContext::SensitiveEntryResult>
        future;
    permission_context_->ConfirmSensitiveEntryAccess(
        kTestOrigin, path_info, handle_type, user_action,
        content::GlobalRenderFrameHostId(), future.GetCallback());
    return future.Get();
  }

  // Shorthand for `ConfirmSensitiveEntryAccessSync()` with some common
  // arguments.
  SensitiveDirectoryResult GetOpenResult(const base::FilePath& path,
                                         HandleType handle_type) {
    base::test::TestFuture<
        ChromeFileSystemAccessPermissionContext::SensitiveEntryResult>
        future;
    permission_context_->ConfirmSensitiveEntryAccess(
        kTestOrigin, PathInfo(path), handle_type, UserAction::kOpen,
        content::GlobalRenderFrameHostId(), future.GetCallback());
    return future.Get();
  }

  bool IsOpenAllowed(const base::FilePath& path, HandleType handle_type) {
    return GetOpenResult(path, handle_type) ==
           SensitiveDirectoryResult::kAllowed;
  }

  bool IsOpenAbort(const base::FilePath& path, HandleType handle_type) {
    return GetOpenResult(path, handle_type) == SensitiveDirectoryResult::kAbort;
  }

  void SetDefaultContentSettingValue(ContentSettingsType type,
                                     ContentSetting value) {
    HostContentSettingsMap* content_settings =
        HostContentSettingsMapFactory::GetForProfile(profile_.get());
    content_settings->SetDefaultContentSetting(type, value);
  }

  void SetContentSettingValueForOrigin(url::Origin origin,
                                       ContentSettingsType type,
                                       ContentSetting value) {
    HostContentSettingsMap* content_settings =
        HostContentSettingsMapFactory::GetForProfile(profile_.get());
    content_settings->SetContentSettingDefaultScope(
        origin.GetURL(), origin.GetURL(), type, value);
  }

  // Triggers the Restore permission prompt from two dormant grants
  // (`kTestPathInfo` and `kTestPathInfo2`). Note that the scoped references to
  // active grants are gone after this call, and the active grants may not exist
  // in permission context.
  PermissionRequestOutcome TriggerRestorePermissionPromptAfterBeingBackgrounded(
      const url::Origin& origin) {
    base::HistogramTester histograms;
    auto grant1 = permission_context()->GetReadPermissionGrant(
        kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);
    auto grant2 = permission_context()->GetReadPermissionGrant(
        kTestOrigin, kTestPathInfo2, HandleType::kFile, UserAction::kOpen);
    EXPECT_EQ(grant1->GetStatus(), PermissionStatus::GRANTED);
    EXPECT_EQ(grant2->GetStatus(), PermissionStatus::GRANTED);

    // Dormant grants exist after tabs are backgrounded for the amount of time
    // specified by the extended permissions policy, and the corresponding
    // histogram is recorded.
    permission_context()->OnAllTabsInBackgroundTimerExpired(
        kTestOrigin,
        OneTimePermissionsTrackerObserver::BackgroundExpiryType::kLongTimeout);
    histograms.ExpectBucketCount(
        permissions::PermissionUmaUtil::GetOneTimePermissionEventHistogram(
            ContentSettingsType::FILE_SYSTEM_WRITE_GUARD),
        static_cast<base::HistogramBase::Sample>(
            permissions::OneTimePermissionEvent::EXPIRED_IN_BACKGROUND),
        1);
    EXPECT_EQ(grant1->GetStatus(), PermissionStatus::ASK);
    EXPECT_EQ(grant2->GetStatus(), PermissionStatus::ASK);

    // The Restore Permission prompt is triggered by calling
    // `requestPermission()` on the handle of an existing dormant grant.
    base::test::TestFuture<PermissionRequestOutcome> future;
    grant1->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                              future.GetCallback());
    auto result = future.Get();
    if (result == PermissionRequestOutcome::kGrantedByRestorePrompt) {
      histograms.ExpectBucketCount(
          "Storage.FileSystemAccess.RestorePermissionPromptOutcome",
          RestorePermissionPromptOutcome::kAllowed, 1);
      EXPECT_EQ(grant1->GetStatus(), PermissionStatus::GRANTED);
      EXPECT_EQ(grant2->GetStatus(), PermissionStatus::GRANTED);
#if BUILDFLAG(IS_ANDROID)
    } else if (result == PermissionRequestOutcome::kUserGranted) {
      // Android does not support persistent permissions and requests again.
      // TODO(crbug.com/40101963): Remove when android persisted permissions are
      // implemented.
      EXPECT_EQ(grant1->GetStatus(), PermissionStatus::GRANTED);
      EXPECT_EQ(grant2->GetStatus(), PermissionStatus::ASK);
#endif
    } else {
      EXPECT_EQ(grant1->GetStatus(), PermissionStatus::ASK);
      EXPECT_EQ(grant2->GetStatus(), PermissionStatus::ASK);
    }
    return result;
  }

  ChromeFileSystemAccessPermissionContext* permission_context() {
    return permission_context_.get();
  }

  BrowserContext* browser_context() { return profile_.get(); }
  TestingProfile* profile() { return profile_.get(); }
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
  static constexpr char kPermissionIsDirectoryKey[] = "is-directory";
  static constexpr char kPermissionWritableKey[] = "writable";
  static constexpr char kPermissionReadableKey[] = "readable";
  static constexpr char kDeprecatedPermissionLastUsedTimeKey[] = "time";
  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kTestOrigin2 =
      url::Origin::Create(GURL("https://test.com"));
  const url::Origin kPdfOrigin = url::Origin::Create(
      GURL("chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/index.html"));
  const std::string kTestStartingDirectoryId = "test_id";
  const PathInfo kTestPathInfo = PathInfo(FILE_PATH_LITERAL("/foo/bar"));
  const PathInfo kTestPathInfo2 = PathInfo(FILE_PATH_LITERAL("/baz/"));
  const url::Origin kChromeOrigin = url::Origin::Create(GURL("chrome://test"));

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  base::ScopedTempDir profile_dir_;
  std::unique_ptr<ChromeFileSystemAccessPermissionContext> permission_context_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<WebContents> web_contents_;
  base::test::ScopedFeatureList scoped_feature_list_;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  web_app::test::ScopedSkipMainProfileCheck skip_main_profile_check_;
#endif
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

class ChromeFileSystemAccessPermissionContextSymbolicLinkCheckTest
    : public ChromeFileSystemAccessPermissionContextTest {
 public:
  ChromeFileSystemAccessPermissionContextSymbolicLinkCheckTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kFileSystemAccessSymbolicLinkCheck);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_NoSpecialPath) {
  const PathInfo kTestPathInfo(FILE_PATH_LITERAL(
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
      "c:\\foo\\bar"
#else
      "/foo/bar"
#endif
      ));

  // Paths outside of any special directories should be allowed.
  EXPECT_EQ(
      ConfirmSensitiveEntryAccessSync(permission_context(), kTestPathInfo,
                                      HandleType::kFile, UserAction::kOpen),
      SensitiveDirectoryResult::kAllowed);
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(permission_context(), kTestPathInfo,
                                            HandleType::kDirectory,
                                            UserAction::kOpen),
            SensitiveDirectoryResult::kAllowed);

  // External (relative) paths should also be allowed.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(),
                PathInfo(PathType::kExternal,
                         base::FilePath(FILE_PATH_LITERAL("foo/bar"))),
                HandleType::kFile, UserAction::kOpen),
            SensitiveDirectoryResult::kAllowed);

  // Paths outside of any special directories with no user action should be
  // allowed.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(permission_context(), kTestPathInfo,
                                            HandleType::kDirectory,
                                            UserAction::kNone),
            SensitiveDirectoryResult::kAllowed);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_DontBlockAllChildren) {
  base::FilePath home_dir = temp_dir_.GetPath().AppendASCII("home");
  base::ScopedPathOverride home_override(base::DIR_HOME, home_dir, true, true);

  // The Home directory itself should not be allowed.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathInfo(home_dir),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);
  // The parent of the Home directory should also not be allowed.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathInfo(temp_dir_.GetPath()),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);
  // Paths inside of the Home directory should be allowed.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathInfo(home_dir.AppendASCII("foo")),
                HandleType::kFile, UserAction::kOpen),
            SensitiveDirectoryResult::kAllowed);
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathInfo(home_dir.AppendASCII("foo")),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAllowed);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_BlockAllChildren) {
  base::FilePath app_dir = temp_dir_.GetPath().AppendASCII("app");
  base::ScopedPathOverride app_override(base::DIR_EXE, app_dir, true, true);

  // The App directory itself should not be allowed.
  EXPECT_TRUE(IsOpenAbort(app_dir, HandleType::kDirectory));
  // The parent of App directory should also not be allowed.
  EXPECT_TRUE(IsOpenAbort(temp_dir_.GetPath(), HandleType::kDirectory));
  // Paths inside of the App directory should also not be allowed.
  EXPECT_TRUE(IsOpenAbort(app_dir.AppendASCII("foo"), HandleType::kFile));
  EXPECT_TRUE(IsOpenAbort(app_dir.AppendASCII("foo"), HandleType::kDirectory));

#if BUILDFLAG(IS_ANDROID)
  base::FilePath app_data_dir = temp_dir_.GetPath().AppendASCII("app_data");
  base::ScopedPathOverride app_data_override(base::DIR_ANDROID_APP_DATA,
                                             app_data_dir, true, true);

  // The android app data directory, its parent and paths inside should not be
  // allowed.
  EXPECT_TRUE(IsOpenAbort(app_data_dir, HandleType::kDirectory));
  EXPECT_TRUE(IsOpenAbort(temp_dir_.GetPath(), HandleType::kDirectory));
  EXPECT_TRUE(IsOpenAbort(app_data_dir.AppendASCII("foo"), HandleType::kFile));
  EXPECT_TRUE(
      IsOpenAbort(app_data_dir.AppendASCII("foo"), HandleType::kDirectory));

  base::FilePath cache_dir = temp_dir_.GetPath().AppendASCII("cache");
  base::ScopedPathOverride cache_override(base::DIR_CACHE, cache_dir, true,
                                          true);
  // The android cache directory, its parent and paths inside should not be
  // allowed.
  EXPECT_TRUE(IsOpenAbort(cache_dir, HandleType::kDirectory));
  EXPECT_TRUE(IsOpenAbort(temp_dir_.GetPath(), HandleType::kDirectory));
  EXPECT_TRUE(IsOpenAbort(cache_dir.AppendASCII("foo"), HandleType::kFile));
  EXPECT_TRUE(
      IsOpenAbort(cache_dir.AppendASCII("foo"), HandleType::kDirectory));

#endif  // BUILDFLAG(IS_ANDROID)
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_BlockChildrenNested) {
  base::FilePath user_data_dir = temp_dir_.GetPath().AppendASCII("user");
  base::ScopedPathOverride user_data_override(chrome::DIR_USER_DATA,
                                              user_data_dir, true, true);
  {
    base::FilePath download_dir = user_data_dir.AppendASCII("downloads");
    base::ScopedPathOverride download_override(chrome::DIR_DEFAULT_DOWNLOADS,
                                               download_dir, true, true);

    // The User Data directory itself should not be allowed.
    EXPECT_FALSE(IsOpenAllowed(user_data_dir, HandleType::kDirectory));
    // The parent of the User Data directory should also not be allowed.
    EXPECT_FALSE(IsOpenAllowed(temp_dir_.GetPath(), HandleType::kDirectory));
    // The nested Download directory itself should not be allowed.
    EXPECT_FALSE(IsOpenAllowed(download_dir, HandleType::kDirectory));
    // The paths inside of the nested Download directory should be allowed.
    EXPECT_TRUE(
        IsOpenAllowed(download_dir.AppendASCII("foo"), HandleType::kFile));
    EXPECT_TRUE(
        IsOpenAllowed(download_dir.AppendASCII("foo"), HandleType::kDirectory));
  }

  // The profile directory, its children, and its direct parent should all be
  // blocked. Note that this may not match USER_DATA_DIR if the --user-data-dir
  // override is used.
  {
    base::FilePath profile_path;
    base::NormalizeFilePath(profile()->GetPath(), &profile_path);
    base::FilePath download_dir = profile_path.AppendASCII("downloads");
    base::ScopedPathOverride download_override(chrome::DIR_DEFAULT_DOWNLOADS,
                                               download_dir, true, true);

    EXPECT_FALSE(IsOpenAllowed(profile_path, HandleType::kDirectory));
    EXPECT_FALSE(
        IsOpenAllowed(profile_path.AppendASCII("foo"), HandleType::kFile));
    EXPECT_FALSE(IsOpenAllowed(profile_path.DirName(), HandleType::kDirectory));
    // The paths inside of the nested Download directory should be allowed.
    EXPECT_TRUE(
        IsOpenAllowed(download_dir.AppendASCII("foo"), HandleType::kFile));
    EXPECT_TRUE(
        IsOpenAllowed(download_dir.AppendASCII("foo"), HandleType::kDirectory));
  }

#if BUILDFLAG(IS_WIN)
  // `DIR_IE_INTERNET_CACHE` is an example of a directory where nested
  // directories are blocked, but nested files should be allowed.
  base::FilePath internet_cache = user_data_dir.AppendASCII("INetCache");
  base::ScopedPathOverride internet_cache_override(base::DIR_IE_INTERNET_CACHE,
                                                   internet_cache, true, true);

  // The nested INetCache directory itself should not be allowed.
  EXPECT_FALSE(IsOpenAllowed(internet_cache, HandleType::kDirectory));
  // Files inside of the nested INetCache directory should be allowed.
  EXPECT_TRUE(
      IsOpenAllowed(internet_cache.AppendASCII("foo"), HandleType::kFile));
  // The directories should be blocked.
  EXPECT_FALSE(
      IsOpenAllowed(internet_cache.AppendASCII("foo"), HandleType::kDirectory));
#endif
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_RelativePathBlock) {
  base::FilePath home_dir = temp_dir_.GetPath().AppendASCII("home");
  base::ScopedPathOverride home_override(base::DIR_HOME, home_dir, true, true);

  // ~/.ssh should be blocked.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathInfo(home_dir.AppendASCII(".ssh")),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);
  // And anything inside ~/.ssh should also be blocked.
  EXPECT_EQ(
      ConfirmSensitiveEntryAccessSync(
          permission_context(), PathInfo(home_dir.AppendASCII(".ssh/id_rsa")),
          HandleType::kFile, UserAction::kOpen),
      SensitiveDirectoryResult::kAbort);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_ExplicitPathBlock) {
// Linux is the only OS where we have some blocked directories with explicit
// paths (as opposed to PathService provided paths).
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  // /dev should be blocked.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathInfo(FILE_PATH_LITERAL("/dev")),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);
  // As well as children of /dev.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathInfo(FILE_PATH_LITERAL("/dev/foo")),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathInfo(FILE_PATH_LITERAL("/dev/foo")),
                HandleType::kFile, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);
  // Even if the user action is none, a blocklisted path should be blocked.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathInfo(FILE_PATH_LITERAL("/dev")),
                HandleType::kDirectory, UserAction::kNone),
            SensitiveDirectoryResult::kAbort);
#elif BUILDFLAG(IS_WIN)
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(),
                PathInfo(FILE_PATH_LITERAL("c:\\Program Files")),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);
#endif
}

#if BUILDFLAG(IS_MAC)
TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_DontBlockAllChildren_Overlapping) {
  base::FilePath home_dir = temp_dir_.GetPath().AppendASCII("home");
  base::ScopedPathOverride home_override(base::DIR_HOME, home_dir, true, true);

  // The Home directory itself should not be allowed.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathInfo(home_dir),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);
  // $HOME/Library should be blocked.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathInfo(home_dir.AppendASCII("Library")),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);
  // $HOME/Library/Mobile Documents should be blocked.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(),
                PathInfo(home_dir.AppendASCII("Library/Mobile Documents")),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);
  // Paths within $HOME/Library/Mobile Documents should not be blocked.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(),
                PathInfo(home_dir.AppendASCII("Library/Mobile Documents/foo")),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAllowed);
  // Except for $HOME/Library/Mobile Documents/com~apple~CloudDocs, which should
  // be blocked.
  EXPECT_EQ(

      ConfirmSensitiveEntryAccessSync(
          permission_context(),
          PathInfo(home_dir.AppendASCII(
              "Library/Mobile Documents/com~apple~CloudDocs")),
          HandleType::kDirectory, UserAction::kOpen),
      SensitiveDirectoryResult::kAbort);
  // Paths within $HOME/Library/Mobile Documents/com~apple~CloudDocs should not
  // be blocked.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(),
                PathInfo(home_dir.AppendASCII(
                    "Library/Mobile Documents/com~apple~CloudDocs/foo")),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAllowed);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_BlockAllChildren_RootApplicationsDir) {
  // Root Applications directory should be blocked.
  base::FilePath root_applications_dir(FILE_PATH_LITERAL("/Applications"));
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathInfo(root_applications_dir),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);

  // Paths within /Applications should be blocked.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(),
                PathInfo(root_applications_dir.AppendASCII("foo")),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_BlockAllChildren_UserApplicationsDir) {
  base::FilePath home_dir = temp_dir_.GetPath().AppendASCII("home");
  base::ScopedPathOverride home_override(base::DIR_HOME, home_dir, true, true);

  // User's Applications directory should be blocked.
  base::FilePath user_applications_dir(home_dir.AppendASCII("Applications"));
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathInfo(user_applications_dir),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);

  // Paths within $HOME/Applications should be blocked.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(),
                PathInfo(user_applications_dir.AppendASCII("foo")),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_UNCPath) {
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(),
                PathInfo(FILE_PATH_LITERAL("\\\\server\\share\\foo\\bar")),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAllowed);

  EXPECT_EQ(
      ConfirmSensitiveEntryAccessSync(
          permission_context(), PathInfo(FILE_PATH_LITERAL("c:\\\\foo\\bar")),
          HandleType::kDirectory, UserAction::kOpen),
      SensitiveDirectoryResult::kAllowed);

  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(),
                PathInfo(FILE_PATH_LITERAL("\\\\localhost\\c$\\foo\\bar")),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);

  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(),
                PathInfo(FILE_PATH_LITERAL("\\\\LOCALHOST\\c$\\foo\\bar")),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);

  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(),
                PathInfo(FILE_PATH_LITERAL("\\\\127.0.0.1\\c$\\foo\\bar")),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);

  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(),
                PathInfo(FILE_PATH_LITERAL("\\\\.\\c:\\foo\\bar")),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);

  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(),
                PathInfo(FILE_PATH_LITERAL("\\\\?\\c:\\foo\\bar")),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);

  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(),
                PathInfo(FILE_PATH_LITERAL(
                    "\\\\;LanmanRedirector\\localhost\\c$\\foo\\bar")),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);

  EXPECT_EQ(
      ConfirmSensitiveEntryAccessSync(
          permission_context(),
          PathInfo(FILE_PATH_LITERAL("\\\\.\\UNC\\LOCALHOST\\c:\\foo\\bar")),
          HandleType::kDirectory, UserAction::kOpen),
      SensitiveDirectoryResult::kAbort);

  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(),
                PathInfo(FILE_PATH_LITERAL("\\\\myhostname\\c$\\foo\\bar")),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);
}
#endif

#if BUILDFLAG(IS_ANDROID)
TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_ContentUri) {
  // This test runs under org.chromium.native_test.
  // Content-URI with an authority which matches the package name should fail.
  EXPECT_TRUE(IsOpenAbort(
      base::FilePath(
          "content://org.chromium.native_test.fileprovider/cache/dir"),
      HandleType::kDirectory));
  EXPECT_TRUE(IsOpenAbort(
      base::FilePath(
          "content://org.chromium.native_test.fileprovider/cache/file"),
      HandleType::kFile));

  EXPECT_TRUE(IsOpenAllowed(base::FilePath("content://authority/dir"),
                            HandleType::kDirectory));
  EXPECT_TRUE(IsOpenAllowed(base::FilePath("content://authority/file"),
                            HandleType::kFile));
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(ChromeFileSystemAccessPermissionContextSymbolicLinkCheckTest,
       ConfirmSensitiveEntryAccess_ResolveSymbolicLink) {
  base::FilePath symlink1 = temp_dir_.GetPath().AppendASCII("symlink1");

  base::FilePath app_dir = temp_dir_.GetPath().AppendASCII("app");
  ASSERT_TRUE(base::CreateDirectory(app_dir));

  base::ScopedPathOverride app_override(base::DIR_EXE, app_dir, true, true);

  CreateSymbolicLinkResult result =
      CreateSymbolicLinkForTesting(app_dir, symlink1);
  if (result == CreateSymbolicLinkResult::kUnsupported) {
    GTEST_SKIP();
  }
  ASSERT_EQ(result, CreateSymbolicLinkResult::kSucceeded);

  EXPECT_EQ(
      ConfirmSensitiveEntryAccessSync(permission_context(), PathInfo(symlink1),
                                      HandleType::kFile, UserAction::kOpen),
      SensitiveDirectoryResult::kAbort);

  base::FilePath symlink2 = temp_dir_.GetPath().AppendASCII("symlink2");

  base::FilePath allowed_file = temp_dir_.GetPath().AppendASCII("foo");
  ASSERT_TRUE(base::CreateDirectory(allowed_file));

  result = CreateSymbolicLinkForTesting(allowed_file, symlink2);
  if (result == CreateSymbolicLinkResult::kUnsupported) {
    GTEST_SKIP();
  }
  ASSERT_EQ(result, CreateSymbolicLinkResult::kSucceeded);

  EXPECT_EQ(
      ConfirmSensitiveEntryAccessSync(permission_context(), PathInfo(symlink2),
                                      HandleType::kFile, UserAction::kOpen),
      SensitiveDirectoryResult::kAllowed);
}

TEST_F(ChromeFileSystemAccessPermissionContextSymbolicLinkCheckTest,
       ConfirmSensitiveEntryAccess_ResolveBlockPathSymbolicLink) {
  base::FilePath symlink1 = temp_dir_.GetPath().AppendASCII("symlink1");

  base::FilePath app_dir = temp_dir_.GetPath().AppendASCII("app");
  ASSERT_TRUE(base::CreateDirectory(app_dir));

  CreateSymbolicLinkResult result =
      CreateSymbolicLinkForTesting(app_dir, symlink1);
  if (result == CreateSymbolicLinkResult::kUnsupported) {
    GTEST_SKIP();
  }
  ASSERT_EQ(result, CreateSymbolicLinkResult::kSucceeded);

  // Set the blocked path to a symbolic link.
  base::ScopedPathOverride app_override(base::DIR_EXE, symlink1, true, true);

  // The target of the blocked symbolic link should be blocked.
  EXPECT_EQ(
      ConfirmSensitiveEntryAccessSync(permission_context(), PathInfo(app_dir),
                                      HandleType::kFile, UserAction::kOpen),
      SensitiveDirectoryResult::kAbort);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_DangerousFile) {
  base::FilePath home_dir = temp_dir_.GetPath().AppendASCII("home");
  base::ScopedPathOverride home_override(base::DIR_HOME, home_dir, true, true);

  // Saving files with a harmless extension should be allowed.
  EXPECT_EQ(
      ConfirmSensitiveEntryAccessSync(
          permission_context(), PathInfo(home_dir.AppendASCII("test.txt")),
          HandleType::kFile, UserAction::kSave),
      SensitiveDirectoryResult::kAllowed);
  // Saving files with a dangerous extension should show a prompt.
  EXPECT_EQ(
      ConfirmSensitiveEntryAccessSync(
          permission_context(), PathInfo(home_dir.AppendASCII("test.swf")),
          HandleType::kFile, UserAction::kSave),
      SensitiveDirectoryResult::kAbort);
  // Files with a dangerous extension from no user action should be allowed.
  EXPECT_EQ(
      ConfirmSensitiveEntryAccessSync(
          permission_context(), PathInfo(home_dir.AppendASCII("test.swf")),
          HandleType::kFile, UserAction::kNone),
      SensitiveDirectoryResult::kAllowed);
  // Opening files with a dangerous extension should be allowed.
  EXPECT_EQ(
      ConfirmSensitiveEntryAccessSync(
          permission_context(), PathInfo(home_dir.AppendASCII("test.swf")),
          HandleType::kFile, UserAction::kOpen),
      SensitiveDirectoryResult::kAllowed);
  // Opening files with a dangerous compound extension should show a prompt.
  EXPECT_EQ(
      ConfirmSensitiveEntryAccessSync(
          permission_context(), PathInfo(home_dir.AppendASCII("test.txt.swf")),
          HandleType::kFile, UserAction::kSave),
      SensitiveDirectoryResult::kAbort);
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
  // chrome:// scheme is whitelisted, but we can't set the default content
  // setting here because `ALLOW` is not an acceptable option.
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
  // Set the default to `BLOCK` so that the policy being tested overrides it.
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
  // Set the default to `BLOCK` so that the policy being tested overrides it.
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

  permission_context()->SetLastPickedDirectory(
      kTestOrigin, kTestStartingDirectoryId, kTestPathInfo);
  auto path_info = permission_context()->GetLastPickedDirectory(
      kTestOrigin, kTestStartingDirectoryId);
  EXPECT_EQ(path_info, kTestPathInfo);

  PathInfo updated(PathType::kExternal, path_info.path.AppendASCII("baz"));
  permission_context()->SetLastPickedDirectory(
      kTestOrigin, kTestStartingDirectoryId, updated);
  auto new_path_info = permission_context()->GetLastPickedDirectory(
      kTestOrigin, kTestStartingDirectoryId);
  EXPECT_EQ(new_path_info, updated);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       SetLastPickedDirectory_DefaultId) {
  EXPECT_EQ(permission_context()
                ->GetLastPickedDirectory(kTestOrigin, kTestStartingDirectoryId)
                .path,
            base::FilePath());

  // Call `SetLastPickedDirectory` with `kTestStartingDirectoryId`.
  permission_context()->SetLastPickedDirectory(
      kTestOrigin, kTestStartingDirectoryId, kTestPathInfo);
  auto path_info = permission_context()->GetLastPickedDirectory(
      kTestOrigin, kTestStartingDirectoryId);
  EXPECT_EQ(path_info, kTestPathInfo);

  // Call `SetLastPickedDirectory` with an empty (default) ID.
  auto new_id = std::string();
  PathInfo updated(PathType::kExternal, path_info.path.AppendASCII("baz"));
  permission_context()->SetLastPickedDirectory(kTestOrigin, new_id, updated);
  auto new_path_info =
      permission_context()->GetLastPickedDirectory(kTestOrigin, new_id);
  EXPECT_EQ(new_path_info, updated);

  // Confirm that the original ID can still be retrieved as it was previously.
  auto old_path_info = permission_context()->GetLastPickedDirectory(
      kTestOrigin, kTestStartingDirectoryId);
  EXPECT_EQ(old_path_info, kTestPathInfo);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, LimitNumberOfIds) {
  EXPECT_EQ(permission_context()
                ->GetLastPickedDirectory(kTestOrigin, kTestStartingDirectoryId)
                .path,
            base::FilePath());

  permission_context()->SetMaxIdsPerOriginForTesting(3);

  // The default path should NOT be evicted.
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
                                               PathInfo(default_path));
  EXPECT_EQ(permission_context()
                ->GetLastPickedDirectory(kTestOrigin, default_id)
                .path,
            default_path);

  // Set the maximum number of IDs. Only set IDs should return non-empty paths.
  permission_context()->SetLastPickedDirectory(kTestOrigin, id1,
                                               PathInfo(path1));
  Advance(base::Minutes(1));
  permission_context()->SetLastPickedDirectory(kTestOrigin, id2,
                                               PathInfo(path2));
  Advance(base::Minutes(1));
  permission_context()->SetLastPickedDirectory(kTestOrigin, id3,
                                               PathInfo(path3));
  Advance(base::Minutes(1));
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id1).path,
            path1);
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id2).path,
            path2);
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id3).path,
            path3);
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id4).path,
            base::FilePath());  // Unset.

  // Once the fourth id has been set, only `id1` should be evicted.
  permission_context()->SetLastPickedDirectory(kTestOrigin, id4,
                                               PathInfo(path4));
  Advance(base::Minutes(1));
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id1).path,
            base::FilePath());  // Unset.
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id2).path,
            path2);
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id3).path,
            path3);
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id4).path,
            path4);

  // Reset `id1`, evicting `id2`.
  permission_context()->SetLastPickedDirectory(kTestOrigin, id1,
                                               PathInfo(path1));
  Advance(base::Minutes(1));
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id1).path,
            path1);
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id2).path,
            base::FilePath());  // Unset.
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id3).path,
            path3);
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id4).path,
            path4);

  // Ensure that the default path was not evicted.
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
      kTestOrigin, kTestStartingDirectoryId, PathInfo(path));
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
      kTestOrigin, kTestStartingDirectoryId, PathInfo(new_path));
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
       GetWellKnownDirectoryPath_Pdf_Downloads) {
  base::FilePath expected_downloads = temp_dir_.GetPath();
  DownloadPrefs::FromBrowserContext(browser_context())
      ->SkipSanitizeDownloadTargetPathForTesting();
  DownloadPrefs::FromBrowserContext(browser_context())
      ->SetDownloadPath(temp_dir_.GetPath());
#if BUILDFLAG(IS_ANDROID)
  // Android always uses the system Download directory (/storage/emulated/...).
  ASSERT_TRUE(base::android::GetDownloadsDirectory(&expected_downloads));
#endif
  EXPECT_EQ(permission_context()->GetWellKnownDirectoryPath(
                blink::mojom::WellKnownDirectory::kDirDownloads, kPdfOrigin),
            expected_downloads);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetReadPermissionGrant_InitialState_LoadFromStorage) {
  auto grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile,
      UserAction::kLoadFromStorage);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kRead));
}

// TODO(crbug.com/40101963): Enable when android persisted permissions are
// implemented.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetReadPermissionGrant_InitialState_Open_File) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kRead));
}
#endif

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetReadPermissionGrant_InitialState_Open_Directory) {
  auto grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, GrantType::kRead));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_InitialState_LoadFromStorage) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile,
      UserAction::kLoadFromStorage);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_InitialState_Open_File) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_InitialState_Open_Directory) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, GrantType::kWrite));
}

// TODO(crbug.com/40101963): Enable when android persisted permissions are
// implemented.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_InitialState_WritableImplicitState) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));

  // The existing grant should not change if the permission is blocked globally.
  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));

  // Getting a grant for the same file again should also not change the grant,
  // even though asking for more permissions is blocked globally.
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_WriteGrantedChangesExistingGrant) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto grant1 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);
  auto grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  auto grant3 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);
  // All grants should be equivalent, and should be granted and persisted.
  EXPECT_EQ(grant1, grant2);
  EXPECT_EQ(grant1, grant3);
  EXPECT_EQ(grant1->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(
    ChromeFileSystemAccessPermissionContextNoPersistenceTest,
    GetWritePermissionGrant_GrantIsRevokedWhenNoLongerUsed_NoPersistentPermissions) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  grant.reset();
  // After reset, the grant should go be cleared, and the new grant request is
  // in the `ASK` state.
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
}

// TODO(crbug.com/40101963): Enable when android persisted permissions are
// implemented.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_GrantIsAutoGrantedViaPersistentPermissions) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  // A valid persisted permission should be created.
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));

  permission_context()->RevokeActiveGrantsForTesting(kTestOrigin,
                                                     kTestPathInfo.path);

  // Permission should be auto-granted here via the persisted permission.
  base::test::TestFuture<PermissionRequestOutcome> future;
  grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                           future.GetCallback());
  EXPECT_EQ(future.Get(),
            PermissionRequestOutcome::kGrantedByPersistentPermission);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       IsValidObject_GrantsWithDeprecatedTimestampKeyAreNotValidObjects) {
  // Create a placeholder grant for testing, containing a
  // 'kDeprecatedPermissionLastUsedTimeKey' key, which should render the
  // permission object invalid.
  base::Value::Dict grant;
  grant.Set(ChromeFileSystemAccessPermissionContext::kPermissionPathKey,
            FilePathToValue(kTestPathInfo.path));
  grant.Set(kPermissionIsDirectoryKey, true);
  grant.Set(kPermissionReadableKey, true);
  grant.Set(kDeprecatedPermissionLastUsedTimeKey,
            base::TimeToValue(base::Time::Min()));
  EXPECT_FALSE(permission_context()->IsValidObject(grant));
}

// TODO(crbug.com/40101963): Enable when android persisted permissions are
// implemented.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(
    ChromeFileSystemAccessPermissionContextTest,
    GetGrantedObjectsAndConvertObjectsToGrants_GrantsAreRetainedViaPersistedPermissions) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  PathInfo kTestPath2(kTestPathInfo.path.AppendASCII("baz"));
  auto file_write_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  auto file_read_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  auto file_read_only_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo2, HandleType::kFile, UserAction::kSave);
  auto grants = permission_context()->ConvertObjectsToGrants(
      permission_context()->GetGrantedObjects(kTestOrigin));
  std::vector<base::FilePath> expected_file_write_grants = {kTestPathInfo.path};
  std::vector<base::FilePath> expected_file_read_grants = {kTestPathInfo2.path,
                                                           kTestPathInfo.path};
  EXPECT_EQ(grants.file_write_grants, expected_file_write_grants);
  EXPECT_EQ(grants.file_read_grants, expected_file_read_grants);

  auto granted_paths = permission_context()->GetGrantedPaths(kTestOrigin);
  ASSERT_THAT(granted_paths, testing::SizeIs(2));
  EXPECT_EQ(kTestPathInfo2.path, granted_paths[0]);
  EXPECT_EQ(kTestPathInfo.path, granted_paths[1]);

  // Persisted permissions are retained after resetting the active grants.
  file_write_grant.reset();
  file_read_grant.reset();
  file_read_only_grant.reset();
  grants = permission_context()->ConvertObjectsToGrants(
      permission_context()->GetGrantedObjects(kTestOrigin));
  EXPECT_EQ(grants.file_write_grants, expected_file_write_grants);
  EXPECT_EQ(grants.file_read_grants, expected_file_read_grants);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetExtendedPersistedObjects) {
  PathInfo kTestPath2(kTestPathInfo.path.AppendASCII("foo"));
  const url::Origin kTestOrigin2 =
      url::Origin::Create(GURL("https://www.c.com"));
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin2);
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  auto grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin2, kTestPathInfo2, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  ASSERT_THAT(
      permission_context()->GetExtendedPersistedObjectsForTesting(kTestOrigin),
      testing::SizeIs(1));
  ASSERT_THAT(
      permission_context()->GetExtendedPersistedObjectsForTesting(kTestOrigin2),
      testing::SizeIs(1));

  // Revoke the active grant, but not the persisted permission. The granted
  // object for the given origin is not revoked.
  permission_context()->RevokeActiveGrantsForTesting(kTestOrigin);
  ASSERT_THAT(
      permission_context()->GetExtendedPersistedObjectsForTesting(kTestOrigin),
      testing::SizeIs(1));

  // The granted objects are updated when all of its permissions are revoked.
  permission_context()->RevokeGrants(kTestOrigin);
  ASSERT_THAT(
      permission_context()->GetExtendedPersistedObjectsForTesting(kTestOrigin),
      testing::IsEmpty());

  // An empty vector is returned when an origin does not have extended
  // permissions.
  SetContentSettingValueForOrigin(kTestOrigin2,
                                  ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                  CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin2, kTestPathInfo, HandleType::kFile, GrantType::kWrite));
  auto granted_objects_no_persistent_permissions =
      permission_context()->GetExtendedPersistedObjectsForTesting(kTestOrigin2);
  EXPECT_TRUE(granted_objects_no_persistent_permissions.empty());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_InitialState_OpenAction_GlobalGuardBlocked) {
  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::DENIED);
  grant.reset();

  SetContentSettingValueForOrigin(kTestOrigin,
                                  ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                  CONTENT_SETTING_ASK);

  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));
}

TEST_F(
    ChromeFileSystemAccessPermissionContextTest,
    GetWritePermissionGrant_InitialState_WritableImplicitState_GlobalGuardBlocked) {
  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::DENIED);
  grant.reset();

  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));

  SetContentSettingValueForOrigin(kTestOrigin,
                                  ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                  CONTENT_SETTING_ASK);

  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(
    ChromeFileSystemAccessPermissionContextTest,
    GetWritePermissionGrant_WriteGrantedChangesExistingGrant_GlobalGuardBlocked) {
  SetContentSettingValueForOrigin(kTestOrigin,
                                  ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                  CONTENT_SETTING_BLOCK);

  auto grant1 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);
  auto grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  auto grant3 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);
  // All grants should be equivalent, and have a `DENIED` state.
  EXPECT_EQ(grant1, grant2);
  EXPECT_EQ(grant1, grant3);
  EXPECT_EQ(grant1->GetStatus(), PermissionStatus::DENIED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));
}

TEST_F(
    ChromeFileSystemAccessPermissionContextTest,
    GetWritePermissionGrant_GrantIsRevokedWhenNoLongerUsed_GlobalGuardBlockedBeforeNewGrant) {
  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::DENIED);
  grant.reset();

  // After reset, the grant should go away and the new grant request should
  // have a `DENIED` state.
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::DENIED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextNoPersistenceTest,
       GetGrantedObjects_NoPersistentPermissions) {
  auto grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);
  auto grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  // Only one permission grant object is recorded when a given origin has both
  // read and write access for a given resource.
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(1));

  auto granted_paths = permission_context()->GetGrantedPaths(kTestOrigin);
  ASSERT_THAT(granted_paths, testing::SizeIs(1));
  EXPECT_EQ(kTestPathInfo.path, granted_paths[0]);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetOriginsWithGrants_ForGrantedActiveGrantsOnly) {
  auto grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  ASSERT_THAT(permission_context()->GetOriginsWithGrants(), testing::SizeIs(1));

  // After grants are revoked, `GetOriginsWithGrants()` only returns origins
  // with active grants with a `GRANTED` permission status.
  permission_context()->RevokeGrants(kTestOrigin);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  ASSERT_THAT(permission_context()->GetOriginsWithGrants(), testing::SizeIs(0));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RestorePermissionPrompt_Triggered_AfterTabBackgrounded) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);
  auto result =
      TriggerRestorePermissionPromptAfterBeingBackgrounded(kTestOrigin);
#if BUILDFLAG(IS_ANDROID)
  // Android does not support persistent permissions and requests again.
  // TODO(crbug.com/40101963): Remove when android persisted permissions are
  // implemented.
  EXPECT_EQ(result, PermissionRequestOutcome::kUserGranted);
#else
  EXPECT_EQ(result, PermissionRequestOutcome::kGrantedByRestorePrompt);
#endif
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RestorePermissionPrompt_Triggered_HandleLoadedFromStorage) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  // Create dormant grants that are eligible to be restored.
  // Use a directory handle, which is not auto-granted, so that later it can be
  // demonstrated that a directory handle is not able to trigger the restore
  // prompt, and will instead trigger the single-file permission prompt.
  auto grant1 = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, UserAction::kOpen);
  base::test::TestFuture<PermissionRequestOutcome> grant1_future;
  grant1->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                            grant1_future.GetCallback());
  EXPECT_EQ(grant1_future.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(grant1->GetStatus(), PermissionStatus::GRANTED);

  auto grant2 = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo2, HandleType::kDirectory, UserAction::kOpen);
  base::test::TestFuture<PermissionRequestOutcome> grant2_future;
  grant2->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                            grant2_future.GetCallback());
  EXPECT_EQ(grant2_future.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(grant2->GetStatus(), PermissionStatus::GRANTED);

  // TODO(crbug.com/40101962): Update this test to navigate away from the page,
  // instead of manually resetting the grant.
  permission_context()->RevokeActiveGrantsForTesting(kTestOrigin);
  grant1.reset();
  grant2.reset();

  // Get the handles from the storage (i.e. IndexedDB).
  auto grant1_from_storage = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory,
      UserAction::kLoadFromStorage);
  auto grant2_from_storage = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo2, HandleType::kDirectory,
      UserAction::kLoadFromStorage);
  EXPECT_EQ(grant1_from_storage->GetStatus(), PermissionStatus::ASK);
  EXPECT_EQ(grant2_from_storage->GetStatus(), PermissionStatus::ASK);

  // `requestPermission()` on a handle from IndxedDB triggers the Restore
  // Permission prompt.
  base::test::TestFuture<PermissionRequestOutcome> future;
  grant1_from_storage->RequestPermission(
      frame_id(), UserActivationState::kNotRequired, future.GetCallback());
#if BUILDFLAG(IS_ANDROID)
  // Android does not support persistent permissions and requests again.
  // TODO(crbug.com/40101963): Remove when android persisted permissions are
  // implemented.
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(grant1_from_storage->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_EQ(grant2_from_storage->GetStatus(), PermissionStatus::ASK);
#else
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kGrantedByRestorePrompt);
  EXPECT_EQ(grant1_from_storage->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_EQ(grant2_from_storage->GetStatus(), PermissionStatus::GRANTED);
#endif
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RestorePermissionPrompt_NotTriggered_HandleNotLoadedFromStorage) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  // Create dormant grants that are eligible to be restored.
  // Use a directory handle, which is not auto-granted, so that later it can be
  // demonstrated that a directory handle is not able to trigger the restore
  // prompt and can only trigger the single-file permission flow.
  auto grant1 = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, UserAction::kOpen);
  base::test::TestFuture<PermissionRequestOutcome> grant1_future;
  grant1->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                            grant1_future.GetCallback());
  EXPECT_EQ(grant1_future.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(grant1->GetStatus(), PermissionStatus::GRANTED);

  auto grant2 = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo2, HandleType::kDirectory, UserAction::kOpen);
  base::test::TestFuture<PermissionRequestOutcome> grant2_future;
  grant2->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                            grant2_future.GetCallback());
  EXPECT_EQ(grant2_future.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(grant2->GetStatus(), PermissionStatus::GRANTED);

  // TODO(crbug.com/40101962): Update this test to navigate away from the page,
  // instead of manually resetting the grant.
  permission_context()->RevokeActiveGrantsForTesting(kTestOrigin);
  grant1.reset();
  grant2.reset();

  // Get the handles from the directory picker.
  auto grant1_not_from_storage = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, UserAction::kOpen);
  auto grant2_not_from_storage = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo2, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(grant1_not_from_storage->GetStatus(), PermissionStatus::ASK);
  EXPECT_EQ(grant2_not_from_storage->GetStatus(), PermissionStatus::ASK);

  // Calling `requestPermission()` on a handle not loaded from IndexedDB will
  // not trigger the restore permission prompt. Only the requested handle is
  // granted permission.
  base::test::TestFuture<PermissionRequestOutcome> future;
  grant1_not_from_storage->RequestPermission(
      frame_id(), UserActivationState::kNotRequired, future.GetCallback());
  EXPECT_NE(future.Get(), PermissionRequestOutcome::kGrantedByRestorePrompt);
  EXPECT_EQ(grant1_not_from_storage->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_EQ(grant2_not_from_storage->GetStatus(), PermissionStatus::ASK);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RestorePermissionPrompt_NotTriggered_WhenNoDormatGrants) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);
  auto grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  // The origin only has active grants, and no dormant grants. Therefore, the
  // origin should not grant permission via the Restore Permission prompt.
  base::test::TestFuture<PermissionRequestOutcome> future;
  grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                           future.GetCallback());
  auto permission_request_outcome = future.Get();
  EXPECT_NE(permission_request_outcome,
            PermissionRequestOutcome::kGrantedByRestorePrompt);
}

TEST_F(
    ChromeFileSystemAccessPermissionContextTest,
    RestorePermissionPrompt_NotTriggered_WhenRequestingWriteAccessToReadGrant) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);
  auto grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);

  // Dormant grants exist after tabs are backgrounded for the amount of time
  // specified by the extended permissions policy.
  permission_context()->OnAllTabsInBackgroundTimerExpired(
      kTestOrigin,
      OneTimePermissionsTrackerObserver::BackgroundExpiryType::kLongTimeout);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);

  // The origin is not eligible to request permission via the restore prompt if
  // requesting write access to a file which previously had read access.
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile,
      UserAction::kLoadFromStorage);
  base::test::TestFuture<PermissionRequestOutcome> future;
  grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                           future.GetCallback());
  auto permission_request_outcome = future.Get();
  EXPECT_NE(permission_request_outcome,
            PermissionRequestOutcome::kGrantedByRestorePrompt);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RestorePermissionPrompt_NotTriggered_WhenRequestAccessToNewFile) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);

  // Dormant grants exist after tabs are backgrounded for the amount of time
  // specified by the extended permissions policy.
  permission_context()->OnAllTabsInBackgroundTimerExpired(
      kTestOrigin,
      OneTimePermissionsTrackerObserver::BackgroundExpiryType::kLongTimeout);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);

  // The origin is not eligible to request permission via the restore prompt if
  // requesting access to a new file (`kTestPathInfo2`).
  auto grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo2, HandleType::kFile,
      UserAction::kLoadFromStorage);
  base::test::TestFuture<PermissionRequestOutcome> future;
  grant2->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                            future.GetCallback());
  auto permission_request_outcome = future.Get();
  EXPECT_NE(permission_request_outcome,
            PermissionRequestOutcome::kGrantedByRestorePrompt);
}

// TODO(crbug.com/40101963): Enable when android persisted permissions are
// implemented.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RestorePermissionPrompt_AllowEveryTime) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);
  auto result =
      TriggerRestorePermissionPromptAfterBeingBackgrounded(kTestOrigin);
  EXPECT_EQ(result, PermissionRequestOutcome::kGrantedByRestorePrompt);
  EXPECT_EQ(permission_context()->content_settings()->GetContentSetting(
                kTestOrigin.GetURL(), kTestOrigin.GetURL(),
                ContentSettingsType::FILE_SYSTEM_ACCESS_EXTENDED_PERMISSION),
            ContentSetting::CONTENT_SETTING_ALLOW);

  // The dormant grants are now extended grants, which can be returned from
  // `GetGrantedObjects()`.
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(2));

  // TODO(crbug.com/40101962): Update this test to navigate away from the page,
  // instead of manually resetting the grants.
  permission_context()->RevokeActiveGrantsForTesting(kTestOrigin);

  // TODO(crbug.com/40101962): Update this test to navigate away from the page,
  // instead of manually resetting the grants.
  // The granted permission objects remain, even after navigating away from the
  // page.
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(2));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RestorePermissionPrompt_AllowOnce) {
  base::HistogramTester histograms;
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED_ONCE);
  auto grant1 = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);
  auto grant2 = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo2, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant1->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_EQ(grant2->GetStatus(), PermissionStatus::GRANTED);

  // Dormant grants exist after tabs are backgrounded for the amount of time
  // specified by the extended permissions policy.
  permission_context()->OnAllTabsInBackgroundTimerExpired(
      kTestOrigin,
      OneTimePermissionsTrackerObserver::BackgroundExpiryType::kLongTimeout);
  EXPECT_EQ(grant1->GetStatus(), PermissionStatus::ASK);
  EXPECT_EQ(grant2->GetStatus(), PermissionStatus::ASK);

  // Restore Permission prompt is triggered by calling
  // `requestPermission()` on the handle of an existing dormant grant.
  base::test::TestFuture<PermissionRequestOutcome> future;
  grant1->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                            future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kGrantedByRestorePrompt);
  histograms.ExpectBucketCount(
      "Storage.FileSystemAccess.RestorePermissionPromptOutcome",
      RestorePermissionPromptOutcome::kAllowedOnce, 1);
  EXPECT_NE(permission_context()->content_settings()->GetContentSetting(
                kTestOrigin.GetURL(), kTestOrigin.GetURL(),
                ContentSettingsType::FILE_SYSTEM_ACCESS_EXTENDED_PERMISSION),
            ContentSetting::CONTENT_SETTING_ALLOW);
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(2));

  // TODO(crbug.com/40101962): Update this test to navigate away from the page,
  // instead of manually resetting the grants.
  permission_context()->RevokeActiveGrantsForTesting(kTestOrigin);
  // The granted permissions are cleared after navigating away from the page.
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::IsEmpty());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RestorePermissionPrompt_Denied) {
  base::HistogramTester histograms;
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::DENIED);
  EXPECT_EQ(TriggerRestorePermissionPromptAfterBeingBackgrounded(kTestOrigin),
            PermissionRequestOutcome::kUserDenied);
  histograms.ExpectBucketCount(
      "Storage.FileSystemAccess.RestorePermissionPromptOutcome",
      RestorePermissionPromptOutcome::kRejected, 1);

  // Persisted grants are cleared as a result of restore prompt rejection, when
  // Extended Permissions is not enabled.
  // The origin is not embargoed when the restore prompt is rejected one time.
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::IsEmpty());
  auto origin_is_embargoed =
      PermissionDecisionAutoBlockerFactory::GetForProfile(
          Profile::FromBrowserContext(profile()))
          ->IsEmbargoed(
              kTestOrigin.GetURL(),
              ContentSettingsType::FILE_SYSTEM_ACCESS_RESTORE_PERMISSION);
  EXPECT_FALSE(origin_is_embargoed);

  //  Check that the origin is placed under embargo after being ignored
  // `kDefaultDismissalsBeforeBlock` times.
  EXPECT_EQ(TriggerRestorePermissionPromptAfterBeingBackgrounded(kTestOrigin),
            PermissionRequestOutcome::kUserDenied);
  // The origin is not embargoed after being ignored two times, when the limit
  // set by `kDefaultDismissalsBeforeBlock` is three times.
  auto origin_is_embargoed_updated =
      PermissionDecisionAutoBlockerFactory::GetForProfile(
          Profile::FromBrowserContext(profile()))
          ->IsEmbargoed(
              kTestOrigin.GetURL(),
              ContentSettingsType::FILE_SYSTEM_ACCESS_RESTORE_PERMISSION);
  EXPECT_FALSE(origin_is_embargoed_updated);
  // The origin is embargoed, after reaching the ignore limit set by
  // `kDefaultDismissalsBeforeBlock`.
  EXPECT_EQ(TriggerRestorePermissionPromptAfterBeingBackgrounded(kTestOrigin),
            PermissionRequestOutcome::kUserDenied);
  auto origin_is_embargoed_after_rejection_limit =
      PermissionDecisionAutoBlockerFactory::GetForProfile(
          Profile::FromBrowserContext(profile()))
          ->IsEmbargoed(
              kTestOrigin.GetURL(),
              ContentSettingsType::FILE_SYSTEM_ACCESS_RESTORE_PERMISSION);
  EXPECT_TRUE(origin_is_embargoed_after_rejection_limit);
}
#endif  // !BUILDFLAG(IS_ANDROID)

// TODO(crbug.com/40101963): Enable when android webapps integration is done.
#if !BUILDFLAG(IS_ANDROID)

class ChromeFileSystemAccessPermissionContextTestWithWebApp
    : public ChromeFileSystemAccessPermissionContextTest {
 private:
  web_app::OsIntegrationTestOverrideBlockingRegistration fake_os_integration_;
};

TEST_F(ChromeFileSystemAccessPermissionContextTestWithWebApp,
       OnWebAppInstalled) {
  // Create a persisted grant for `kTestOrigin`.
  auto read_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  auto write_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(1));

  // Install a web app for `kTestOrigin`.
  const GURL kTestOriginUrl = GURL("https://example.com");
  web_app::test::InstallDummyWebApp(profile(), "Test App", kTestOriginUrl);

  // When extended permissions is not enabled, the persistent grants are
  // revoked and the grant status is set to current.
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::IsEmpty());
  EXPECT_EQ(
      permission_context()->GetPersistedGrantStatusForTesting(kTestOrigin),
      PersistedGrantStatus::kCurrent);
}

TEST_F(ChromeFileSystemAccessPermissionContextTestWithWebApp,
       OnWebAppInstalled_WithCurrentGrants) {
  // Create current, persisted grants by triggering the restore prompt and
  // accepting it.
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);
  TriggerRestorePermissionPromptAfterBeingBackgrounded(kTestOrigin);
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(2));

  // Install a web app for `kTestOrigin`.
  const GURL kTestOriginUrl = GURL("https://example.com");
  web_app::test::InstallDummyWebApp(profile(), "Test App", kTestOriginUrl);

  // When extended permissions is not enabled and a web app is installed when
  // the grant status is current, the persisted grants are not revoked.
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(2));
}

TEST_F(ChromeFileSystemAccessPermissionContextTestWithWebApp,
       OnWebAppInstalled_ExtendedPermissionsEnabled) {
  // Enable extended permissions.
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);

  // Create a persisted grant for `kTestOrigin`.
  auto read_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  auto write_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(1));

  // Install a web app for `kTestOrigin`.
  const GURL kTestOriginUrl = GURL("https://example.com");
  web_app::test::InstallDummyWebApp(profile(), "Test App", kTestOriginUrl);

  // When extended permissions is enabled, the persisted grants are not
  // revoked and the persisted grant type remains 'extended'.
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(1));
  EXPECT_EQ(permission_context()->GetPersistedGrantTypeForTesting(kTestOrigin),
            PersistedGrantType::kExtended);
}

TEST_F(ChromeFileSystemAccessPermissionContextTestWithWebApp,
       OnWebAppUninstalled_WithPeristentAndActiveGrants) {
  const PathInfo kTestPathInfo2(FILE_PATH_LITERAL("/a/b"));
  // Install a web app for `kTestOrigin`.
  const GURL kTestOriginUrl = GURL("https://example.com");
  const webapps::AppId app_id =
      web_app::test::InstallDummyWebApp(profile(), "Test App", kTestOriginUrl);

  // Create a grant, then revoke its active permissions.
  auto read_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  auto write_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);

  permission_context()->RevokeActiveGrantsForTesting(kTestOrigin);

  // Create another grant, with granted active permissions.
  auto read_grant2 = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo2, HandleType::kFile, UserAction::kSave);
  auto write_grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo2, HandleType::kFile, UserAction::kSave);
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(2));

  // Uninstall the web app for `kTestOrigin`.
  web_app::test::UninstallWebApp(profile(), app_id);

  // After the web app is uninstalled, persistent grants are cleared and
  // re-created off of the granted active grants set.
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(1));
}

TEST_F(ChromeFileSystemAccessPermissionContextTestWithWebApp,
       OnWebAppUninstalled_NoGrantedActiveGrants) {
  // Install a web app for `kTestOrigin`.
  const GURL kTestOriginUrl = GURL("https://example.com");
  const webapps::AppId app_id =
      web_app::test::InstallDummyWebApp(profile(), "Test App", kTestOriginUrl);

  // Create a persistent grant, and revoke its active permissions.
  auto read_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  auto write_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  permission_context()->RevokeActiveGrantsForTesting(kTestOrigin);
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(1));
  EXPECT_EQ(
      permission_context()->GetPersistedGrantStatusForTesting(kTestOrigin),
      PersistedGrantStatus::kLoaded);

  // Uninstall the web app for `kTestOrigin`, while there are persistent grants
  // and no granted active grants.
  web_app::test::UninstallWebApp(profile(), app_id);

  // The grant status is set to current, and the persistent grants are revoked.
  // The persisted grants are not re-created because there were no granted
  // active grants at the time the web app was uninstalled.
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::IsEmpty());
  EXPECT_EQ(
      permission_context()->GetPersistedGrantStatusForTesting(kTestOrigin),
      PersistedGrantStatus::kCurrent);
}
#endif  // !BUILDFLAG(IS_ANDROID)

// TODO(crbug.com/40101963): Enable when android persisted permissions are
// implemented.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ToggleExtendedPermissionByUser) {
  auto read_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  auto write_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);

  EXPECT_FALSE(permission_context()->OriginHasExtendedPermission(kTestOrigin));
  EXPECT_EQ(
      PersistedGrantStatus::kLoaded,
      permission_context()->GetPersistedGrantStatusForTesting(kTestOrigin));

  // The persisted grant status and content setting are updated after the user
  // opts into extended permissions.
  permission_context()->SetOriginExtendedPermissionByUser(kTestOrigin);
  EXPECT_TRUE(permission_context()->OriginHasExtendedPermission(kTestOrigin));
  EXPECT_EQ(
      PersistedGrantStatus::kCurrent,
      permission_context()->GetPersistedGrantStatusForTesting(kTestOrigin));

  // Calling `SetOriginExtendedPermissionByUser` again results in the same
  // state.
  permission_context()->SetOriginExtendedPermissionByUser(kTestOrigin);
  EXPECT_TRUE(permission_context()->OriginHasExtendedPermission(kTestOrigin));
  EXPECT_EQ(
      PersistedGrantStatus::kCurrent,
      permission_context()->GetPersistedGrantStatusForTesting(kTestOrigin));

  // Extended permissions are removed when the user opts out, and the
  // persisted grants remain current.
  permission_context()->RemoveOriginExtendedPermissionByUser(kTestOrigin);
  EXPECT_FALSE(permission_context()->OriginHasExtendedPermission(kTestOrigin));
  EXPECT_EQ(
      PersistedGrantStatus::kCurrent,
      permission_context()->GetPersistedGrantStatusForTesting(kTestOrigin));

  // Calling `RemoveOriginExtendedPermissionByUser` again results in the same
  // state.
  permission_context()->RemoveOriginExtendedPermissionByUser(kTestOrigin);
  EXPECT_FALSE(permission_context()->OriginHasExtendedPermission(kTestOrigin));
  EXPECT_EQ(
      PersistedGrantStatus::kCurrent,
      permission_context()->GetPersistedGrantStatusForTesting(kTestOrigin));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RestorePermissionPrompt_Ignored) {
  base::HistogramTester histograms;
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::IGNORED);
  auto result =
      TriggerRestorePermissionPromptAfterBeingBackgrounded(kTestOrigin);
  histograms.ExpectBucketCount(
      "Storage.FileSystemAccess.RestorePermissionPromptOutcome",
      RestorePermissionPromptOutcome::kIgnored, 1);
  EXPECT_EQ(result, PermissionRequestOutcome::kRequestAborted);

  // Persisted grants are cleared by ignoring the restore prompt.
  // The origin is not embargoed on first ignore.
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::IsEmpty());
  auto origin_is_embargoed =
      PermissionDecisionAutoBlockerFactory::GetForProfile(
          Profile::FromBrowserContext(profile()))
          ->IsEmbargoed(
              kTestOrigin.GetURL(),
              ContentSettingsType::FILE_SYSTEM_ACCESS_RESTORE_PERMISSION);
  EXPECT_FALSE(origin_is_embargoed);

  // The origin is placed under embargo after being ignored
  // `kDefaultIgnoresBeforeBlock` times.
  result = TriggerRestorePermissionPromptAfterBeingBackgrounded(kTestOrigin);
  EXPECT_EQ(result, PermissionRequestOutcome::kRequestAborted);
  result = TriggerRestorePermissionPromptAfterBeingBackgrounded(kTestOrigin);
  EXPECT_EQ(result, PermissionRequestOutcome::kRequestAborted);
  // The origin is not embargoed after being ignored three times, when the
  // limit set by `kDefaultIgnoresBeforeBlock` is four times.
  auto origin_is_embargoed_updated =
      PermissionDecisionAutoBlockerFactory::GetForProfile(
          Profile::FromBrowserContext(profile()))
          ->IsEmbargoed(
              kTestOrigin.GetURL(),
              ContentSettingsType::FILE_SYSTEM_ACCESS_RESTORE_PERMISSION);
  EXPECT_FALSE(origin_is_embargoed_updated);

  // The origin is embargoed, after reaching the ignore limit set by
  // `kDefaultIgnoresBeforeBlock`.
  result = TriggerRestorePermissionPromptAfterBeingBackgrounded(kTestOrigin);
  EXPECT_EQ(result, PermissionRequestOutcome::kRequestAborted);
  auto origin_is_embargoed_after_ignore_limit =
      PermissionDecisionAutoBlockerFactory::GetForProfile(
          Profile::FromBrowserContext(profile()))
          ->IsEmbargoed(
              kTestOrigin.GetURL(),
              ContentSettingsType::FILE_SYSTEM_ACCESS_RESTORE_PERMISSION);
  EXPECT_TRUE(origin_is_embargoed_after_ignore_limit);
}
#endif  // !BUILDFLAG(IS_ANDROID)

// TODO(crbug.com/40101962): Expand upon this test case to cover checking that
// dormant grants are not revoked, when backgrounded dormant grants exist.
// Currently, there is no method to retrieve dormant grants, for testing
// purposes.
TEST_F(ChromeFileSystemAccessPermissionContextTest,
       OnLastPageFromOriginClosed) {
  auto read_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  auto write_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(write_grant->GetStatus(), PermissionStatus::GRANTED);
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(1));
  // When the last tab for an origin is closed or navigated away from,
  // while there are loaded grants, both the active and persistent grants
  // are revoked.
  permission_context()->OnLastPageFromOriginClosed(kTestOrigin);
  EXPECT_EQ(write_grant->GetStatus(), PermissionStatus::ASK);
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::IsEmpty());
}

// TODO(crbug.com/40101963): Enable when android persisted permissions are
// implemented.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(ChromeFileSystemAccessPermissionContextTest,
       OnLastPageFromOriginClosed_PersistedGrantStatusUpdated) {
  // Create a current grant by triggering the restore prompt, and accepting it.
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);
  TriggerRestorePermissionPromptAfterBeingBackgrounded(kTestOrigin);
  EXPECT_EQ(
      permission_context()->GetPersistedGrantStatusForTesting(kTestOrigin),
      PersistedGrantStatus::kCurrent);
  // The grant status is updated to loaded, as a result of the last tab being
  // navigated away from.
  permission_context()->OnLastPageFromOriginClosed(kTestOrigin);
  EXPECT_EQ(
      permission_context()->GetPersistedGrantStatusForTesting(kTestOrigin),
      PersistedGrantStatus::kLoaded);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       OnLastPageFromOriginClosed_HasExtendedPermissions) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto read_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  auto write_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  base::test::TestFuture<PermissionRequestOutcome> future;
  write_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                                 future.GetCallback());
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(1));
  // When the last tab for an origin is closed or navigated away from, while
  // extended permissions is enabled, the grants are not cleared.
  permission_context()->OnLastPageFromOriginClosed(kTestOrigin);
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(1));
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(
    ChromeFileSystemAccessPermissionContextNoPersistenceTest,
    GetWritePermissionGrant_GrantIsRevokedWhenNoLongerUsed_GlobalGuardBlockedAfterNewGrant_NoPersistentPermissions) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);

  // Revoke the active and persisted permissions.
  permission_context()->RevokeGrants(kTestOrigin);
  grant.reset();
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));

  // After resetting the grant, the granted object should not exist, and the
  // new grant request should be in the `ASK` state.
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::IsEmpty());

  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);

  // After the guard is blocked, the permission status for the grant is
  // unchanged.
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));
}

TEST_F(
    ChromeFileSystemAccessPermissionContextTest,
    GetWritePermissionGrant_GrantIsRevokedWhenNoLongerUsed_GlobalGuardBlockedAfterNewGrant_HasPersistentPermissions) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  // Revoke active and persisted permissions.
  permission_context()->RevokeGrants(kTestOrigin);
  grant.reset();
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));

  // After reset, the grant should be cleared and the new grant request should
  // be in the `ASK` state.
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);

  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);

  // After the guard is blocked, the permission status for the `grant` should
  // remain unchanged.
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));
}

// TODO(crbug.com/40101963): Enable when android persisted permissions are
// implemented.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetReadPermissionGrant_InheritFromAncestor) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  auto dir_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(dir_grant->GetStatus(), PermissionStatus::ASK);
  base::test::TestFuture<PermissionRequestOutcome> future;
  dir_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                               future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(dir_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, GrantType::kRead));

  // A file in `dir_path`'s directory should have auto-granted permissions.
  PathInfo file_path(kTestPathInfo.path.AppendASCII("baz"));
  auto file_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, file_path, HandleType::kFile, UserAction::kLoadFromStorage);
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, file_path, HandleType::kFile, GrantType::kRead));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_InheritFromAncestor) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  auto dir_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(dir_grant->GetStatus(), PermissionStatus::ASK);
  base::test::TestFuture<PermissionRequestOutcome> future;
  dir_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                               future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(dir_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, GrantType::kWrite));

  // A file in `dir_path`'s directory should be auto-granted permissions.
  PathInfo file_path(kTestPathInfo.path.AppendASCII("baz"));
  auto file_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, file_path, HandleType::kFile, UserAction::kLoadFromStorage);
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, file_path, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       DoNotInheritFromAncestorOfOppositeType) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  auto dir_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(dir_grant->GetStatus(), PermissionStatus::ASK);
  base::test::TestFuture<PermissionRequestOutcome> future;
  dir_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                               future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(dir_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, GrantType::kRead));

  // `dir_path` already has read permission while we're asking for write
  // permission, so the permission is not auto-granted.
  PathInfo file_path(kTestPathInfo.path.AppendASCII("baz"));
  auto file_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, file_path, HandleType::kFile, UserAction::kLoadFromStorage);
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, file_path, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetReadPermissionGrant_InheritFromPersistedAncestor) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  auto dir_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(dir_grant->GetStatus(), PermissionStatus::ASK);
  base::test::TestFuture<PermissionRequestOutcome> future;
  dir_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                               future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(dir_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, GrantType::kRead));

  // Remove the active grant, but not the persisted permission.
  dir_grant.reset();

  // A file in `dir_path`'s directory should not be granted permission until
  // permission is explicitly requested.
  PathInfo file_path(kTestPathInfo.path.AppendASCII("baz"));
  auto file_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, file_path, HandleType::kFile, UserAction::kLoadFromStorage);
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::ASK);
  base::test::TestFuture<PermissionRequestOutcome> future2;
  file_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                                future2.GetCallback());
  EXPECT_EQ(future2.Get(),
            PermissionRequestOutcome::kGrantedByAncestorPersistentPermission);
  // Age should not be recorded if granted via an ancestor's permission.
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, file_path, HandleType::kFile, GrantType::kRead));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_InheritFromPersistedAncestor) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  auto dir_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(dir_grant->GetStatus(), PermissionStatus::ASK);
  base::test::TestFuture<PermissionRequestOutcome> future;
  dir_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                               future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(dir_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, GrantType::kWrite));

  // Remove the active grant, but not the persisted permission.
  dir_grant.reset();

  // A file in `dir_path`'s directory should not be granted permission until
  // permission is explicitly requested.
  PathInfo file_path(kTestPathInfo.path.AppendASCII("baz"));
  auto file_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, file_path, HandleType::kFile, UserAction::kLoadFromStorage);
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::ASK);
  base::test::TestFuture<PermissionRequestOutcome> future2;
  file_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                                future2.GetCallback());
  EXPECT_EQ(future2.Get(),
            PermissionRequestOutcome::kGrantedByAncestorPersistentPermission);
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, file_path, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       DoNotInheritFromPersistedAncestorOfOppositeType) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  auto dir_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(dir_grant->GetStatus(), PermissionStatus::ASK);
  base::test::TestFuture<PermissionRequestOutcome> future;
  dir_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                               future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(dir_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, GrantType::kRead));

  // Remove the active grant, but not the persisted permission.
  dir_grant.reset();

  // `dir_path` has read permission while we're asking for write permission,
  // so do not auto-grant the permission and do not grant via persisted
  // permission.
  PathInfo file_path(kTestPathInfo.path.AppendASCII("baz"));
  auto file_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, file_path, HandleType::kFile, UserAction::kLoadFromStorage);
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::ASK);
  base::test::TestFuture<PermissionRequestOutcome> future2;
  file_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                                future2.GetCallback());
  EXPECT_EQ(future2.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, file_path, HandleType::kFile, GrantType::kWrite));
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       PersistedPermission_RevokeGrantByFilePath) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  permission_context()->RevokeGrant(kTestOrigin, kTestPathInfo.path);
  auto updated_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kNone);
  EXPECT_EQ(updated_grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kRead));
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));
}

// TODO(crbug.com/40101963): Enable when android persisted permissions are
// implemented.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(ChromeFileSystemAccessPermissionContextTest,
       PersistedPermission_NotAccessibleIfContentSettingBlock) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  permission_context()->RevokeActiveGrantsForTesting(kTestOrigin,
                                                     kTestPathInfo.path);

  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);

  // After the guard is blocked, the permission status for the `grant` should
  // remain unchanged and the persisted permission should not be accessible.
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       PersistedPermission_ReadWriteGrants) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);

  // Grant the write access.
  auto write_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, UserAction::kSave);
  EXPECT_EQ(write_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, GrantType::kWrite));

  // Deny the read access.
  auto read_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, UserAction::kOpen);
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::DENIED);
  base::test::TestFuture<PermissionRequestOutcome> future;
  read_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                                future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kUserDenied);
  EXPECT_EQ(read_grant->GetStatus(), PermissionStatus::DENIED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, GrantType::kRead));

  // Denying read access should not remove write access.
  EXPECT_EQ(write_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, GrantType::kWrite));
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RequestPermission_ClearOutdatedDormantGrants) {
  // If the restore permission prompt is not triggered when requesting
  // permission, dormant grants should be cleared such that old dormant grants
  // are not carried over to the next session.
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  // Create a persisted grant by granting a directory handle.
  auto grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, UserAction::kOpen);
  base::test::TestFuture<PermissionRequestOutcome> future;
  grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                           future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);

  // Navigate away so that the persisted grant becomes dormant.
  // TODO(crbug.com/40101962): Update this test to navigate away from the page,
  // instead of manually resetting the grant.
  permission_context()->RevokeActiveGrantsForTesting(kTestOrigin);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);

  // On a revisit, requesting permission does not trigger the restore permission
  // prompt, as there is no dormant grant.
  base::test::TestFuture<PermissionRequestOutcome> future2;
  grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                           future2.GetCallback());
  EXPECT_NE(future2.Get(), PermissionRequestOutcome::kGrantedByRestorePrompt);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
}

// TODO(crbug.com/40101963): Enable when android persisted permissions are
// implemented.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RequestPermission_Dismissed) {
  base::HistogramTester histograms;
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::DISMISSED);
  content::RenderFrameHostTester::For(web_contents_->GetPrimaryMainFrame())
      ->SimulateUserActivation();

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);
  auto result =
      TriggerRestorePermissionPromptAfterBeingBackgrounded(kTestOrigin);
  EXPECT_EQ(result, PermissionRequestOutcome::kUserDismissed);
  histograms.ExpectBucketCount(
      "Storage.FileSystemAccess.RestorePermissionPromptOutcome",
      RestorePermissionPromptOutcome::kDismissed, 1);

  // The grant status should change as a result of dismissal.
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, RequestPermission_Granted) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);
  content::RenderFrameHostTester::For(web_contents_->GetPrimaryMainFrame())
      ->SimulateUserActivation();

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);

  base::test::TestFuture<PermissionRequestOutcome> future;
  grant->RequestPermission(frame_id(), UserActivationState::kRequired,
                           future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(ChromeFileSystemAccessPermissionContextTest, RequestPermission_Denied) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::DENIED);
  content::RenderFrameHostTester::For(web_contents_->GetPrimaryMainFrame())
      ->SimulateUserActivation();

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);

  base::test::TestFuture<PermissionRequestOutcome> future;
  grant->RequestPermission(frame_id(), UserActivationState::kRequired,
                           future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kUserDenied);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::DENIED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));
}

// TODO(crbug.com/40101963): Enable when android persisted permissions are
// implemented.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RequestPermission_NoUserActivation) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);

  base::test::TestFuture<PermissionRequestOutcome> future;
  grant->RequestPermission(frame_id(), UserActivationState::kRequired,
                           future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kNoUserActivation);
  // The status should not change when there is no user activation.
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RequestPermission_NoUserActivation_UserActivationNotRequired) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);

  base::test::TestFuture<PermissionRequestOutcome> future;
  grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                           future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kUserGranted);
  // The status should not change when there is no user activation.
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RequestPermission_AlreadyGranted) {
  // If the permission has already been granted, a call to `RequestPermission`
  // should call the passed-in callback and return immediately without showing
  // a prompt.
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);

  base::test::TestFuture<PermissionRequestOutcome> future;
  grant->RequestPermission(frame_id(), UserActivationState::kRequired,
                           future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kRequestAborted);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RequestPermission_GlobalGuardBlockedBeforeOpenGrant) {
  // If the guard content setting is blocked, a call to `RequestPermission`
  // should update the `PermissionStatus` to `DENIED`, call the passed-in
  // callback, and return immediately without showing a prompt.
  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);

  base::test::TestFuture<PermissionRequestOutcome> future1;
  grant->RequestPermission(frame_id(), UserActivationState::kRequired,
                           future1.GetCallback());
  EXPECT_EQ(future1.Get(), PermissionRequestOutcome::kRequestAborted);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::DENIED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));

  auto grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin2, kTestPathInfo, HandleType::kFile, UserAction::kOpen);

  base::test::TestFuture<PermissionRequestOutcome> future2;
  grant2->RequestPermission(frame_id(), UserActivationState::kRequired,
                            future2.GetCallback());
  EXPECT_EQ(future2.Get(), PermissionRequestOutcome::kRequestAborted);
  EXPECT_EQ(grant2->GetStatus(), PermissionStatus::DENIED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin2, kTestPathInfo, HandleType::kFile, GrantType::kWrite));

  grant2.reset();
  SetContentSettingValueForOrigin(kTestOrigin2,
                                  ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                  CONTENT_SETTING_ASK);

  grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin2, kTestPathInfo, HandleType::kFile, UserAction::kOpen);

  base::test::TestFuture<PermissionRequestOutcome> future3;
  grant2->RequestPermission(frame_id(), UserActivationState::kRequired,
                            future3.GetCallback());
  EXPECT_EQ(future3.Get(), PermissionRequestOutcome::kNoUserActivation);
  EXPECT_EQ(grant2->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin2, kTestPathInfo, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RequestPermission_GlobalGuardBlockedAfterOpenGrant) {
  // If the guard content setting is blocked, a call to `RequestPermission`
  // should update the `PermissionStatus` to `DENIED`, call the passed-in
  // callback, and return immediately without showing a prompt.
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);
  auto grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin2, kTestPathInfo, HandleType::kFile, UserAction::kOpen);

  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);

  base::test::TestFuture<PermissionRequestOutcome> future1;
  grant->RequestPermission(frame_id(), UserActivationState::kRequired,
                           future1.GetCallback());
  EXPECT_EQ(future1.Get(), PermissionRequestOutcome::kBlockedByContentSetting);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::DENIED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));

  base::test::TestFuture<PermissionRequestOutcome> future2;
  grant2->RequestPermission(frame_id(), UserActivationState::kRequired,
                            future2.GetCallback());
  EXPECT_EQ(future2.Get(), PermissionRequestOutcome::kBlockedByContentSetting);
  EXPECT_EQ(grant2->GetStatus(), PermissionStatus::DENIED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin2, kTestPathInfo, HandleType::kFile, GrantType::kWrite));

  grant.reset();
  grant2.reset();

  SetContentSettingValueForOrigin(kTestOrigin,
                                  ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                  CONTENT_SETTING_ASK);
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);
  grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin2, kTestPathInfo, HandleType::kFile, UserAction::kOpen);

  base::test::TestFuture<PermissionRequestOutcome> future3;
  grant->RequestPermission(frame_id(), UserActivationState::kRequired,
                           future3.GetCallback());
  EXPECT_EQ(future3.Get(), PermissionRequestOutcome::kNoUserActivation);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));

  base::test::TestFuture<PermissionRequestOutcome> future4;
  grant2->RequestPermission(frame_id(), UserActivationState::kRequired,
                            future4.GetCallback());
  EXPECT_EQ(future4.Get(), PermissionRequestOutcome::kRequestAborted);
  EXPECT_EQ(grant2->GetStatus(), PermissionStatus::DENIED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin2, kTestPathInfo, HandleType::kFile, GrantType::kWrite));
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

  // An allowlisted origin automatically gets write permission.
  auto grant1 = permission_context()->GetWritePermissionGrant(
      kChromeOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant1->GetStatus(), PermissionStatus::GRANTED);
  // Permissions are not persisted for allowlisted origins.
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kChromeOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));

  auto grant2 = permission_context()->GetWritePermissionGrant(
      kChromeOrigin, kTestPathInfo, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(grant2->GetStatus(), PermissionStatus::GRANTED);
  // Permissions are not persisted for allowlisted origins.
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kChromeOrigin, kTestPathInfo, HandleType::kDirectory, GrantType::kWrite));

  // The other origin should be blocked.
  auto grant3 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant3->GetStatus(), PermissionStatus::DENIED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));

  auto grant4 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(grant4->GetStatus(), PermissionStatus::DENIED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, GrantType::kWrite));
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
      kChromeOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant1->GetStatus(), PermissionStatus::GRANTED);
  // Permissions are not persisted for allowlisted origins.
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kChromeOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));

  // Existing grant (file).
  auto grant2 = permission_context()->GetWritePermissionGrant(
      kChromeOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant2->GetStatus(), PermissionStatus::GRANTED);

  // Initial grant (directory).
  auto grant3 = permission_context()->GetWritePermissionGrant(
      kChromeOrigin, kTestPathInfo, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(grant3->GetStatus(), PermissionStatus::GRANTED);
  // Permissions are not persisted for allowlisted origins.
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kChromeOrigin, kTestPathInfo, HandleType::kDirectory, GrantType::kWrite));

  // Existing grant (directory).
  auto grant4 = permission_context()->GetWritePermissionGrant(
      kChromeOrigin, kTestPathInfo, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(grant4->GetStatus(), PermissionStatus::GRANTED);
}

// TODO(crbug.com/40101963): Enable when android persisted permissions are
// implemented.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetReadPermissionGrant_FileBecomesDirectory) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto file_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kRead));

  auto directory_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(directory_grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, GrantType::kRead));

  // Requesting a permission grant for a directory which was previously a file
  // revokes the original file permission.
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::DENIED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kRead));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_FileBecomesDirectory) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto file_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));

  auto directory_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(directory_grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, GrantType::kWrite));

  // Requesting a permission grant for a directory which was previously a file
  // revokes the original file permission.
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::DENIED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, NotifyEntryMoved_File) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto file_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));

  const auto new_path =
      PathInfo(kTestPathInfo.path.DirName().AppendASCII("new_name.txt"));
  permission_context()->NotifyEntryMoved(kTestOrigin, kTestPathInfo, new_path);

  // Permissions to the old path are revoked.
  auto file_grant_at_old_path = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(file_grant_at_old_path->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kFile, GrantType::kWrite));

  // Permissions to the new path are updated.
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_EQ(file_grant->GetPath(), new_path.path);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, new_path, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       NotifyEntryMoved_ChildFileObtainedLater) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto parent_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, UserAction::kOpen);
  base::test::TestFuture<PermissionRequestOutcome> future;
  parent_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                                  future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(parent_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, GrantType::kWrite));

  // The child file should inherit write permission from its parent.
  const auto old_file_path_info =
      PathInfo(kTestPathInfo.path.AppendASCII("old_name.txt"));
  auto file_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, old_file_path_info, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, old_file_path_info, HandleType::kFile, GrantType::kWrite));

  const auto new_path =
      PathInfo(old_file_path_info.path.DirName().AppendASCII("new_name.txt"));
  permission_context()->NotifyEntryMoved(kTestOrigin, old_file_path_info,
                                         new_path);

  // Permissions to the parent are not affected.
  auto parent_grant_copy = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(parent_grant_copy->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, GrantType::kWrite));

  // Permissions to the old file path are not affected.
  auto file_grant_at_old_path = permission_context()->GetWritePermissionGrant(
      kTestOrigin, old_file_path_info, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(file_grant_at_old_path->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_EQ(file_grant_at_old_path->GetPath(), old_file_path_info.path);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, old_file_path_info, HandleType::kFile, GrantType::kWrite));

  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_EQ(file_grant->GetPath(), new_path.path);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, new_path, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       NotifyEntryMoved_ChildFileObtainedFirst) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  // Acquire permission to the child file's path.
  const auto old_file_path_info =
      PathInfo(kTestPathInfo.path.AppendASCII("old_name.txt"));
  auto file_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, old_file_path_info, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, old_file_path_info, HandleType::kFile, GrantType::kWrite));

  // Later, acquire permission to the child parent.
  auto parent_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, UserAction::kOpen);
  base::test::TestFuture<PermissionRequestOutcome> future;
  parent_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                                  future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(parent_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, GrantType::kWrite));

  const auto new_path =
      PathInfo(old_file_path_info.path.DirName().AppendASCII("new_name.txt"));
  permission_context()->NotifyEntryMoved(kTestOrigin, old_file_path_info,
                                         new_path);

  // Permissions to the parent are not affected.
  auto parent_grant_copy = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(parent_grant_copy->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPathInfo, HandleType::kDirectory, GrantType::kWrite));

  // Permissions to the old file path are not affected.
  auto file_grant_at_old_path = permission_context()->GetWritePermissionGrant(
      kTestOrigin, old_file_path_info, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(file_grant_at_old_path->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, old_file_path_info, HandleType::kFile, GrantType::kWrite));

  // Permission should still be granted at the new path.
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_EQ(file_grant->GetPath(), new_path.path);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, new_path, HandleType::kFile, GrantType::kWrite));
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ReadGrantDestroyedOnRevokeActiveGrants) {
  auto grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);

  auto observer =
      SelfDestructingPermissionGrantObserver::Create(std::move(grant));

  // `observer` destroys itself when the permission status is changed.
  // `observer` is the only holder of `grant`, so `grant` is destroyed as well.
  // This should work without crashing.
  permission_context()->RevokeActiveGrantsForTesting(kTestOrigin,
                                                     kTestPathInfo.path);
  EXPECT_FALSE(observer);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       WriteGrantDestroyedOnRevokeActiveGrants) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);

  auto observer =
      SelfDestructingPermissionGrantObserver::Create(std::move(grant));

  // `observer` destroys itself when the permission status is changed.
  // `observer` is the only holder of `grant`, so `grant` is destroyed as well.
  // This should work without crashing.
  permission_context()->RevokeActiveGrantsForTesting(kTestOrigin,
                                                     kTestPathInfo.path);
  EXPECT_FALSE(observer);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ReadGrantDestroyedOnRevokeAllActiveGrants) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);

  auto observer =
      SelfDestructingPermissionGrantObserver::Create(std::move(grant));

  // `observer` destroys itself when the permission status is changed.
  // `observer` is the only holder of `grant`, so `grant` is destroyed as well.
  // This should work without crashing.
  permission_context()->RevokeAllActiveGrants();
  EXPECT_FALSE(observer);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       WriteGrantDestroyedOnRevokeAllActiveGrants) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);

  auto observer =
      SelfDestructingPermissionGrantObserver::Create(std::move(grant));

  // `observer` destroys itself when the permission status is changed.
  // `observer` is the only holder of `grant`, so `grant` is destroyed as well.
  // This should work without crashing.
  permission_context()->RevokeAllActiveGrants();
  EXPECT_FALSE(observer);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GrantDestroyedOnRequestingPermission) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kNone);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);

  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);

  auto observer =
      SelfDestructingPermissionGrantObserver::Create(std::move(grant));

  // `observer` destroys itself when the permission status is changed.
  // `observer` is the only holder of `grant`, so `grant` is destroyed as well.
  // This should work without crashing.
  base::test::TestFuture<PermissionRequestOutcome> future;
  observer->grant()->RequestPermission(
      frame_id(), UserActivationState::kNotRequired, future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kBlockedByContentSetting);
  EXPECT_FALSE(observer);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       WriteGrantDestroyedOnGrantingSecondWriteGrant) {
  auto grant1 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kNone);
  EXPECT_EQ(grant1->GetStatus(), PermissionStatus::ASK);

  auto observer =
      SelfDestructingPermissionGrantObserver::Create(std::move(grant1));

  // `observer` destroys itself when the permission status is changed.
  // `observer` is the only holder of `grant`, so `grant` is destroyed as well.
  // This should work without crashing.
  auto grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant2->GetStatus(), PermissionStatus::GRANTED);

  EXPECT_FALSE(observer);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ReadGrantDestroyedOnGrantingSecondReadGrant) {
  auto grant1 = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kNone);
  EXPECT_EQ(grant1->GetStatus(), PermissionStatus::ASK);

  auto observer =
      SelfDestructingPermissionGrantObserver::Create(std::move(grant1));

  // `observer` destroys itself when the permission status is changed.
  // `observer` is the only holder of `grant`, so `grant` is destroyed as well.
  // This should work without crashing.
  auto grant2 = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPathInfo, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant2->GetStatus(), PermissionStatus::GRANTED);

  EXPECT_FALSE(observer);
}

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       CheckPathsAgainstEnterprisePolicy_Empty) {
  EnableEnterpriseAnalysis(profile());
  ContentAnalysisDelegate::SetFactoryForTesting(base::BindRepeating(
      &FakeContentAnalysisDelegate::Create, base::DoNothing(),
      base::BindRepeating(
          [](const std::string& contents, const base::FilePath& path) {
            return FakeContentAnalysisDelegate::SuccessfulResponse({"dlp"});
          }),
      kDummyDmToken));

  std::vector<PathInfo> entries;

  base::test::TestFuture<std::vector<PathInfo>> future;
  permission_context_->CheckPathsAgainstEnterprisePolicy(entries, frame_id(),
                                                         future.GetCallback());
  EXPECT_TRUE(future.Get<0>().empty());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       CheckPathsAgainstEnterprisePolicy_BadFrameId) {
  EnableEnterpriseAnalysis(profile());
  ContentAnalysisDelegate::SetFactoryForTesting(base::BindRepeating(
      &FakeContentAnalysisDelegate::Create, base::DoNothing(),
      base::BindRepeating(
          [](const std::string& contents, const base::FilePath& path) {
            return FakeContentAnalysisDelegate::SuccessfulResponse({"dlp"});
          }),
      kDummyDmToken));

  base::FilePath path = temp_dir_.GetPath().AppendASCII("foo");
  EXPECT_TRUE(CreateNonEmptyFile(path));
  std::vector<PathInfo> entries{
      {PathType::kLocal, path},
  };

  base::test::TestFuture<std::vector<PathInfo>> future;
  permission_context_->CheckPathsAgainstEnterprisePolicy(
      entries, content::GlobalRenderFrameHostId(), future.GetCallback());
  EXPECT_THAT(future.Get<0>(), testing::ElementsAreArray(entries));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       CheckPathsAgainstEnterprisePolicy_OneFile) {
  EnableEnterpriseAnalysis(profile());
  ContentAnalysisDelegate::SetFactoryForTesting(base::BindRepeating(
      &FakeContentAnalysisDelegate::Create, base::DoNothing(),
      base::BindRepeating(
          [](const std::string& contents, const base::FilePath& path) {
            return FakeContentAnalysisDelegate::SuccessfulResponse({"dlp"});
          }),
      kDummyDmToken));

  base::FilePath path = temp_dir_.GetPath().AppendASCII("foo");
  EXPECT_TRUE(CreateNonEmptyFile(path));
  std::vector<PathInfo> entries{
      {PathType::kLocal, path},
  };

  base::test::TestFuture<std::vector<PathInfo>> future;
  permission_context_->CheckPathsAgainstEnterprisePolicy(entries, frame_id(),
                                                         future.GetCallback());
  EXPECT_THAT(future.Get<0>(), testing::ElementsAreArray(entries));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       CheckPathsAgainstEnterprisePolicy_OneDirectoryAllowed) {
  base::FilePath path_foo = temp_dir_.GetPath().AppendASCII("foo");
  base::FilePath path_bar = temp_dir_.GetPath().AppendASCII("bar");
  EXPECT_TRUE(CreateNonEmptyFile(path_foo));
  EXPECT_TRUE(CreateNonEmptyFile(path_bar));

  // A directory is allowed if all contained files are allowed.
  EnableEnterpriseAnalysis(profile());
  ContentAnalysisDelegate::SetFactoryForTesting(base::BindRepeating(
      &FakeContentAnalysisDelegate::Create, base::DoNothing(),
      base::BindLambdaForTesting(
          [path_foo](const std::string& contents, const base::FilePath& path) {
            return FakeContentAnalysisDelegate::SuccessfulResponse({"dlp"});
          }),
      kDummyDmToken));

  std::vector<PathInfo> entries{
      {PathType::kLocal, temp_dir_.GetPath()},
  };

  base::test::TestFuture<std::vector<PathInfo>> future;
  permission_context_->CheckPathsAgainstEnterprisePolicy(entries, frame_id(),
                                                         future.GetCallback());
  EXPECT_THAT(future.Get<0>(), testing::SizeIs(1));
  EXPECT_EQ(future.Get<0>()[0].path, temp_dir_.GetPath());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       CheckPathsAgainstEnterprisePolicy_OneDirectoryBlocked) {
  base::FilePath path_foo = temp_dir_.GetPath().AppendASCII("foo");
  base::FilePath path_bar = temp_dir_.GetPath().AppendASCII("bar");
  EXPECT_TRUE(CreateNonEmptyFile(path_foo));
  EXPECT_TRUE(CreateNonEmptyFile(path_bar));

  // A directory is blocked if at least one file is blocked.
  EnableEnterpriseAnalysis(profile());
  ContentAnalysisDelegate::SetFactoryForTesting(base::BindRepeating(
      &FakeContentAnalysisDelegate::Create, base::DoNothing(),
      base::BindLambdaForTesting([path_foo](const std::string& contents,
                                            const base::FilePath& path) {
        return path == path_foo
                   ? FakeContentAnalysisDelegate::SuccessfulResponse({"dlp"})
                   : FakeContentAnalysisDelegate::DlpResponse(
                         ContentAnalysisResponse::Result::SUCCESS, "rule",
                         ContentAnalysisResponse::Result::TriggeredRule::BLOCK);
      }),
      kDummyDmToken));

  std::vector<PathInfo> entries{
      {PathType::kLocal, temp_dir_.GetPath()},
  };

  base::test::TestFuture<std::vector<PathInfo>> future;
  permission_context_->CheckPathsAgainstEnterprisePolicy(entries, frame_id(),
                                                         future.GetCallback());
  EXPECT_THAT(future.Get<0>(), testing::SizeIs(0));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       CheckPathsAgainstEnterprisePolicy_TwoFilesOneBlocked) {
  base::FilePath path_foo = temp_dir_.GetPath().AppendASCII("foo");
  base::FilePath path_bar = temp_dir_.GetPath().AppendASCII("bar");
  EXPECT_TRUE(CreateNonEmptyFile(path_foo));
  EXPECT_TRUE(CreateNonEmptyFile(path_bar));

  EnableEnterpriseAnalysis(profile());
  ContentAnalysisDelegate::SetFactoryForTesting(base::BindRepeating(
      &FakeContentAnalysisDelegate::Create, base::DoNothing(),
      base::BindLambdaForTesting([path_foo](const std::string& contents,
                                            const base::FilePath& path) {
        return path == path_foo
                   ? FakeContentAnalysisDelegate::SuccessfulResponse({"dlp"})
                   : FakeContentAnalysisDelegate::DlpResponse(
                         ContentAnalysisResponse::Result::SUCCESS, "rule",
                         ContentAnalysisResponse::Result::TriggeredRule::BLOCK);
      }),
      kDummyDmToken));

  std::vector<PathInfo> entries{
      {PathType::kLocal, path_foo},
      {PathType::kLocal, path_bar},
  };

  base::test::TestFuture<std::vector<PathInfo>> future;
  permission_context_->CheckPathsAgainstEnterprisePolicy(entries, frame_id(),
                                                         future.GetCallback());
  EXPECT_THAT(future.Get<0>(), testing::SizeIs(1));
  EXPECT_EQ(future.Get<0>()[0].path, path_foo);
}

#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
