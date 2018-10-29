// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_permission_context.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/id_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/permissions/permission_context_base.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/ui/permission_bubble/mock_permission_prompt_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/mock_location_settings.h"
#include "chrome/browser/android/search_permissions/search_permissions_service.h"
#include "chrome/browser/geolocation/geolocation_permission_context_android.h"
#include "components/location/android/location_settings_dialog_outcome.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/permission_type.h"
#include "third_party/blink/public/platform/modules/permissions/permission_status.mojom.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/view_type_utils.h"
#endif

using content::MockRenderProcessHost;


#if defined(OS_ANDROID)
// TestSearchEngineDelegate
class TestSearchEngineDelegate
    : public SearchPermissionsService::SearchEngineDelegate {
 public:
  base::string16 GetDSEName() override { return base::string16(); }

  url::Origin GetDSEOrigin() override {
    return url::Origin::Create(GURL(kDSETestUrl));
  }

  void SetDSEChangedCallback(const base::Closure& callback) override {
    dse_changed_callback_ = callback;
  }

  void UpdateDSEOrigin() { dse_changed_callback_.Run(); }

  static const char kDSETestUrl[];

 private:
  base::Closure dse_changed_callback_;
};

const char TestSearchEngineDelegate::kDSETestUrl[] = "https://www.dsetest.com";
#endif  // defined(OS_ANDROID)

// GeolocationPermissionContextTests ------------------------------------------

class GeolocationPermissionContextTests
    : public ChromeRenderViewHostTestHarness {
 protected:
  // ChromeRenderViewHostTestHarness:
  void SetUp() override;
  void TearDown() override;

  PermissionRequestID RequestID(int request_id);
  PermissionRequestID RequestIDForTab(int tab, int request_id);

  void RequestGeolocationPermission(content::WebContents* web_contents,
                                    const PermissionRequestID& id,
                                    const GURL& requesting_frame,
                                    bool user_gesture);

  void PermissionResponse(const PermissionRequestID& id,
                          ContentSetting content_setting);
  void CheckPermissionMessageSent(int request_id, bool allowed);
  void CheckPermissionMessageSentForTab(int tab, int request_id, bool allowed);
  void CheckPermissionMessageSentInternal(MockRenderProcessHost* process,
                                          int request_id,
                                          bool allowed);
  void AddNewTab(const GURL& url);
  void CheckTabContentsState(const GURL& requesting_frame,
                             ContentSetting expected_content_setting);
  void SetupRequestManager(content::WebContents* web_contents);
#if defined(OS_ANDROID)
  bool RequestPermissionIsLSDShown(const GURL& origin);
  bool RequestPermissionIsLSDShownWithPermissionPrompt(const GURL& origin);
  void AddDayOffsetForTesting(int days);
  void SetDSEOriginForTesting(const char* dse_origin);
#endif
  void RequestManagerDocumentLoadCompleted();
  void RequestManagerDocumentLoadCompleted(content::WebContents* web_contents);
  ContentSetting GetGeolocationContentSetting(GURL frame_0, GURL frame_1);
  void SetGeolocationContentSetting(GURL frame_0,
                                    GURL frame_1,
                                    ContentSetting content_setting);
  bool HasActivePrompt();
  bool HasActivePrompt(content::WebContents* web_contents);
  void AcceptPrompt();
  void AcceptPrompt(content::WebContents* web_contents);
  void DenyPrompt();
  void ClosePrompt();
  base::string16 GetPromptText();

  // owned by the browser context
  GeolocationPermissionContext* geolocation_permission_context_;
  std::vector<std::unique_ptr<content::WebContents>> extra_tabs_;
  std::vector<std::unique_ptr<MockPermissionPromptFactory>>
      mock_permission_prompt_factories_;

  // A map between renderer child id and a pair represending the bridge id and
  // whether the requested permission was allowed.
  std::map<int, std::pair<int, bool>> responses_;
};

PermissionRequestID GeolocationPermissionContextTests::RequestID(
    int request_id) {
  return PermissionRequestID(
      web_contents()->GetMainFrame()->GetProcess()->GetID(),
      web_contents()->GetMainFrame()->GetRoutingID(), request_id);
}

PermissionRequestID GeolocationPermissionContextTests::RequestIDForTab(
    int tab,
    int request_id) {
  return PermissionRequestID(
      extra_tabs_[tab]->GetMainFrame()->GetProcess()->GetID(),
      extra_tabs_[tab]->GetMainFrame()->GetRoutingID(), request_id);
}

void GeolocationPermissionContextTests::RequestGeolocationPermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    bool user_gesture) {
  geolocation_permission_context_->RequestPermission(
      web_contents, id, requesting_frame, user_gesture,
      base::Bind(&GeolocationPermissionContextTests::PermissionResponse,
                 base::Unretained(this), id));
  content::RunAllTasksUntilIdle();
}

void GeolocationPermissionContextTests::PermissionResponse(
    const PermissionRequestID& id,
    ContentSetting content_setting) {
  responses_[id.render_process_id()] =
      std::make_pair(id.request_id(), content_setting == CONTENT_SETTING_ALLOW);
}

void GeolocationPermissionContextTests::CheckPermissionMessageSent(
    int request_id,
    bool allowed) {
  CheckPermissionMessageSentInternal(process(), request_id, allowed);
}

void GeolocationPermissionContextTests::CheckPermissionMessageSentForTab(
    int tab,
    int request_id,
    bool allowed) {
  CheckPermissionMessageSentInternal(
      static_cast<MockRenderProcessHost*>(
          extra_tabs_[tab]->GetMainFrame()->GetProcess()),
      request_id, allowed);
}

void GeolocationPermissionContextTests::CheckPermissionMessageSentInternal(
    MockRenderProcessHost* process,
    int request_id,
    bool allowed) {
  ASSERT_EQ(responses_.count(process->GetID()), 1U);
  EXPECT_EQ(request_id, responses_[process->GetID()].first);
  EXPECT_EQ(allowed, responses_[process->GetID()].second);
  responses_.erase(process->GetID());
}

void GeolocationPermissionContextTests::AddNewTab(const GURL& url) {
  std::unique_ptr<content::WebContents> new_tab = CreateTestWebContents();
  content::NavigationSimulator::NavigateAndCommitFromBrowser(new_tab.get(),
                                                             url);

  // Set up required helpers, and make this be as "tabby" as the code requires.
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::SetViewType(new_tab.get(), extensions::VIEW_TYPE_TAB_CONTENTS);
#endif

  SetupRequestManager(new_tab.get());

  extra_tabs_.push_back(std::move(new_tab));
}

void GeolocationPermissionContextTests::CheckTabContentsState(
    const GURL& requesting_frame,
    ContentSetting expected_content_setting) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  const ContentSettingsUsagesState::StateMap& state_map =
      content_settings->geolocation_usages_state().state_map();
  EXPECT_EQ(1U, state_map.count(requesting_frame.GetOrigin()));
  EXPECT_EQ(0U, state_map.count(requesting_frame));
  auto settings = state_map.find(requesting_frame.GetOrigin());
  ASSERT_FALSE(settings == state_map.end())
      << "geolocation state not found " << requesting_frame;
  EXPECT_EQ(expected_content_setting, settings->second);
}

void GeolocationPermissionContextTests::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  // Set up required helpers, and make this be as "tabby" as the code requires.
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::SetViewType(web_contents(), extensions::VIEW_TYPE_TAB_CONTENTS);
#endif
  TabSpecificContentSettings::CreateForWebContents(web_contents());
  geolocation_permission_context_ = static_cast<GeolocationPermissionContext*>(
      PermissionManager::Get(profile())->GetPermissionContext(
          CONTENT_SETTINGS_TYPE_GEOLOCATION));
  SetupRequestManager(web_contents());

#if defined(OS_ANDROID)
  static_cast<GeolocationPermissionContextAndroid*>(
      geolocation_permission_context_)
      ->SetLocationSettingsForTesting(
          std::unique_ptr<LocationSettings>(new MockLocationSettings()));
  MockLocationSettings::SetLocationStatus(true, true);
  MockLocationSettings::SetCanPromptForAndroidPermission(true);
  MockLocationSettings::SetLocationSettingsDialogStatus(false /* enabled */,
                                                        GRANTED);
  MockLocationSettings::ClearHasShownLocationSettingsDialog();
#endif
}

void GeolocationPermissionContextTests::TearDown() {
  mock_permission_prompt_factories_.clear();
  extra_tabs_.clear();
  ChromeRenderViewHostTestHarness::TearDown();
}

void GeolocationPermissionContextTests::SetupRequestManager(
    content::WebContents* web_contents) {
  // Create PermissionRequestManager.
  PermissionRequestManager::CreateForWebContents(web_contents);
  PermissionRequestManager* permission_request_manager =
      PermissionRequestManager::FromWebContents(web_contents);

  // Create a MockPermissionPromptFactory for the PermissionRequestManager.
  mock_permission_prompt_factories_.push_back(
      std::make_unique<MockPermissionPromptFactory>(
          permission_request_manager));
}

#if defined(OS_ANDROID)

bool GeolocationPermissionContextTests::RequestPermissionIsLSDShown(
    const GURL& origin) {
  NavigateAndCommit(origin);
  RequestManagerDocumentLoadCompleted();
  MockLocationSettings::ClearHasShownLocationSettingsDialog();
  RequestGeolocationPermission(web_contents(), RequestID(0), origin, true);

  return MockLocationSettings::HasShownLocationSettingsDialog();
}

bool GeolocationPermissionContextTests::
    RequestPermissionIsLSDShownWithPermissionPrompt(const GURL& origin) {
  NavigateAndCommit(origin);
  RequestManagerDocumentLoadCompleted();
  MockLocationSettings::ClearHasShownLocationSettingsDialog();
  RequestGeolocationPermission(web_contents(), RequestID(0), origin, true);

  EXPECT_TRUE(HasActivePrompt());
  AcceptPrompt();

  return MockLocationSettings::HasShownLocationSettingsDialog();
}

void GeolocationPermissionContextTests::AddDayOffsetForTesting(int days) {
  GeolocationPermissionContextAndroid::AddDayOffsetForTesting(days);
}

void GeolocationPermissionContextTests::SetDSEOriginForTesting(
    const char* dse_origin) {
  GeolocationPermissionContextAndroid::SetDSEOriginForTesting(dse_origin);
}
#endif

void GeolocationPermissionContextTests::RequestManagerDocumentLoadCompleted() {
  GeolocationPermissionContextTests::RequestManagerDocumentLoadCompleted(
      web_contents());
}

void GeolocationPermissionContextTests::RequestManagerDocumentLoadCompleted(
    content::WebContents* web_contents) {
  PermissionRequestManager::FromWebContents(web_contents)
      ->DocumentOnLoadCompletedInMainFrame();
}

ContentSetting GeolocationPermissionContextTests::GetGeolocationContentSetting(
    GURL frame_0, GURL frame_1) {
  return HostContentSettingsMapFactory::GetForProfile(profile())
      ->GetContentSetting(frame_0,
                          frame_1,
                          CONTENT_SETTINGS_TYPE_GEOLOCATION,
                          std::string());
}

void GeolocationPermissionContextTests::SetGeolocationContentSetting(
    GURL frame_0,
    GURL frame_1,
    ContentSetting content_setting) {
  return HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetContentSettingDefaultScope(frame_0, frame_1,
                                      CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                      std::string(), content_setting);
}

bool GeolocationPermissionContextTests::HasActivePrompt() {
  return HasActivePrompt(web_contents());
}

bool GeolocationPermissionContextTests::HasActivePrompt(
    content::WebContents* web_contents) {
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents);
  return manager->IsBubbleVisible();
}

void GeolocationPermissionContextTests::AcceptPrompt() {
  return AcceptPrompt(web_contents());
}

void GeolocationPermissionContextTests::AcceptPrompt(
    content::WebContents* web_contents) {
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents);
  manager->Accept();
}

void GeolocationPermissionContextTests::DenyPrompt() {
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents());
  manager->Deny();
}

void GeolocationPermissionContextTests::ClosePrompt() {
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents());
  manager->Closing();
}

base::string16 GeolocationPermissionContextTests::GetPromptText() {
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents());
  PermissionRequest* request = manager->requests_.front();
  return base::ASCIIToUTF16(request->GetOrigin().spec()) +
         request->GetMessageTextFragment();
}

// Tests ----------------------------------------------------------------------

TEST_F(GeolocationPermissionContextTests, SinglePermissionPrompt) {
  GURL requesting_frame("https://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();

  EXPECT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(
      web_contents(), RequestID(0), requesting_frame, true);
  ASSERT_TRUE(HasActivePrompt());
}

TEST_F(GeolocationPermissionContextTests,
       SinglePermissionPromptFailsOnInsecureOrigin) {
  GURL requesting_frame("http://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();

  EXPECT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(web_contents(), RequestID(0), requesting_frame,
                               true);
  ASSERT_FALSE(HasActivePrompt());
}

#if defined(OS_ANDROID)
// Tests concerning Android location settings permission
TEST_F(GeolocationPermissionContextTests, GeolocationEnabledDisabled) {
  GURL requesting_frame("https://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  base::HistogramTester histograms;
  MockLocationSettings::SetLocationStatus(true /* android */,
                                          true /* system */);
  EXPECT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(
      web_contents(), RequestID(0), requesting_frame, true);
  EXPECT_TRUE(HasActivePrompt());
  histograms.ExpectTotalCount("Permissions.Action.Geolocation", 0);

  content::NavigationSimulator::Reload(web_contents());
  histograms.ExpectUniqueSample("Permissions.Action.Geolocation",
                                static_cast<int>(PermissionAction::IGNORED), 1);
  MockLocationSettings::SetLocationStatus(false /* android */,
                                          true /* system */);
  MockLocationSettings::SetCanPromptForAndroidPermission(false);
  EXPECT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(
      web_contents(), RequestID(0), requesting_frame, true);
  histograms.ExpectUniqueSample("Permissions.Action.Geolocation",
                                static_cast<int>(PermissionAction::IGNORED), 1);
  EXPECT_FALSE(HasActivePrompt());
}

TEST_F(GeolocationPermissionContextTests, AndroidEnabledCanPrompt) {
  GURL requesting_frame("https://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  MockLocationSettings::SetLocationStatus(false /* android */,
                                          true /* system */);
  EXPECT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(
      web_contents(), RequestID(0), requesting_frame, true);
  ASSERT_TRUE(HasActivePrompt());
  AcceptPrompt();
  CheckTabContentsState(requesting_frame, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);
}

TEST_F(GeolocationPermissionContextTests, AndroidEnabledCantPrompt) {
  GURL requesting_frame("https://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  MockLocationSettings::SetLocationStatus(false /* android */,
                                          true /* system */);
  MockLocationSettings::SetCanPromptForAndroidPermission(false);
  EXPECT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(web_contents(), RequestID(0), requesting_frame,
                               true);
  EXPECT_FALSE(HasActivePrompt());
}

TEST_F(GeolocationPermissionContextTests, SystemLocationOffLSDDisabled) {
  GURL requesting_frame("https://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  MockLocationSettings::SetLocationStatus(true /* android */,
                                          false /* system */);
  EXPECT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(
      web_contents(), RequestID(0), requesting_frame, true);
  EXPECT_FALSE(HasActivePrompt());
  EXPECT_FALSE(MockLocationSettings::HasShownLocationSettingsDialog());
}

TEST_F(GeolocationPermissionContextTests, SystemLocationOnNoLSD) {
  GURL requesting_frame("https://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  EXPECT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(web_contents(), RequestID(0), requesting_frame,
                               true);
  ASSERT_TRUE(HasActivePrompt());
  AcceptPrompt();
  CheckTabContentsState(requesting_frame, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);
  EXPECT_FALSE(MockLocationSettings::HasShownLocationSettingsDialog());
}

TEST_F(GeolocationPermissionContextTests, SystemLocationOffLSDAccept) {
  base::HistogramTester tester;

  GURL requesting_frame("https://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  MockLocationSettings::SetLocationStatus(true /* android */,
                                          false /* system */);
  MockLocationSettings::SetLocationSettingsDialogStatus(true /* enabled */,
                                                        GRANTED);
  EXPECT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(web_contents(), RequestID(0), requesting_frame,
                               true);
  ASSERT_TRUE(HasActivePrompt());
  AcceptPrompt();
  CheckTabContentsState(requesting_frame, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);
  EXPECT_TRUE(MockLocationSettings::HasShownLocationSettingsDialog());

  tester.ExpectTotalCount("Geolocation.SettingsDialog.ShowEvent.NonDSE", 1);
  tester.ExpectTotalCount("Geolocation.SettingsDialog.AcceptEvent.NonDSE", 1);
  tester.ExpectTotalCount("Geolocation.SettingsDialog.DenyEvent.NonDSE", 0);
}

TEST_F(GeolocationPermissionContextTests, SystemLocationOffLSDReject) {
  base::HistogramTester tester;

  GURL requesting_frame("https://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  MockLocationSettings::SetLocationStatus(true /* android */,
                                          false /* system */);
  MockLocationSettings::SetLocationSettingsDialogStatus(true /* enabled */,
                                                        DENIED);
  EXPECT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(web_contents(), RequestID(0), requesting_frame,
                               true);
  ASSERT_TRUE(HasActivePrompt());
  AcceptPrompt();
  CheckTabContentsState(requesting_frame, CONTENT_SETTING_BLOCK);
  CheckPermissionMessageSent(0, false);
  EXPECT_TRUE(MockLocationSettings::HasShownLocationSettingsDialog());

  tester.ExpectTotalCount("Geolocation.SettingsDialog.ShowEvent.NonDSE", 1);
  tester.ExpectTotalCount("Geolocation.SettingsDialog.AcceptEvent.NonDSE", 0);
  tester.ExpectTotalCount("Geolocation.SettingsDialog.DenyEvent.NonDSE", 1);
}

TEST_F(GeolocationPermissionContextTests, LSDBackOffDifferentSites) {
  base::HistogramTester tester;

  GURL requesting_frame_1("https://www.example.com/geolocation");
  GURL requesting_frame_2("https://www.example-2.com/geolocation");
  const char* requesting_frame_dse_ptr = "https://www.dse.com/geolocation";
  GURL requesting_frame_dse(requesting_frame_dse_ptr);

  SetDSEOriginForTesting(requesting_frame_dse_ptr);

  // Set all origin geolocation permissions to ALLOW.
  SetGeolocationContentSetting(requesting_frame_1, requesting_frame_1,
                               CONTENT_SETTING_ALLOW);
  SetGeolocationContentSetting(requesting_frame_2, requesting_frame_2,
                               CONTENT_SETTING_ALLOW);
  SetGeolocationContentSetting(requesting_frame_dse, requesting_frame_dse,
                               CONTENT_SETTING_ALLOW);

  // Turn off system location but allow the LSD to be shown, and denied.
  MockLocationSettings::SetLocationStatus(true /* android */,
                                          false /* system */);
  MockLocationSettings::SetLocationSettingsDialogStatus(true /* enabled */,
                                                        DENIED);

  // Now permission requests should trigger the LSD, but the LSD will be denied,
  // putting the requesting origins into backoff. Check that the two non-DSE
  // origins share the same backoff, which is distinct to the DSE origin.
  // First, cancel a LSD prompt on the first non-DSE origin to go into backoff.
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame_1));

  // Now check that the LSD is prevented on this origin.
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame_1));

  // Now ask on the other non-DSE origin and check backoff prevented the prompt.
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame_2));

  // Test that the right histograms are updated.
  tester.ExpectTotalCount("Geolocation.SettingsDialog.ShowEvent.NonDSE", 1);
  tester.ExpectTotalCount("Geolocation.SettingsDialog.ShowEvent.DSE", 0);
  tester.ExpectTotalCount("Geolocation.SettingsDialog.SuppressEvent.NonDSE", 2);
  tester.ExpectTotalCount("Geolocation.SettingsDialog.SuppressEvent.DSE", 0);

  // Now request on the DSE and check that the LSD is shown, as the non-DSE
  // backoff should not apply.
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame_dse));

  // Now check that the DSE is in backoff.
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame_dse));

  // Test that the right histograms are updated.
  tester.ExpectTotalCount("Geolocation.SettingsDialog.ShowEvent.NonDSE", 1);
  tester.ExpectTotalCount("Geolocation.SettingsDialog.ShowEvent.DSE", 1);
  tester.ExpectTotalCount("Geolocation.SettingsDialog.SuppressEvent.NonDSE", 2);
  tester.ExpectTotalCount("Geolocation.SettingsDialog.SuppressEvent.DSE", 1);
}

TEST_F(GeolocationPermissionContextTests, LSDBackOffTiming) {
  base::HistogramTester tester;

  GURL requesting_frame("https://www.example.com/geolocation");
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_ALLOW);

  // Turn off system location but allow the LSD to be shown, and denied.
  MockLocationSettings::SetLocationStatus(true /* android */,
                                          false /* system */);
  MockLocationSettings::SetLocationSettingsDialogStatus(true /* enabled */,
                                                        DENIED);

  // First, cancel a LSD prompt on the first non-DSE origin to go into backoff.
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame));

  // Check the LSD is prevented in 6 days time.
  AddDayOffsetForTesting(6);
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame));

  // Check histograms so far.
  tester.ExpectTotalCount("Geolocation.SettingsDialog.ShowEvent.NonDSE", 1);
  tester.ExpectBucketCount("Geolocation.SettingsDialog.ShowEvent.NonDSE",
                           static_cast<base::HistogramBase::Sample>(
                               GeolocationPermissionContextAndroid::
                                   LocationSettingsDialogBackOff::kNoBackOff),
                           1);

  // Check it is shown in one more days time, but then not straight after..
  AddDayOffsetForTesting(1);
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame));

  // Check that it isn't shown 29 days after that.
  AddDayOffsetForTesting(29);
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame));

  // Check histograms so far.
  tester.ExpectTotalCount("Geolocation.SettingsDialog.ShowEvent.NonDSE", 2);
  tester.ExpectBucketCount("Geolocation.SettingsDialog.ShowEvent.NonDSE",
                           static_cast<base::HistogramBase::Sample>(
                               GeolocationPermissionContextAndroid::
                                   LocationSettingsDialogBackOff::kOneWeek),
                           1);

  // Check it is shown in one more days time, but then not straight after..
  AddDayOffsetForTesting(1);
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame));

  // Check that it isn't shown 89 days after that.
  AddDayOffsetForTesting(89);
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame));

  // Check histograms so far.
  tester.ExpectTotalCount("Geolocation.SettingsDialog.ShowEvent.NonDSE", 3);
  tester.ExpectBucketCount("Geolocation.SettingsDialog.ShowEvent.NonDSE",
                           static_cast<base::HistogramBase::Sample>(
                               GeolocationPermissionContextAndroid::
                                   LocationSettingsDialogBackOff::kOneMonth),
                           1);

  // Check it is shown in one more days time, but then not straight after..
  AddDayOffsetForTesting(1);
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame));

  // Check that it isn't shown 89 days after that.
  AddDayOffsetForTesting(89);
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame));

  // Check it is shown in one more days time, but then not straight after..
  AddDayOffsetForTesting(1);
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame));

  // Check histograms so far.
  tester.ExpectTotalCount("Geolocation.SettingsDialog.ShowEvent.NonDSE", 5);
  tester.ExpectBucketCount("Geolocation.SettingsDialog.ShowEvent.NonDSE",
                           static_cast<base::HistogramBase::Sample>(
                               GeolocationPermissionContextAndroid::
                                   LocationSettingsDialogBackOff::kThreeMonths),
                           2);
}

TEST_F(GeolocationPermissionContextTests, LSDBackOffPermissionStatus) {
  GURL requesting_frame("https://www.example.com/geolocation");
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_ALLOW);

  // Turn off system location but allow the LSD to be shown, and denied.
  MockLocationSettings::SetLocationStatus(true /* android */,
                                          false /* system */);
  MockLocationSettings::SetLocationSettingsDialogStatus(true /* enabled */,
                                                        DENIED);

  // The permission status should reflect that the LSD will be shown.
  ASSERT_EQ(blink::mojom::PermissionStatus::ASK,
            PermissionManager::Get(profile())->GetPermissionStatus(
                content::PermissionType::GEOLOCATION, requesting_frame,
                requesting_frame));
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));

  // Now that the LSD is in backoff, the permission status should reflect it.
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame));
  ASSERT_EQ(blink::mojom::PermissionStatus::DENIED,
            PermissionManager::Get(profile())->GetPermissionStatus(
                content::PermissionType::GEOLOCATION, requesting_frame,
                requesting_frame));
}

TEST_F(GeolocationPermissionContextTests, LSDBackOffAskPromptsDespiteBackOff) {
  GURL requesting_frame("https://www.example.com/geolocation");
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_ALLOW);

  // Turn off system location but allow the LSD to be shown, and denied.
  MockLocationSettings::SetLocationStatus(true /* android */,
                                          false /* system */);
  MockLocationSettings::SetLocationSettingsDialogStatus(true /* enabled */,
                                                        DENIED);

  // First, cancel a LSD prompt on the first non-DSE origin to go into backoff.
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame));

  // Set the content setting back to ASK. The permission status should be
  // prompt, and the LSD prompt should now be shown.
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_ASK);
  ASSERT_EQ(blink::mojom::PermissionStatus::ASK,
            PermissionManager::Get(profile())->GetPermissionStatus(
                content::PermissionType::GEOLOCATION, requesting_frame,
                requesting_frame));
  EXPECT_TRUE(
      RequestPermissionIsLSDShownWithPermissionPrompt(requesting_frame));
}

TEST_F(GeolocationPermissionContextTests,
       LSDBackOffAcceptPermissionResetsBackOff) {
  GURL requesting_frame("https://www.example.com/geolocation");
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_ALLOW);

  // Turn off system location but allow the LSD to be shown, and denied.
  MockLocationSettings::SetLocationStatus(true /* android */,
                                          false /* system */);
  MockLocationSettings::SetLocationSettingsDialogStatus(true /* enabled */,
                                                        DENIED);

  // First, get into the highest backoff state.
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));
  AddDayOffsetForTesting(7);
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));
  AddDayOffsetForTesting(30);
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));
  AddDayOffsetForTesting(90);
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));

  // Now accept a permissions prompt.
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_ASK);
  EXPECT_TRUE(
      RequestPermissionIsLSDShownWithPermissionPrompt(requesting_frame));

  // Denying the LSD stops the content setting from being stored, so explicitly
  // set it to ALLOW.
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_ALLOW);

  // And check that back in the lowest backoff state.
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame));
  AddDayOffsetForTesting(7);
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));
}

TEST_F(GeolocationPermissionContextTests, LSDBackOffAcceptLSDResetsBackOff) {
  GURL requesting_frame("https://www.example.com/geolocation");
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_ALLOW);

  // Turn off system location but allow the LSD to be shown, and denied.
  MockLocationSettings::SetLocationStatus(true /* android */,
                                          false /* system */);
  MockLocationSettings::SetLocationSettingsDialogStatus(true /* enabled */,
                                                        DENIED);

  // First, get into the highest backoff state.
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));
  AddDayOffsetForTesting(7);
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));
  AddDayOffsetForTesting(30);
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));

  // Now accept the LSD.
  AddDayOffsetForTesting(90);
  MockLocationSettings::SetLocationSettingsDialogStatus(true /* enabled */,
                                                        GRANTED);
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));

  // Check that not in backoff, and that at the lowest backoff state.
  MockLocationSettings::SetLocationSettingsDialogStatus(true /* enabled */,
                                                        DENIED);
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame));
  AddDayOffsetForTesting(7);
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));
}

#endif

TEST_F(GeolocationPermissionContextTests, QueuedPermission) {
  // With permission delegation enabled, there can't be multiple permission
  // requests on the same page. This test can be deleted once the feature is
  // enabled by default.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kPermissionDelegation);
  GURL requesting_frame_0("https://www.example.com/geolocation");
  GURL requesting_frame_1("https://www.example-2.com/geolocation");
  EXPECT_EQ(CONTENT_SETTING_ASK, GetGeolocationContentSetting(
                                     requesting_frame_0, requesting_frame_1));
  EXPECT_EQ(CONTENT_SETTING_ASK, GetGeolocationContentSetting(
                                     requesting_frame_1, requesting_frame_1));

  NavigateAndCommit(requesting_frame_0);
  RequestManagerDocumentLoadCompleted();

  // Check that no permission requests have happened yet.
  EXPECT_FALSE(HasActivePrompt());

  // Request permission for two frames.
  RequestGeolocationPermission(web_contents(), RequestID(0), requesting_frame_0,
                               true);
  RequestGeolocationPermission(web_contents(), RequestID(1), requesting_frame_1,
                               true);
  // Ensure only one prompt is created.
  ASSERT_TRUE(HasActivePrompt());
  base::string16 text_0 = GetPromptText();

  // Accept the first frame.
  AcceptPrompt();
  CheckTabContentsState(requesting_frame_0, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);

  // Now we should have a new prompt for the second frame.
  ASSERT_TRUE(HasActivePrompt());
  base::string16 text_1 = GetPromptText();

  // Check that the messages differ.
  EXPECT_NE(text_0, text_1);

  // Cancel (block) this frame.
  DenyPrompt();
  CheckTabContentsState(requesting_frame_1, CONTENT_SETTING_BLOCK);
  CheckPermissionMessageSent(1, false);

  // Ensure the persisted permissions are ok.
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      GetGeolocationContentSetting(requesting_frame_0, requesting_frame_0));
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      GetGeolocationContentSetting(requesting_frame_1, requesting_frame_0));
}

TEST_F(GeolocationPermissionContextTests, HashIsIgnored) {
  GURL url_a("https://www.example.com/geolocation#a");
  GURL url_b("https://www.example.com/geolocation#b");

  // Navigate to the first url.
  NavigateAndCommit(url_a);
  RequestManagerDocumentLoadCompleted();

  // Check permission is requested.
  ASSERT_FALSE(HasActivePrompt());
  const bool user_gesture = true;
  RequestGeolocationPermission(web_contents(), RequestID(0), url_a,
                               user_gesture);
  ASSERT_TRUE(HasActivePrompt());

  // Change the hash, we'll still be on the same page.
  NavigateAndCommit(url_b);
  RequestManagerDocumentLoadCompleted();

  // Accept.
  AcceptPrompt();
  CheckTabContentsState(url_a, CONTENT_SETTING_ALLOW);
  CheckTabContentsState(url_b, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);
}

TEST_F(GeolocationPermissionContextTests, DISABLED_PermissionForFileScheme) {
  // TODO(felt): The bubble is rejecting file:// permission requests.
  // Fix and enable this test. crbug.com/444047
  GURL requesting_frame("file://example/geolocation.html");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();

  // Check permission is requested.
  ASSERT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(web_contents(), RequestID(0), requesting_frame,
                               true);
  EXPECT_TRUE(HasActivePrompt());

  // Accept the frame.
  AcceptPrompt();
  CheckTabContentsState(requesting_frame, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);

  // Make sure the setting is not stored.
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetGeolocationContentSetting(requesting_frame, requesting_frame));
}

TEST_F(GeolocationPermissionContextTests, CancelGeolocationPermissionRequest) {
  GURL frame_0("https://www.example.com/geolocation");
  EXPECT_EQ(
      CONTENT_SETTING_ASK, GetGeolocationContentSetting(frame_0, frame_0));

  NavigateAndCommit(frame_0);
  RequestManagerDocumentLoadCompleted();

  ASSERT_FALSE(HasActivePrompt());

  RequestGeolocationPermission(
      web_contents(), RequestID(0), frame_0, true);

  ASSERT_TRUE(HasActivePrompt());
  base::string16 text_0 = GetPromptText();
  ASSERT_FALSE(text_0.empty());

  // Simulate the frame going away; the request should be removed.
  ClosePrompt();

  // Ensure permission isn't persisted.
  EXPECT_EQ(
      CONTENT_SETTING_ASK, GetGeolocationContentSetting(frame_0, frame_0));
}

TEST_F(GeolocationPermissionContextTests, InvalidURL) {
  // Navigate to the first url.
  GURL invalid_embedder("about:blank");
  GURL requesting_frame;
  NavigateAndCommit(invalid_embedder);
  RequestManagerDocumentLoadCompleted();

  // Nothing should be displayed.
  EXPECT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(
      web_contents(), RequestID(0), requesting_frame, true);
  EXPECT_FALSE(HasActivePrompt());
  CheckPermissionMessageSent(0, false);
}

TEST_F(GeolocationPermissionContextTests, SameOriginMultipleTabs) {
  GURL url_a("https://www.example.com/geolocation");
  GURL url_b("https://www.example-2.com/geolocation");
  NavigateAndCommit(url_a);  // Tab A0
  AddNewTab(url_b);          // Tab B (extra_tabs_[0])
  AddNewTab(url_a);          // Tab A1 (extra_tabs_[1])
  RequestManagerDocumentLoadCompleted();
  RequestManagerDocumentLoadCompleted(extra_tabs_[0].get());
  RequestManagerDocumentLoadCompleted(extra_tabs_[1].get());

  // Request permission in all three tabs.
  RequestGeolocationPermission(
      web_contents(), RequestID(0), url_a, true);
  RequestGeolocationPermission(
      extra_tabs_[0].get(), RequestIDForTab(0, 0), url_b, true);
  RequestGeolocationPermission(
      extra_tabs_[1].get(), RequestIDForTab(1, 0), url_a, true);
  ASSERT_TRUE(HasActivePrompt());  // For A0.
  ASSERT_TRUE(HasActivePrompt(extra_tabs_[0].get()));
  ASSERT_TRUE(HasActivePrompt(extra_tabs_[1].get()));

  // Accept the permission in tab A0.
  AcceptPrompt();
  CheckPermissionMessageSent(0, true);
  // Because they're the same origin, this should cause tab A1's prompt to
  // disappear, but it doesn't: crbug.com/443013.
  // TODO(felt): Update this test when the bubble's behavior is changed.
  // Either way, tab B should still have a pending permission request.
  ASSERT_TRUE(HasActivePrompt(extra_tabs_[0].get()));
  ASSERT_TRUE(HasActivePrompt(extra_tabs_[1].get()));
}

TEST_F(GeolocationPermissionContextTests, QueuedOriginMultipleTabs) {
  // With permission delegation enabled, there can't be multiple permission
  // requests on the same page. This test can be deleted once the feature is
  // enabled by default.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kPermissionDelegation);
  GURL url_a("https://www.example.com/geolocation");
  GURL url_b("https://www.example-2.com/geolocation");
  NavigateAndCommit(url_a);  // Tab A0.
  AddNewTab(url_a);          // Tab A1.
  RequestManagerDocumentLoadCompleted();
  RequestManagerDocumentLoadCompleted(extra_tabs_[0].get());

  // Request permission in both tabs; the extra tab will have two permission
  // requests from two origins.
  RequestGeolocationPermission(web_contents(), RequestID(0), url_a, true);
  RequestGeolocationPermission(extra_tabs_[0].get(), RequestIDForTab(0, 0),
                               url_a, true);
  RequestGeolocationPermission(extra_tabs_[0].get(), RequestIDForTab(0, 1),
                               url_b, true);

  ASSERT_TRUE(HasActivePrompt());
  ASSERT_TRUE(HasActivePrompt(extra_tabs_[0].get()));

  // Accept the first request in tab A1.
  AcceptPrompt(extra_tabs_[0].get());
  CheckPermissionMessageSentForTab(0, 0, true);

  // Because they're the same origin, this should cause tab A0's prompt to
  // disappear, but it doesn't : crbug.com/443013.
  // TODO(felt): Update this test when the bubble's behavior is changed.
  EXPECT_TRUE(HasActivePrompt());

  // The second request should now be visible in tab A1.
  ASSERT_TRUE(HasActivePrompt(extra_tabs_[0].get()));

  // Accept the second request and check that it's gone.
  AcceptPrompt(extra_tabs_[0].get());
  EXPECT_FALSE(HasActivePrompt(extra_tabs_[0].get()));
}

TEST_F(GeolocationPermissionContextTests, TabDestroyed) {
  GURL requesting_frame("https://www.example.com/geolocation");
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetGeolocationContentSetting(requesting_frame, requesting_frame));

  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();

  // Request permission for two frames.
  RequestGeolocationPermission(web_contents(), RequestID(0), requesting_frame,
                               false);

  ASSERT_TRUE(HasActivePrompt());
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetGeolocationContentSetting(requesting_frame, requesting_frame));
}

#if defined(OS_ANDROID)
TEST_F(GeolocationPermissionContextTests, SearchGeolocationInIncognito) {
  GURL requesting_frame(TestSearchEngineDelegate::kDSETestUrl);

  SearchPermissionsService* service =
      SearchPermissionsService::Factory::GetForBrowserContext(profile());
  std::unique_ptr<TestSearchEngineDelegate> delegate =
      std::make_unique<TestSearchEngineDelegate>();
  TestSearchEngineDelegate* delegate_ptr = delegate.get();
  service->SetSearchEngineDelegateForTest(std::move(delegate));
  delegate_ptr->UpdateDSEOrigin();

  // The DSE should be auto-granted geolocation.
  ASSERT_EQ(CONTENT_SETTING_ALLOW,
            PermissionManager::Get(profile())
                ->GetPermissionStatus(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                      requesting_frame, requesting_frame)
                .content_setting);

  Profile* otr_profile = profile()->GetOffTheRecordProfile();

  // A DSE setting of ALLOW should not flow through to incognito.
  ASSERT_EQ(CONTENT_SETTING_ASK,
            PermissionManager::Get(otr_profile)
                ->GetPermissionStatus(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                      requesting_frame, requesting_frame)
                .content_setting);
}

TEST_F(GeolocationPermissionContextTests, GeolocationStatusAndroidDisabled) {
  GURL requesting_frame("https://www.example.com/geolocation");

  // With the Android permission off, but location allowed for a domain, the
  // permission status should be ASK.
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_ALLOW);
  MockLocationSettings::SetLocationStatus(false /* android */,
                                          true /* system */);
  ASSERT_EQ(blink::mojom::PermissionStatus::ASK,
            PermissionManager::Get(profile())->GetPermissionStatus(
                content::PermissionType::GEOLOCATION, requesting_frame,
                requesting_frame));

  // With the Android permission off, and location blocked for a domain, the
  // permission status should still be BLOCK.
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_BLOCK);
  ASSERT_EQ(blink::mojom::PermissionStatus::DENIED,
            PermissionManager::Get(profile())->GetPermissionStatus(
                content::PermissionType::GEOLOCATION, requesting_frame,
                requesting_frame));

  // With the Android permission off, and location prompt for a domain, the
  // permission status should still be ASK.
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_ASK);
  ASSERT_EQ(blink::mojom::PermissionStatus::ASK,
            PermissionManager::Get(profile())->GetPermissionStatus(
                content::PermissionType::GEOLOCATION, requesting_frame,
                requesting_frame));
}

TEST_F(GeolocationPermissionContextTests, GeolocationStatusSystemDisabled) {
  GURL requesting_frame("https://www.example.com/geolocation");

  // With the system permission off, but location allowed for a domain, the
  // permission status should be reflect whether the LSD can be shown.
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_ALLOW);
  MockLocationSettings::SetLocationStatus(true /* android */,
                                          false /* system */);
  MockLocationSettings::SetLocationSettingsDialogStatus(true /* enabled */,
                                                        DENIED);
  ASSERT_EQ(blink::mojom::PermissionStatus::ASK,
            PermissionManager::Get(profile())->GetPermissionStatus(
                content::PermissionType::GEOLOCATION, requesting_frame,
                requesting_frame));

  MockLocationSettings::SetLocationSettingsDialogStatus(false /* enabled */,
                                                        GRANTED);
  ASSERT_EQ(blink::mojom::PermissionStatus::DENIED,
            PermissionManager::Get(profile())->GetPermissionStatus(
                content::PermissionType::GEOLOCATION, requesting_frame,
                requesting_frame));

  // The result should be the same if the location permission is ASK.
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_ASK);
  MockLocationSettings::SetLocationSettingsDialogStatus(true /* enabled */,
                                                        GRANTED);
  ASSERT_EQ(blink::mojom::PermissionStatus::ASK,
            PermissionManager::Get(profile())->GetPermissionStatus(
                content::PermissionType::GEOLOCATION, requesting_frame,
                requesting_frame));

  MockLocationSettings::SetLocationSettingsDialogStatus(false /* enabled */,
                                                        GRANTED);
  ASSERT_EQ(blink::mojom::PermissionStatus::DENIED,
            PermissionManager::Get(profile())->GetPermissionStatus(
                content::PermissionType::GEOLOCATION, requesting_frame,
                requesting_frame));

  // With the Android permission off, and location blocked for a domain, the
  // permission status should still be BLOCK.
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_BLOCK);
  MockLocationSettings::SetLocationSettingsDialogStatus(true /* enabled */,
                                                        GRANTED);
  ASSERT_EQ(blink::mojom::PermissionStatus::DENIED,
            PermissionManager::Get(profile())->GetPermissionStatus(
                content::PermissionType::GEOLOCATION, requesting_frame,
                requesting_frame));
}
#endif  // defined(OS_ANDROID)
