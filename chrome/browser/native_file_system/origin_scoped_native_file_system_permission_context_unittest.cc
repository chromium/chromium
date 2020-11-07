// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/native_file_system/origin_scoped_native_file_system_permission_context.h"

#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_path_override.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/native_file_system/native_file_system_permission_request_manager.h"
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
#include "ui/webui/webui_allowlist.h"
#include "url/gurl.h"
#include "url/origin.h"

using content::BrowserContext;
using content::WebContents;
using content::WebContentsTester;
using permissions::PermissionAction;
using UserAction = ChromeNativeFileSystemPermissionContext::UserAction;
using PermissionStatus =
    content::NativeFileSystemPermissionGrant::PermissionStatus;
using PermissionRequestOutcome =
    content::NativeFileSystemPermissionGrant::PermissionRequestOutcome;
using SensitiveDirectoryResult =
    ChromeNativeFileSystemPermissionContext::SensitiveDirectoryResult;
using HandleType = content::NativeFileSystemPermissionContext::HandleType;
using UserActivationState =
    content::NativeFileSystemPermissionGrant::UserActivationState;

class OriginScopedNativeFileSystemPermissionContextTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    NativeFileSystemPermissionRequestManager::CreateForWebContents(
        web_contents_.get());
    content::WebContentsTester::For(web_contents_.get())
        ->NavigateAndCommit(kTestOrigin.GetURL());

    NativeFileSystemPermissionRequestManager::FromWebContents(
        web_contents_.get())
        ->set_auto_response_for_test(PermissionAction::DISMISSED);
    permission_context_ =
        std::make_unique<OriginScopedNativeFileSystemPermissionContext>(
            browser_context());
  }

  void TearDown() override {
    ASSERT_TRUE(temp_dir_.Delete());
    web_contents_.reset();
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

  OriginScopedNativeFileSystemPermissionContext* permission_context() {
    return permission_context_.get();
  }
  BrowserContext* browser_context() { return &profile_; }
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
  const base::FilePath kTestPath =
      base::FilePath(FILE_PATH_LITERAL("/foo/bar"));
  const url::Origin kChromeOrigin =
      url::Origin::Create(GURL("chrome://test-origin"));

  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<OriginScopedNativeFileSystemPermissionContext>
      permission_context_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  TestingProfile profile_;
  std::unique_ptr<WebContents> web_contents_;
};

#if !defined(OS_ANDROID)

TEST_F(OriginScopedNativeFileSystemPermissionContextTest,
       GetReadPermissionGrant_InitialState_LoadFromStorage) {
  auto grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kLoadFromStorage);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
}

TEST_F(OriginScopedNativeFileSystemPermissionContextTest,
       GetReadPermissionGrant_InitialState_Open_File) {
  auto grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
}

TEST_F(OriginScopedNativeFileSystemPermissionContextTest,
       GetReadPermissionGrant_InitialState_Open_Directory) {
  auto grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
}

TEST_F(OriginScopedNativeFileSystemPermissionContextTest,
       GetWritePermissionGrant_InitialState_LoadFromStorage) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kLoadFromStorage);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
}

TEST_F(OriginScopedNativeFileSystemPermissionContextTest,
       GetWritePermissionGrant_InitialState_Open_File) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
}

TEST_F(OriginScopedNativeFileSystemPermissionContextTest,
       GetWritePermissionGrant_InitialState_Open_Directory) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
}

TEST_F(OriginScopedNativeFileSystemPermissionContextTest,
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

TEST_F(OriginScopedNativeFileSystemPermissionContextTest,
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

TEST_F(OriginScopedNativeFileSystemPermissionContextTest,
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

TEST_F(OriginScopedNativeFileSystemPermissionContextTest,
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
    OriginScopedNativeFileSystemPermissionContextTest,
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
    OriginScopedNativeFileSystemPermissionContextTest,
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
    OriginScopedNativeFileSystemPermissionContextTest,
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
    OriginScopedNativeFileSystemPermissionContextTest,
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

TEST_F(OriginScopedNativeFileSystemPermissionContextTest,
       RequestPermission_Dismissed) {
  NativeFileSystemPermissionRequestManager::FromWebContents(web_contents_.get())
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

TEST_F(OriginScopedNativeFileSystemPermissionContextTest,
       RequestPermission_Granted) {
  NativeFileSystemPermissionRequestManager::FromWebContents(web_contents_.get())
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

TEST_F(OriginScopedNativeFileSystemPermissionContextTest,
       RequestPermission_Denied) {
  NativeFileSystemPermissionRequestManager::FromWebContents(web_contents_.get())
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

TEST_F(OriginScopedNativeFileSystemPermissionContextTest,
       RequestPermission_NoUserActivation) {
  NativeFileSystemPermissionRequestManager::FromWebContents(web_contents_.get())
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

TEST_F(OriginScopedNativeFileSystemPermissionContextTest,
       RequestPermission_NoUserActivation_UserActivationNotRequired) {
  NativeFileSystemPermissionRequestManager::FromWebContents(web_contents_.get())
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

TEST_F(OriginScopedNativeFileSystemPermissionContextTest,
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

TEST_F(OriginScopedNativeFileSystemPermissionContextTest,
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

TEST_F(OriginScopedNativeFileSystemPermissionContextTest,
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

TEST_F(OriginScopedNativeFileSystemPermissionContextTest,
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

TEST_F(OriginScopedNativeFileSystemPermissionContextTest,
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

TEST_F(OriginScopedNativeFileSystemPermissionContextTest,
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

TEST_F(OriginScopedNativeFileSystemPermissionContextTest,
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
