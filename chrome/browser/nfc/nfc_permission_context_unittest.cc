// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nfc/nfc_permission_context.h"

#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_utils.h"

#if defined(OS_ANDROID)
#include "chrome/browser/nfc/nfc_permission_context_android.h"
#include "components/permissions/android/nfc/mock_nfc_system_level_setting.h"

using permissions::MockNfcSystemLevelSetting;
#endif

using content::MockRenderProcessHost;

// NfcPermissionContextTests ------------------------------------------

class NfcPermissionContextTests : public ChromeRenderViewHostTestHarness {
 protected:
  // ChromeRenderViewHostTestHarness:
  void SetUp() override;
  void TearDown() override;

  permissions::PermissionRequestID RequestID(int request_id);

  void RequestNfcPermission(content::WebContents* web_contents,
                            const permissions::PermissionRequestID& id,
                            const GURL& requesting_frame,
                            bool user_gesture);

  void PermissionResponse(const permissions::PermissionRequestID& id,
                          ContentSetting content_setting);
  void CheckPermissionMessageSent(int request_id, bool allowed);
  void CheckPermissionMessageSentInternal(MockRenderProcessHost* process,
                                          int request_id,
                                          bool allowed);
  void SetupRequestManager(content::WebContents* web_contents);
  void RequestManagerDocumentLoadCompleted();
  void RequestManagerDocumentLoadCompleted(content::WebContents* web_contents);
  ContentSetting GetNfcContentSetting(GURL frame_0, GURL frame_1);
  void SetNfcContentSetting(GURL frame_0,
                            GURL frame_1,
                            ContentSetting content_setting);
  bool HasActivePrompt();
  bool HasActivePrompt(content::WebContents* web_contents);
  void AcceptPrompt();
  void AcceptPrompt(content::WebContents* web_contents);
  void DenyPrompt();
  void ClosePrompt();

  // owned by the browser context
  NfcPermissionContext* nfc_permission_context_;
  std::vector<std::unique_ptr<permissions::MockPermissionPromptFactory>>
      mock_permission_prompt_factories_;

  // A map between renderer child id and a pair represending the bridge id and
  // whether the requested permission was allowed.
  std::map<int, std::pair<int, bool>> responses_;
};

permissions::PermissionRequestID NfcPermissionContextTests::RequestID(
    int request_id) {
  return permissions::PermissionRequestID(
      web_contents()->GetMainFrame()->GetProcess()->GetID(),
      web_contents()->GetMainFrame()->GetRoutingID(), request_id);
}

void NfcPermissionContextTests::RequestNfcPermission(
    content::WebContents* web_contents,
    const permissions::PermissionRequestID& id,
    const GURL& requesting_frame,
    bool user_gesture) {
  nfc_permission_context_->RequestPermission(
      web_contents, id, requesting_frame, user_gesture,
      base::BindOnce(&NfcPermissionContextTests::PermissionResponse,
                     base::Unretained(this), id));
  content::RunAllTasksUntilIdle();
}

void NfcPermissionContextTests::PermissionResponse(
    const permissions::PermissionRequestID& id,
    ContentSetting content_setting) {
  responses_[id.render_process_id()] =
      std::make_pair(id.request_id(), content_setting == CONTENT_SETTING_ALLOW);
}

void NfcPermissionContextTests::CheckPermissionMessageSent(int request_id,
                                                           bool allowed) {
  CheckPermissionMessageSentInternal(process(), request_id, allowed);
}

void NfcPermissionContextTests::CheckPermissionMessageSentInternal(
    MockRenderProcessHost* process,
    int request_id,
    bool allowed) {
  ASSERT_EQ(responses_.count(process->GetID()), 1U);
  EXPECT_EQ(request_id, responses_[process->GetID()].first);
  EXPECT_EQ(allowed, responses_[process->GetID()].second);
  responses_.erase(process->GetID());
}

void NfcPermissionContextTests::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  content_settings::PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<chrome::PageSpecificContentSettingsDelegate>(
          web_contents()));
  nfc_permission_context_ = static_cast<NfcPermissionContext*>(
      PermissionManagerFactory::GetForProfile(profile())
          ->GetPermissionContextForTesting(ContentSettingsType::NFC));
  SetupRequestManager(web_contents());

#if defined(OS_ANDROID)
  static_cast<NfcPermissionContextAndroid*>(nfc_permission_context_)
      ->set_nfc_system_level_setting_for_testing(
          std::unique_ptr<permissions::NfcSystemLevelSetting>(
              new MockNfcSystemLevelSetting()));
  MockNfcSystemLevelSetting::SetNfcSystemLevelSettingEnabled(true);
  MockNfcSystemLevelSetting::SetNfcAccessIsPossible(true);
  MockNfcSystemLevelSetting::ClearHasShownNfcSettingPrompt();
#endif
}

void NfcPermissionContextTests::TearDown() {
  mock_permission_prompt_factories_.clear();
  ChromeRenderViewHostTestHarness::TearDown();
}

void NfcPermissionContextTests::SetupRequestManager(
    content::WebContents* web_contents) {
  // Create PermissionRequestManager.
  permissions::PermissionRequestManager::CreateForWebContents(web_contents);
  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);

  // Create a MockPermissionPromptFactory for the PermissionRequestManager.
  mock_permission_prompt_factories_.push_back(
      std::make_unique<permissions::MockPermissionPromptFactory>(
          permission_request_manager));
}

void NfcPermissionContextTests::RequestManagerDocumentLoadCompleted() {
  NfcPermissionContextTests::RequestManagerDocumentLoadCompleted(
      web_contents());
}

void NfcPermissionContextTests::RequestManagerDocumentLoadCompleted(
    content::WebContents* web_contents) {
  permissions::PermissionRequestManager::FromWebContents(web_contents)
      ->DocumentOnLoadCompletedInMainFrame();
}

ContentSetting NfcPermissionContextTests::GetNfcContentSetting(GURL frame_0,
                                                               GURL frame_1) {
  return HostContentSettingsMapFactory::GetForProfile(profile())
      ->GetContentSetting(frame_0, frame_1, ContentSettingsType::NFC);
}

void NfcPermissionContextTests::SetNfcContentSetting(
    GURL frame_0,
    GURL frame_1,
    ContentSetting content_setting) {
  return HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetContentSettingDefaultScope(
          frame_0, frame_1, ContentSettingsType::NFC, content_setting);
}

bool NfcPermissionContextTests::HasActivePrompt() {
  return HasActivePrompt(web_contents());
}

bool NfcPermissionContextTests::HasActivePrompt(
    content::WebContents* web_contents) {
  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  return manager->IsRequestInProgress();
}

void NfcPermissionContextTests::AcceptPrompt() {
  return AcceptPrompt(web_contents());
}

void NfcPermissionContextTests::AcceptPrompt(
    content::WebContents* web_contents) {
  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  manager->Accept();
  base::RunLoop().RunUntilIdle();
}

void NfcPermissionContextTests::DenyPrompt() {
  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());
  manager->Deny();
  base::RunLoop().RunUntilIdle();
}

void NfcPermissionContextTests::ClosePrompt() {
  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());
  manager->Closing();
  base::RunLoop().RunUntilIdle();
}

// Tests ----------------------------------------------------------------------

TEST_F(NfcPermissionContextTests, SinglePermissionPrompt) {
  GURL requesting_frame("https://www.example.com/nfc");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();

  EXPECT_FALSE(HasActivePrompt());
  RequestNfcPermission(web_contents(), RequestID(0), requesting_frame,
                       true /* user_gesture */);

#if defined(OS_ANDROID)
  ASSERT_TRUE(HasActivePrompt());
#else
  ASSERT_FALSE(HasActivePrompt());
#endif
}

TEST_F(NfcPermissionContextTests, SinglePermissionPromptFailsOnInsecureOrigin) {
  GURL requesting_frame("http://www.example.com/nfc");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();

  EXPECT_FALSE(HasActivePrompt());
  RequestNfcPermission(web_contents(), RequestID(0), requesting_frame, true);
  ASSERT_FALSE(HasActivePrompt());
}

#if defined(OS_ANDROID)
// Tests concerning Android NFC setting
TEST_F(NfcPermissionContextTests,
       SystemNfcSettingDisabledWhenNfcPermissionGetsGranted) {
  GURL requesting_frame("https://www.example.com/nfc");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  MockNfcSystemLevelSetting::SetNfcSystemLevelSettingEnabled(false);
  EXPECT_FALSE(HasActivePrompt());
  RequestNfcPermission(web_contents(), RequestID(0), requesting_frame, true);
  ASSERT_TRUE(HasActivePrompt());
  ASSERT_FALSE(MockNfcSystemLevelSetting::HasShownNfcSettingPrompt());
  AcceptPrompt();
  ASSERT_TRUE(MockNfcSystemLevelSetting::HasShownNfcSettingPrompt());
  CheckPermissionMessageSent(0 /* request _id */, true /* allowed */);
}

TEST_F(NfcPermissionContextTests,
       SystemNfcSettingDisabledWhenNfcPermissionGetsDenied) {
  GURL requesting_frame("https://www.example.com/nfc");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  MockNfcSystemLevelSetting::SetNfcSystemLevelSettingEnabled(false);
  EXPECT_FALSE(HasActivePrompt());
  RequestNfcPermission(web_contents(), RequestID(0), requesting_frame, true);
  ASSERT_TRUE(HasActivePrompt());
  ASSERT_FALSE(MockNfcSystemLevelSetting::HasShownNfcSettingPrompt());
  DenyPrompt();
  ASSERT_FALSE(MockNfcSystemLevelSetting::HasShownNfcSettingPrompt());
  CheckPermissionMessageSent(0 /* request _id */, false /* allowed */);
}

TEST_F(NfcPermissionContextTests,
       SystemNfcSettingDisabledWhenNfcPermissionAlreadyGranted) {
  GURL requesting_frame("https://www.example.com/nfc");
  SetNfcContentSetting(requesting_frame, requesting_frame,
                       CONTENT_SETTING_ALLOW);
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  MockNfcSystemLevelSetting::SetNfcSystemLevelSettingEnabled(false);
  EXPECT_FALSE(HasActivePrompt());
  RequestNfcPermission(web_contents(), RequestID(0), requesting_frame, true);
  ASSERT_FALSE(HasActivePrompt());
  ASSERT_TRUE(MockNfcSystemLevelSetting::HasShownNfcSettingPrompt());
}

TEST_F(NfcPermissionContextTests,
       SystemNfcSettingEnabledWhenNfcPermissionAlreadyGranted) {
  GURL requesting_frame("https://www.example.com/nfc");
  SetNfcContentSetting(requesting_frame, requesting_frame,
                       CONTENT_SETTING_ALLOW);
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  EXPECT_FALSE(HasActivePrompt());
  RequestNfcPermission(web_contents(), RequestID(0), requesting_frame, true);
  ASSERT_FALSE(HasActivePrompt());
  ASSERT_FALSE(MockNfcSystemLevelSetting::HasShownNfcSettingPrompt());
}

TEST_F(NfcPermissionContextTests,
       SystemNfcSettingCantBeEnabledWhenNfcPermissionGetsGranted) {
  GURL requesting_frame("https://www.example.com/nfc");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  MockNfcSystemLevelSetting::SetNfcSystemLevelSettingEnabled(false);
  MockNfcSystemLevelSetting::SetNfcAccessIsPossible(false);
  EXPECT_FALSE(HasActivePrompt());
  RequestNfcPermission(web_contents(), RequestID(0), requesting_frame, true);
  ASSERT_TRUE(HasActivePrompt());
  ASSERT_FALSE(MockNfcSystemLevelSetting::HasShownNfcSettingPrompt());
  AcceptPrompt();
  ASSERT_FALSE(HasActivePrompt());
  ASSERT_FALSE(MockNfcSystemLevelSetting::HasShownNfcSettingPrompt());
  CheckPermissionMessageSent(0 /* request _id */, true /* allowed */);
}

TEST_F(NfcPermissionContextTests,
       SystemNfcSettingCantBeEnabledWhenNfcPermissionGetsDenied) {
  GURL requesting_frame("https://www.example.com/nfc");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  MockNfcSystemLevelSetting::SetNfcSystemLevelSettingEnabled(false);
  MockNfcSystemLevelSetting::SetNfcAccessIsPossible(false);
  EXPECT_FALSE(HasActivePrompt());
  RequestNfcPermission(web_contents(), RequestID(0), requesting_frame, true);
  ASSERT_TRUE(HasActivePrompt());
  ASSERT_FALSE(MockNfcSystemLevelSetting::HasShownNfcSettingPrompt());
  DenyPrompt();
  ASSERT_FALSE(HasActivePrompt());
  ASSERT_FALSE(MockNfcSystemLevelSetting::HasShownNfcSettingPrompt());
  CheckPermissionMessageSent(0 /* request _id */, false /* allowed */);
}

TEST_F(NfcPermissionContextTests,
       SystemNfcSettingCantBeEnabledWhenNfcPermissionAlreadyGranted) {
  GURL requesting_frame("https://www.example.com/nfc");
  SetNfcContentSetting(requesting_frame, requesting_frame,
                       CONTENT_SETTING_ALLOW);
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  MockNfcSystemLevelSetting::SetNfcSystemLevelSettingEnabled(false);
  MockNfcSystemLevelSetting::SetNfcAccessIsPossible(false);
  EXPECT_FALSE(HasActivePrompt());
  RequestNfcPermission(web_contents(), RequestID(0), requesting_frame, true);
  ASSERT_FALSE(HasActivePrompt());
  ASSERT_FALSE(MockNfcSystemLevelSetting::HasShownNfcSettingPrompt());
  CheckPermissionMessageSent(0 /* request _id */, true /* allowed */);
}

TEST_F(NfcPermissionContextTests, CancelNfcPermissionRequest) {
  GURL requesting_frame("https://www.example.com/nfc");
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetNfcContentSetting(requesting_frame, requesting_frame));

  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();

  ASSERT_FALSE(HasActivePrompt());

  RequestNfcPermission(web_contents(), RequestID(0), requesting_frame, true);

  ASSERT_TRUE(HasActivePrompt());

  // Simulate the frame going away; the request should be removed.
  ClosePrompt();

  ASSERT_FALSE(MockNfcSystemLevelSetting::HasShownNfcSettingPrompt());

  // Ensure permission isn't persisted.
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetNfcContentSetting(requesting_frame, requesting_frame));
}
#endif
