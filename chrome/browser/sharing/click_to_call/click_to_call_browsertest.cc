// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_ui_controller.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_utils.h"
#include "chrome/browser/sharing/click_to_call/feature.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/browser/sharing/sharing_browsertest.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sync/test/integration/sessions_helper.h"
#include "chrome/common/pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace {
const char kTelUrl[] = "tel:+9876543210";
const char kNonTelUrl[] = "https://google.com";
const char kLinkText[] = "Google";
const char kTextWithPhoneNumber[] = "call 9876543210 now";
const char kTextWithoutPhoneNumber[] = "abcde";

const char kTestPageURL[] = "/sharing/tel.html";

enum class ClickToCallPolicy {
  kNotConfigured,
  kFalse,
  kTrue,
};

}  // namespace

// Browser tests for the Click To Call feature.
class ClickToCallBrowserTestBase : public SharingBrowserTest {
 public:
  ClickToCallBrowserTestBase() {}

  ~ClickToCallBrowserTestBase() override {}

  std::string GetTestPageURL() const override {
    return std::string(kTestPageURL);
  }

  void CheckLastSharingMessageSent(
      const std::string& expected_phone_number) const {
    chrome_browser_sharing::SharingMessage sharing_message =
        GetLastSharingMessageSent();
    ASSERT_TRUE(sharing_message.has_click_to_call_message());
    EXPECT_EQ(expected_phone_number,
              sharing_message.click_to_call_message().phone_number());
  }

 protected:
  base::test::ScopedFeatureList feature_list_;

  std::string HistogramName(const char* suffix) {
    return base::StrCat({"Sharing.ClickToCallPhoneNumber", suffix});
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ClickToCallBrowserTestBase);
};

class ClickToCallBrowserTest : public ClickToCallBrowserTestBase {
 public:
  ClickToCallBrowserTest() {
    feature_list_.InitWithFeatures({kSharingDeviceRegistration, kClickToCallUI,
                                    kClickToCallContextMenuForSelectedText},
                                   {});
  }
};

// TODO(himanshujaju): Add UI checks.
IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest,
                       ContextMenu_TelLink_SingleDeviceAvailable) {
  Init(sync_pb::SharingSpecificFields::CLICK_TO_CALL,
       sync_pb::SharingSpecificFields::UNKNOWN);
  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::CLICK_TO_CALL);
  ASSERT_EQ(1u, devices.size());

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitContextMenu(GURL(kTelUrl), kLinkText, kTextWithoutPhoneNumber);

  // Check click to call items in context menu
  ASSERT_TRUE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES));

  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
                       0);
  CheckLastReceiver(devices[0]->guid());
  CheckLastSharingMessageSent(GetUnescapedURLContent(GURL(kTelUrl)));
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest, ContextMenu_NoDevicesAvailable) {
  Init(sync_pb::SharingSpecificFields::UNKNOWN,
       sync_pb::SharingSpecificFields::UNKNOWN);
  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::CLICK_TO_CALL);
  ASSERT_EQ(0u, devices.size());

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitContextMenu(GURL(kTelUrl), kLinkText, kTextWithoutPhoneNumber);
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES));
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest,
                       ContextMenu_DevicesAvailable_SyncTurnedOff) {
  if (base::FeatureList::IsEnabled(kSharingUseDeviceInfo) &&
      base::FeatureList::IsEnabled(kSharingDeriveVapidKey) &&
      base::FeatureList::IsEnabled(switches::kSyncDeviceInfoInTransportMode)) {
    // Turning off sync will have no effect when Click to Call is available on
    // sign-in.
    return;
  }

  Init(sync_pb::SharingSpecificFields::CLICK_TO_CALL,
       sync_pb::SharingSpecificFields::UNKNOWN);
  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::CLICK_TO_CALL);
  ASSERT_EQ(1u, devices.size());

  // Disable syncing preferences which is necessary for Sharing.
  GetSyncService(0)->GetUserSettings()->SetSelectedTypes(false, {});
  ASSERT_TRUE(AwaitQuiescence());

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitContextMenu(GURL(kTelUrl), kLinkText, kTextWithoutPhoneNumber);
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES));
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest,
                       ContextMenu_TelLink_MultipleDevicesAvailable) {
  Init(sync_pb::SharingSpecificFields::CLICK_TO_CALL,
       sync_pb::SharingSpecificFields::CLICK_TO_CALL);
  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::CLICK_TO_CALL);
  ASSERT_EQ(2u, devices.size());

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitContextMenu(GURL(kTelUrl), kLinkText, kTextWithoutPhoneNumber);
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  ASSERT_TRUE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES));

  ui::MenuModel* sub_menu_model = nullptr;
  int device_id = -1;
  ASSERT_TRUE(menu->GetMenuModelAndItemIndex(kSubMenuFirstDeviceCommandId,
                                             &sub_menu_model, &device_id));
  EXPECT_EQ(2, sub_menu_model->GetItemCount());
  EXPECT_EQ(0, device_id);

  for (auto& device : devices) {
    EXPECT_EQ(kSubMenuFirstDeviceCommandId + device_id,
              sub_menu_model->GetCommandIdAt(device_id));
    sub_menu_model->ActivatedAt(device_id);

    CheckLastReceiver(device->guid());
    CheckLastSharingMessageSent(GetUnescapedURLContent(GURL(kTelUrl)));
    device_id++;
  }
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest,
                       ContextMenu_HighlightedText_MultipleDevicesAvailable) {
  Init(sync_pb::SharingSpecificFields::CLICK_TO_CALL,
       sync_pb::SharingSpecificFields::CLICK_TO_CALL);
  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::CLICK_TO_CALL);
  ASSERT_EQ(2u, devices.size());

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitContextMenu(GURL(kNonTelUrl), kLinkText, kTextWithPhoneNumber);
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  ASSERT_TRUE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES));

  ui::MenuModel* sub_menu_model = nullptr;
  int device_id = -1;
  ASSERT_TRUE(menu->GetMenuModelAndItemIndex(kSubMenuFirstDeviceCommandId,
                                             &sub_menu_model, &device_id));
  EXPECT_EQ(2, sub_menu_model->GetItemCount());
  EXPECT_EQ(0, device_id);

  for (auto& device : devices) {
    EXPECT_EQ(kSubMenuFirstDeviceCommandId + device_id,
              sub_menu_model->GetCommandIdAt(device_id));
    sub_menu_model->ActivatedAt(device_id);

    CheckLastReceiver(device->guid());
    base::Optional<std::string> expected_number =
        ExtractPhoneNumberForClickToCall(GetProfile(0), kTextWithPhoneNumber);
    ASSERT_TRUE(expected_number.has_value());
    CheckLastSharingMessageSent(expected_number.value());
    device_id++;
  }
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest, ContextMenu_TelLink_Histograms) {
  base::HistogramTester histograms;
  Init(sync_pb::SharingSpecificFields::CLICK_TO_CALL,
       sync_pb::SharingSpecificFields::UNKNOWN);

  // Trigger a context menu for a link with 8 digits and 9 characters.
  std::unique_ptr<TestRenderViewContextMenu> menu = InitContextMenu(
      GURL("tel:1234-5678"), kLinkText, kTextWithoutPhoneNumber);

  base::HistogramTester::CountsMap expected_counts;
  expected_counts[HistogramName("Digits")] = 1;
  expected_counts[HistogramName("Digits.RightClickLink.Showing")] = 1;
  expected_counts[HistogramName("Length")] = 1;
  expected_counts[HistogramName("Length.RightClickLink.Showing")] = 1;
  EXPECT_THAT(histograms.GetTotalCountsForPrefix(HistogramName("")),
              testing::ContainerEq(expected_counts));

  histograms.ExpectUniqueSample(HistogramName("Digits"),
                                /*sample=*/8, /*count=*/1);
  histograms.ExpectUniqueSample(HistogramName("Digits.RightClickLink.Showing"),
                                /*sample=*/8, /*count=*/1);
  histograms.ExpectUniqueSample(HistogramName("Length"),
                                /*sample=*/9, /*count=*/1);
  histograms.ExpectUniqueSample(HistogramName("Length.RightClickLink.Showing"),
                                /*sample=*/9, /*count=*/1);

  // Send the number to the device in the context menu.
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
                       0);

  expected_counts[HistogramName("Digits")] = 2;
  expected_counts[HistogramName("Digits.RightClickLink.Sending")] = 1;
  expected_counts[HistogramName("Length")] = 2;
  expected_counts[HistogramName("Length.RightClickLink.Sending")] = 1;
  EXPECT_THAT(histograms.GetTotalCountsForPrefix(HistogramName("")),
              testing::ContainerEq(expected_counts));

  histograms.ExpectUniqueSample(HistogramName("Digits"),
                                /*sample=*/8, /*count=*/2);
  histograms.ExpectUniqueSample(HistogramName("Digits.RightClickLink.Sending"),
                                /*sample=*/8, /*count=*/1);
  histograms.ExpectUniqueSample(HistogramName("Length"),
                                /*sample=*/9, /*count=*/2);
  histograms.ExpectUniqueSample(HistogramName("Length.RightClickLink.Sending"),
                                /*sample=*/9, /*count=*/1);
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest,
                       ContextMenu_HighlightedText_Histograms) {
  base::HistogramTester histograms;
  Init(sync_pb::SharingSpecificFields::CLICK_TO_CALL,
       sync_pb::SharingSpecificFields::UNKNOWN);

  // Trigger a context menu for a selection with 8 digits and 9 characters.
  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitContextMenu(GURL(kNonTelUrl), kLinkText, "1234-5678");

  base::HistogramTester::CountsMap expected_counts;
  expected_counts[HistogramName("Digits")] = 1;
  expected_counts[HistogramName("Digits.RightClickSelection.Showing")] = 1;
  expected_counts[HistogramName("Length")] = 1;
  expected_counts[HistogramName("Length.RightClickSelection.Showing")] = 1;
  EXPECT_THAT(histograms.GetTotalCountsForPrefix(HistogramName("")),
              testing::ContainerEq(expected_counts));

  histograms.ExpectUniqueSample(HistogramName("Digits"),
                                /*sample=*/8, /*count=*/1);
  histograms.ExpectUniqueSample(
      HistogramName("Digits.RightClickSelection.Showing"),
      /*sample=*/8, /*count=*/1);
  histograms.ExpectUniqueSample(HistogramName("Length"),
                                /*sample=*/9, /*count=*/1);
  histograms.ExpectUniqueSample(
      HistogramName("Length.RightClickSelection.Showing"),
      /*sample=*/9, /*count=*/1);

  // Send the number to the device in the context menu.
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
                       0);

  expected_counts[HistogramName("Digits")] = 2;
  expected_counts[HistogramName("Digits.RightClickSelection.Sending")] = 1;
  expected_counts[HistogramName("Length")] = 2;
  expected_counts[HistogramName("Length.RightClickSelection.Sending")] = 1;
  EXPECT_THAT(histograms.GetTotalCountsForPrefix(HistogramName("")),
              testing::ContainerEq(expected_counts));

  histograms.ExpectUniqueSample(HistogramName("Digits"),
                                /*sample=*/8, /*count=*/2);
  histograms.ExpectUniqueSample(
      HistogramName("Digits.RightClickSelection.Sending"),
      /*sample=*/8, /*count=*/1);
  histograms.ExpectUniqueSample(HistogramName("Length"),
                                /*sample=*/9, /*count=*/2);
  histograms.ExpectUniqueSample(
      HistogramName("Length.RightClickSelection.Sending"),
      /*sample=*/9, /*count=*/1);
}

class ClickToCallBrowserTestWithContextMenuDisabled
    : public ClickToCallBrowserTestBase {
 public:
  ClickToCallBrowserTestWithContextMenuDisabled() {
    feature_list_.InitWithFeatures({kSharingDeviceRegistration, kClickToCallUI},
                                   {kClickToCallContextMenuForSelectedText});
  }
};

IN_PROC_BROWSER_TEST_F(
    ClickToCallBrowserTestWithContextMenuDisabled,
    ContextMenu_HighlightedText_DevicesAvailable_FeatureFlagOff) {
  Init(sync_pb::SharingSpecificFields::CLICK_TO_CALL,
       sync_pb::SharingSpecificFields::CLICK_TO_CALL);
  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::CLICK_TO_CALL);
  ASSERT_EQ(2u, devices.size());

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitContextMenu(GURL(kNonTelUrl), kLinkText, kTextWithPhoneNumber);

  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES));
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest, ContextMenu_UKM) {
  Init(sync_pb::SharingSpecificFields::CLICK_TO_CALL,
       sync_pb::SharingSpecificFields::UNKNOWN);
  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::CLICK_TO_CALL);
  ASSERT_EQ(1u, devices.size());

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::RunLoop run_loop;
  ukm_recorder.SetOnAddEntryCallback(
      ukm::builders::Sharing_ClickToCall::kEntryName, run_loop.QuitClosure());

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitContextMenu(GURL(kNonTelUrl), kLinkText, kTextWithPhoneNumber);

  // Check click to call items in context menu
  ASSERT_TRUE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  // Send number to device
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
                       0);

  // Expect UKM metrics to be logged
  run_loop.Run();
  std::vector<const ukm::mojom::UkmEntry*> ukm_entries =
      ukm_recorder.GetEntriesByName(
          ukm::builders::Sharing_ClickToCall::kEntryName);
  ASSERT_EQ(1u, ukm_entries.size());

  const int64_t* entry_point = ukm_recorder.GetEntryMetric(
      ukm_entries[0], ukm::builders::Sharing_ClickToCall::kEntryPointName);
  const int64_t* has_apps = ukm_recorder.GetEntryMetric(
      ukm_entries[0], ukm::builders::Sharing_ClickToCall::kHasAppsName);
  const int64_t* has_devices = ukm_recorder.GetEntryMetric(
      ukm_entries[0], ukm::builders::Sharing_ClickToCall::kHasDevicesName);
  const int64_t* selection = ukm_recorder.GetEntryMetric(
      ukm_entries[0], ukm::builders::Sharing_ClickToCall::kSelectionName);

  ASSERT_TRUE(entry_point);
  ASSERT_TRUE(has_apps);
  ASSERT_TRUE(has_devices);
  ASSERT_TRUE(selection);

  EXPECT_EQ(
      static_cast<int64_t>(SharingClickToCallEntryPoint::kRightClickSelection),
      *entry_point);
  EXPECT_EQ(true, *has_devices);
  EXPECT_EQ(static_cast<int64_t>(SharingClickToCallSelection::kDevice),
            *selection);
  // TODO(knollr): mock apps and verify |has_apps| here too.
}

class ClickToCallBrowserTestWithContextMenuFeatureDefault
    : public ClickToCallBrowserTestBase {
 public:
  ClickToCallBrowserTestWithContextMenuFeatureDefault() {
    feature_list_.InitWithFeatures({kSharingDeviceRegistration, kClickToCallUI},
                                   {});
  }
};

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest, CloseTabWithBubble) {
  Init(sync_pb::SharingSpecificFields::CLICK_TO_CALL,
       sync_pb::SharingSpecificFields::UNKNOWN);
  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::CLICK_TO_CALL);
  ASSERT_EQ(1u, devices.size());

  base::RunLoop run_loop;
  ClickToCallUiController::GetOrCreateFromWebContents(web_contents())
      ->set_on_dialog_shown_closure_for_testing(run_loop.QuitClosure());

  // Click on the tel link to trigger the bubble view.
  web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("document.querySelector('a').click();"),
      base::NullCallback());
  // Wait until the bubble is visible.
  run_loop.Run();

  // Close the tab while the bubble is opened.
  // Regression test for http://crbug.com/1000934.
  sessions_helper::CloseTab(/*browser_index=*/0, /*tab_index=*/0);
}

class ClickToCallPolicyTest
    : public policy::PolicyTest,
      public testing::WithParamInterface<ClickToCallPolicy> {
 public:
  ClickToCallPolicyTest() {
    scoped_feature_list_.InitWithFeatures(
        {kClickToCallUI, kClickToCallContextMenuForSelectedText}, {});
  }
  ~ClickToCallPolicyTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    policy::PolicyMap policies;

    const ClickToCallPolicy policy = GetParam();
    if (policy == ClickToCallPolicy::kFalse ||
        policy == ClickToCallPolicy::kTrue) {
      const bool policy_bool = (policy == ClickToCallPolicy::kTrue);
      policies.Set(policy::key::kClickToCallEnabled,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
                   std::make_unique<base::Value>(policy_bool), nullptr);
    }

    provider_.UpdateChromePolicy(policies);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ClickToCallPolicyTest, RunTest) {
  const char* kPhoneNumber = "+9876543210";
  const char* kPhoneLink = "tel:+9876543210";
  bool expected_enabled = GetParam() != ClickToCallPolicy::kFalse;
  bool expected_configured = GetParam() != ClickToCallPolicy::kNotConfigured;

  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_EQ(expected_enabled, prefs->GetBoolean(prefs::kClickToCallEnabled));
  EXPECT_EQ(expected_configured,
            prefs->IsManagedPreference(prefs::kClickToCallEnabled));

  EXPECT_EQ(expected_enabled, ShouldOfferClickToCallForURL(browser()->profile(),
                                                           GURL(kPhoneLink)));

  base::Optional<std::string> extracted =
      ExtractPhoneNumberForClickToCall(browser()->profile(), kPhoneNumber);
  if (expected_enabled)
    EXPECT_EQ(kPhoneNumber, extracted.value());
  else
    EXPECT_FALSE(extracted.has_value());
}

INSTANTIATE_TEST_SUITE_P(,
                         ClickToCallPolicyTest,
                         ::testing::Values(ClickToCallPolicy::kNotConfigured,
                                           ClickToCallPolicy::kFalse,
                                           ClickToCallPolicy::kTrue));
