// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/native_file_system/chrome_native_file_system_permission_context.h"

#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_path_override.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using content::BrowserContext;
using content::WebContents;
using content::WebContentsTester;
using UserAction = ChromeNativeFileSystemPermissionContext::UserAction;
using PermissionStatus =
    content::NativeFileSystemPermissionGrant::PermissionStatus;
using PermissionRequestOutcome =
    content::NativeFileSystemPermissionGrant::PermissionRequestOutcome;
using SensitiveDirectoryResult =
    ChromeNativeFileSystemPermissionContext::SensitiveDirectoryResult;

class ChromeNativeFileSystemPermissionContextTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    permission_context_ =
        std::make_unique<ChromeNativeFileSystemPermissionContext>(
            browser_context());
  }

  void TearDown() override {
    ASSERT_TRUE(temp_dir_.Delete());
    web_contents_.reset();
  }

  SensitiveDirectoryResult ConfirmSensitiveDirectoryAccessSync(
      ChromeNativeFileSystemPermissionContext* context,
      const std::vector<base::FilePath>& paths) {
    base::RunLoop loop;
    SensitiveDirectoryResult out_result;
    permission_context_->ConfirmSensitiveDirectoryAccess(
        kTestOrigin, paths, /*is_directory=*/false, /*process_id=*/0,
        /*frame_id=*/0,
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
        origin.GetURL(), origin.GetURL(), type,
        /*resource_identifier=*/std::string(), value);
  }

  ChromeNativeFileSystemPermissionContext* permission_context() {
    return permission_context_.get();
  }
  BrowserContext* browser_context() { return &profile_; }
  WebContents* web_contents() { return web_contents_.get(); }

  int process_id() {
    return web_contents()->GetMainFrame()->GetProcess()->GetID();
  }

  int frame_id() { return web_contents()->GetMainFrame()->GetRoutingID(); }

  void ExpectCanRequestWritePermission(
      content::NativeFileSystemPermissionGrant* actual_grant,
      bool expected) {
    auto* grant = static_cast<
        ChromeNativeFileSystemPermissionContext::WritePermissionGrantImpl*>(
        actual_grant);
    EXPECT_EQ(expected, grant->CanRequestPermission());
  }

 protected:
  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kTestOrigin2 =
      url::Origin::Create(GURL("https://test.com"));
  const base::FilePath kTestPath =
      base::FilePath(FILE_PATH_LITERAL("/foo/bar"));

  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ChromeNativeFileSystemPermissionContext> permission_context_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  TestingProfile profile_;
  std::unique_ptr<WebContents> web_contents_;
};

#if !defined(OS_ANDROID)
TEST_F(ChromeNativeFileSystemPermissionContextTest,
       GetWritePermissionGrant_InitialState_OpenAction) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, process_id(), frame_id(),
      UserAction::kOpen);
  ExpectCanRequestWritePermission(grant.get(), /*expected=*/true);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       GetWritePermissionGrant_InitialState_WritableImplicitState) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, process_id(), frame_id(),
      UserAction::kSave);
  ExpectCanRequestWritePermission(grant.get(), /*expected=*/true);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());

  // The existing grant should not change if the permission is blocked globally.
  SetDefaultContentSettingValue(
      ContentSettingsType::NATIVE_FILE_SYSTEM_WRITE_GUARD,
      CONTENT_SETTING_BLOCK);
  ExpectCanRequestWritePermission(grant.get(), /*expected=*/false);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       GetWritePermissionGrant_WriteGrantedChangesExistingGrant) {
  auto grant1 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, process_id(), frame_id(),
      UserAction::kOpen);
  auto grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, process_id(), frame_id(),
      UserAction::kSave);
  auto grant3 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, process_id(), frame_id(),
      UserAction::kOpen);
  // All grants should be the same grant, and be granted.
  EXPECT_EQ(grant1, grant2);
  EXPECT_EQ(grant1, grant3);
  ExpectCanRequestWritePermission(grant1.get(), /*expected=*/true);
  EXPECT_EQ(PermissionStatus::GRANTED, grant1->GetStatus());
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       GetWritePermissionGrant_GrantIsRevokedWhenNoLongerUsed) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, process_id(), frame_id(),
      UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
  grant.reset();

  // After reset grant should go away, so new grant request should be in ASK
  // state.
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, process_id(), frame_id(),
      UserAction::kOpen);
  ExpectCanRequestWritePermission(grant.get(), /*expected=*/true);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       GetWritePermissionGrant_InitialState_OpenAction_GlobalGuardBlocked) {
  SetDefaultContentSettingValue(
      ContentSettingsType::NATIVE_FILE_SYSTEM_WRITE_GUARD,
      CONTENT_SETTING_BLOCK);

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, process_id(), frame_id(),
      UserAction::kOpen);
  ExpectCanRequestWritePermission(grant.get(), /*expected=*/false);
  EXPECT_EQ(PermissionStatus::DENIED, grant->GetStatus());
  grant.reset();

  SetContentSettingValueForOrigin(
      kTestOrigin, ContentSettingsType::NATIVE_FILE_SYSTEM_WRITE_GUARD,
      CONTENT_SETTING_ASK);

  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, process_id(), frame_id(),
      UserAction::kOpen);
  ExpectCanRequestWritePermission(grant.get(), /*expected=*/true);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
}

TEST_F(
    ChromeNativeFileSystemPermissionContextTest,
    GetWritePermissionGrant_InitialState_WritableImplicitState_GlobalGuardBlocked) {
  SetDefaultContentSettingValue(
      ContentSettingsType::NATIVE_FILE_SYSTEM_WRITE_GUARD,
      CONTENT_SETTING_BLOCK);

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, process_id(), frame_id(),
      UserAction::kSave);
  ExpectCanRequestWritePermission(grant.get(), /*expected=*/false);
  EXPECT_EQ(PermissionStatus::DENIED, grant->GetStatus());
  grant.reset();

  SetContentSettingValueForOrigin(
      kTestOrigin, ContentSettingsType::NATIVE_FILE_SYSTEM_WRITE_GUARD,
      CONTENT_SETTING_ASK);

  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, process_id(), frame_id(),
      UserAction::kSave);
  ExpectCanRequestWritePermission(grant.get(), /*expected=*/true);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
}

TEST_F(
    ChromeNativeFileSystemPermissionContextTest,
    GetWritePermissionGrant_WriteGrantedChangesExistingGrant_GlobalGuardBlocked) {
  SetContentSettingValueForOrigin(
      kTestOrigin, ContentSettingsType::NATIVE_FILE_SYSTEM_WRITE_GUARD,
      CONTENT_SETTING_BLOCK);

  auto grant1 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, process_id(), frame_id(),
      UserAction::kOpen);
  auto grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, process_id(), frame_id(),
      UserAction::kSave);
  auto grant3 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, process_id(), frame_id(),
      UserAction::kOpen);
  // All grants should be the same grant, and be denied.
  EXPECT_EQ(grant1, grant2);
  EXPECT_EQ(grant1, grant3);
  ExpectCanRequestWritePermission(grant1.get(), /*expected=*/false);
  EXPECT_EQ(PermissionStatus::DENIED, grant1->GetStatus());
}

TEST_F(
    ChromeNativeFileSystemPermissionContextTest,
    GetWritePermissionGrant_GrantIsRevokedWhenNoLongerUsed_GlobalGuardBlockedBeforeNewGrant) {
  SetDefaultContentSettingValue(
      ContentSettingsType::NATIVE_FILE_SYSTEM_WRITE_GUARD,
      CONTENT_SETTING_BLOCK);

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, process_id(), frame_id(),
      UserAction::kSave);
  ExpectCanRequestWritePermission(grant.get(), /*expected=*/false);
  EXPECT_EQ(PermissionStatus::DENIED, grant->GetStatus());
  grant.reset();

  // After reset grant should go away, but the new grant request should be in
  // DENIED state.
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, process_id(), frame_id(),
      UserAction::kOpen);
  ExpectCanRequestWritePermission(grant.get(), /*expected=*/false);
  EXPECT_EQ(PermissionStatus::DENIED, grant->GetStatus());
}

TEST_F(
    ChromeNativeFileSystemPermissionContextTest,
    GetWritePermissionGrant_GrantIsRevokedWhenNoLongerUsed_GlobalGuardBlockedAfterNewGrant) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, process_id(), frame_id(),
      UserAction::kSave);
  ExpectCanRequestWritePermission(grant.get(), /*expected=*/true);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
  grant.reset();

  // After reset grant should go away, but the new grant request should be in
  // ASK state.
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, process_id(), frame_id(),
      UserAction::kOpen);
  ExpectCanRequestWritePermission(grant.get(), /*expected=*/true);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());

  SetDefaultContentSettingValue(
      ContentSettingsType::NATIVE_FILE_SYSTEM_WRITE_GUARD,
      CONTENT_SETTING_BLOCK);

  // After the guard is blocked, the permission status for |grant| should remain
  // unchanged, but |CanRequestPermission()| should return false.
  ExpectCanRequestWritePermission(grant.get(), /*expected=*/false);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       ConfirmSensitiveDirectoryAccess_NoSpecialPath) {
  const base::FilePath kTestPath =
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
      base::FilePath(FILE_PATH_LITERAL("c:\\foo\\bar"));
#else
      base::FilePath(FILE_PATH_LITERAL("/foo/bar"));
#endif

  // Path outside any special directories should be allowed.
  EXPECT_EQ(
      SensitiveDirectoryResult::kAllowed,
      ConfirmSensitiveDirectoryAccessSync(permission_context(), {kTestPath}));

  // Empty set of paths should also be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAllowed,
            ConfirmSensitiveDirectoryAccessSync(permission_context(), {}));
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       ConfirmSensitiveDirectoryAccess_DontBlockAllChildren) {
  base::FilePath home_dir = temp_dir_.GetPath().AppendASCII("home");
  base::ScopedPathOverride home_override(base::DIR_HOME, home_dir, true, true);

  // Home directory itself should not be allowed.
  EXPECT_EQ(
      SensitiveDirectoryResult::kAbort,
      ConfirmSensitiveDirectoryAccessSync(permission_context(), {home_dir}));
  // Parent of home directory should also not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(permission_context(),
                                                {temp_dir_.GetPath()}));
  // Paths inside home directory should be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAllowed,
            ConfirmSensitiveDirectoryAccessSync(permission_context(),
                                                {home_dir.AppendASCII("foo")}));
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       ConfirmSensitiveDirectoryAccess_BlockAllChildren) {
  base::FilePath app_dir = temp_dir_.GetPath().AppendASCII("app");
  base::ScopedPathOverride app_override(chrome::DIR_APP, app_dir, true, true);

  // App directory itself should not be allowed.
  EXPECT_EQ(
      SensitiveDirectoryResult::kAbort,
      ConfirmSensitiveDirectoryAccessSync(permission_context(), {app_dir}));
  // Parent of App directory should also not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(permission_context(),
                                                {temp_dir_.GetPath()}));
  // Paths inside App directory should also not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(permission_context(),
                                                {app_dir.AppendASCII("foo")}));
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
                                                {user_data_dir}));
  // Parent of User Data directory should also not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(permission_context(),
                                                {temp_dir_.GetPath()}));
  // The nested Download directory itself should not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(permission_context(),
                                                {download_dir}));
  // Paths inside the nested Download directory should be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAllowed,
            ConfirmSensitiveDirectoryAccessSync(
                permission_context(), {download_dir.AppendASCII("foo")}));
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       ConfirmSensitiveDirectoryAccess_RelativePathBlock) {
  base::FilePath home_dir = temp_dir_.GetPath().AppendASCII("home");
  base::ScopedPathOverride home_override(base::DIR_HOME, home_dir, true, true);

  // ~/.ssh should be blocked
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(
                permission_context(), {home_dir.AppendASCII(".ssh")}));
  // And anything inside ~/.ssh should also be blocked
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(
                permission_context(), {home_dir.AppendASCII(".ssh/id_rsa")}));
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       ConfirmSensitiveDirectoryAccess_ExplicitPathBlock) {
// Linux is the only OS where we have some blocked directories with explicit
// paths (as opposed to PathService provided paths).
#if defined(OS_LINUX)
  // /dev should be blocked.
  EXPECT_EQ(
      SensitiveDirectoryResult::kAbort,
      ConfirmSensitiveDirectoryAccessSync(
          permission_context(), {base::FilePath(FILE_PATH_LITERAL("/dev"))}));
  // As well as children of /dev.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(
                permission_context(),
                {base::FilePath(FILE_PATH_LITERAL("/dev/foo"))}));
#endif
}

TEST_F(ChromeNativeFileSystemPermissionContextTest, RequestPermission) {
  // The test environment auto-dismisses prompts, as a result, a call to
  // RequestPermission() should not change PermissionStatus.
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, process_id(), frame_id(),
      UserAction::kOpen);

  base::RunLoop loop;
  grant->RequestPermission(
      process_id(), frame_id(),
      base::BindLambdaForTesting(
          [&](PermissionRequestOutcome outcome) { loop.Quit(); }));
  loop.Run();
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       RequestPermission_AlreadyGranted) {
  // If the permission has already been granted, a call to RequestPermission()
  // should call the passed-in callback and return immediately without showing a
  // prompt.
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, process_id(), frame_id(),
      UserAction::kSave);

  base::RunLoop loop;
  grant->RequestPermission(
      process_id(), frame_id(),
      base::BindLambdaForTesting(
          [&](PermissionRequestOutcome outcome) { loop.Quit(); }));
  loop.Run();
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       RequestPermission_GlobalGuardBlockedBeforeOpenGrant) {
  // If the guard content setting is blocked, a call to RequestPermission()
  // should update the PermissionStatus to DENIED, call the passed-in
  // callback, and return immediately without showing a prompt.
  SetDefaultContentSettingValue(
      ContentSettingsType::NATIVE_FILE_SYSTEM_WRITE_GUARD,
      CONTENT_SETTING_BLOCK);

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, process_id(), frame_id(),
      UserAction::kOpen);

  base::RunLoop loop;
  grant->RequestPermission(
      process_id(), frame_id(),
      base::BindLambdaForTesting(
          [&](PermissionRequestOutcome outcome) { loop.Quit(); }));
  loop.Run();
  EXPECT_EQ(PermissionStatus::DENIED, grant->GetStatus());

  auto grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin2, kTestPath, /*is_directory=*/false, process_id(), frame_id(),
      UserAction::kOpen);

  base::RunLoop loop2;
  grant2->RequestPermission(
      process_id(), frame_id(),
      base::BindLambdaForTesting(
          [&](PermissionRequestOutcome outcome) { loop2.Quit(); }));
  loop2.Run();
  EXPECT_EQ(PermissionStatus::DENIED, grant2->GetStatus());

  grant2.reset();
  SetContentSettingValueForOrigin(
      kTestOrigin2, ContentSettingsType::NATIVE_FILE_SYSTEM_WRITE_GUARD,
      CONTENT_SETTING_ASK);

  grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin2, kTestPath, /*is_directory=*/false, process_id(), frame_id(),
      UserAction::kOpen);

  base::RunLoop loop3;
  grant2->RequestPermission(
      process_id(), frame_id(),
      base::BindLambdaForTesting(
          [&](PermissionRequestOutcome outcome) { loop3.Quit(); }));
  loop3.Run();
  EXPECT_EQ(PermissionStatus::ASK, grant2->GetStatus());
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       RequestPermission_GlobalGuardBlockedAfterOpenGrant) {
  // If the guard content setting is blocked, a call to RequestPermission()
  // should update the PermissionStatus to DENIED, call the passed-in
  // callback, and return immediately without showing a prompt.
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, process_id(), frame_id(),
      UserAction::kOpen);
  auto grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin2, kTestPath, /*is_directory=*/false, process_id(), frame_id(),
      UserAction::kOpen);

  SetDefaultContentSettingValue(
      ContentSettingsType::NATIVE_FILE_SYSTEM_WRITE_GUARD,
      CONTENT_SETTING_BLOCK);

  base::RunLoop loop;
  grant->RequestPermission(
      process_id(), frame_id(),
      base::BindLambdaForTesting(
          [&](PermissionRequestOutcome outcome) { loop.Quit(); }));
  loop.Run();
  EXPECT_EQ(PermissionStatus::DENIED, grant->GetStatus());

  base::RunLoop loop2;
  grant2->RequestPermission(
      process_id(), frame_id(),
      base::BindLambdaForTesting(
          [&](PermissionRequestOutcome outcome) { loop2.Quit(); }));
  loop2.Run();
  EXPECT_EQ(PermissionStatus::DENIED, grant2->GetStatus());

  grant.reset();
  grant2.reset();

  SetContentSettingValueForOrigin(
      kTestOrigin, ContentSettingsType::NATIVE_FILE_SYSTEM_WRITE_GUARD,
      CONTENT_SETTING_ASK);
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, process_id(), frame_id(),
      UserAction::kOpen);
  grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin2, kTestPath, /*is_directory=*/false, process_id(), frame_id(),
      UserAction::kOpen);

  base::RunLoop loop3;
  grant->RequestPermission(
      process_id(), frame_id(),
      base::BindLambdaForTesting(
          [&](PermissionRequestOutcome outcome) { loop3.Quit(); }));
  loop3.Run();
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());

  base::RunLoop loop4;
  grant2->RequestPermission(
      process_id(), frame_id(),
      base::BindLambdaForTesting(
          [&](PermissionRequestOutcome outcome) { loop4.Quit(); }));
  loop4.Run();
  EXPECT_EQ(PermissionStatus::DENIED, grant2->GetStatus());
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       CanRequestWritePermission_Allowed) {
  bool expected = permission_context()->CanRequestWritePermission(kTestOrigin);
  EXPECT_EQ(true, expected);
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       CanRequestWritePermission_ContentSettingsBlock) {
  SetDefaultContentSettingValue(
      ContentSettingsType::NATIVE_FILE_SYSTEM_WRITE_GUARD,
      CONTENT_SETTING_BLOCK);
  bool expected = permission_context()->CanRequestWritePermission(kTestOrigin);
  EXPECT_EQ(false, expected);
}

#endif  // !defined(OS_ANDROID)
