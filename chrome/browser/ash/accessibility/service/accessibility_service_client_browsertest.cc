// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/accessibility/service/accessibility_service_router_factory.h"
#include "chrome/browser/ash/accessibility/service/accessibility_service_client.h"
#include "chrome/browser/ash/accessibility/service/automation_client_impl.h"
#include "chrome/browser/ash/accessibility/service/fake_accessibility_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/test/browser_test.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"

using ax::mojom::AssistiveTechnologyType;

namespace ash {

// Tests for the AccessibilityServiceClientTest using a fake service
// implemented in FakeAccessibilityService.
class AccessibilityServiceClientTest : public InProcessBrowserTest {
 public:
  AccessibilityServiceClientTest() = default;
  AccessibilityServiceClientTest(const AccessibilityServiceClientTest&) =
      delete;
  AccessibilityServiceClientTest& operator=(
      const AccessibilityServiceClientTest&) = delete;
  ~AccessibilityServiceClientTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_feature_list_.InitAndEnableFeature(features::kAccessibilityService);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Replaces normal AccessibilityService with a fake one.
    ax::AccessibilityServiceRouterFactory::GetInstanceForTest()
        ->SetTestingFactoryAndUse(
            browser()->profile(),
            base::BindRepeating(
                &AccessibilityServiceClientTest::CreateTestAccessibilityService,
                base::Unretained(this)));
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  bool ServiceHasATEnabled(AssistiveTechnologyType type) {
    std::set<AssistiveTechnologyType> enabled_ATs =
        fake_service_->GetEnabledATs();
    return enabled_ATs.find(type) != enabled_ATs.end();
  }

  bool ServiceIsBound() { return fake_service_->IsBound(); }

  void ToggleAutomationEnabled(AccessibilityServiceClient& client,
                               bool enabled) {
    if (enabled)
      client.automation_client_->Enable(base::DoNothing());
    else
      client.automation_client_->Disable();
  }

  // Unowned.
  FakeAccessibilityService* fake_service_ = nullptr;

 private:
  std::unique_ptr<KeyedService> CreateTestAccessibilityService(
      content::BrowserContext* context) {
    std::unique_ptr<FakeAccessibilityService> fake_service =
        std::make_unique<FakeAccessibilityService>();
    fake_service_ = fake_service.get();
    return std::move(fake_service);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that nothing crashes if the profile isn't set yet.
// Note that this should never happen as enabling/disabling
// features from AccessibilityManager will only happen when
// there is a profile.
IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       DoesNotCrashWithNoProfile) {
  AccessibilityServiceClient client;
  client.SetChromeVoxEnabled(true);

  client.SetProfile(nullptr);
  client.SetSelectToSpeakEnabled(true);

  EXPECT_FALSE(ServiceIsBound());
}

// AccessibilityServiceClient shouldn't try to use the service
// when features are all disabled.
IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       DoesNotCreateServiceForDisabledFeatures) {
  AccessibilityServiceClient client;
  EXPECT_FALSE(ServiceIsBound());

  client.SetProfile(browser()->profile());
  EXPECT_FALSE(ServiceIsBound());

  client.SetChromeVoxEnabled(false);
  EXPECT_FALSE(ServiceIsBound());

  client.SetDictationEnabled(false);
  EXPECT_FALSE(ServiceIsBound());
}

// Test that any previously enabled features are copied when
// the profile changes.
IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       CopiesFeaturesWhenProfileChanges) {
  AccessibilityServiceClient client;
  client.SetChromeVoxEnabled(true);
  client.SetSwitchAccessEnabled(true);
  client.SetAutoclickEnabled(true);
  client.SetAutoclickEnabled(false);

  // Service isn't constructed yet.
  EXPECT_FALSE(ServiceIsBound());

  client.SetProfile(browser()->profile());

  ASSERT_TRUE(ServiceIsBound());
  EXPECT_TRUE(ServiceHasATEnabled(AssistiveTechnologyType::kChromeVox));
  EXPECT_TRUE(ServiceHasATEnabled(AssistiveTechnologyType::kSwitchAccess));
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kAutoClick));
}

// Test that the AccessibilityServiceClient can toggle features in the service
// using the mojom interface.
IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       TogglesAccessibilityFeatures) {
  AccessibilityServiceClient client;
  client.SetProfile(browser()->profile());
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kChromeVox));
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kSelectToSpeak));
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kSwitchAccess));
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kAutoClick));
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kDictation));
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kMagnifier));

  // The first time we enable/disable an AT, the AT controller should be bound
  // with the enabled AT type.
  client.SetChromeVoxEnabled(true);
  EXPECT_TRUE(ServiceHasATEnabled(AssistiveTechnologyType::kChromeVox));
  client.SetSelectToSpeakEnabled(true);
  fake_service_->WaitForATChanged();
  EXPECT_TRUE(ServiceHasATEnabled(AssistiveTechnologyType::kSelectToSpeak));
  client.SetSwitchAccessEnabled(true);
  fake_service_->WaitForATChanged();
  EXPECT_TRUE(ServiceHasATEnabled(AssistiveTechnologyType::kSwitchAccess));
  client.SetAutoclickEnabled(true);
  fake_service_->WaitForATChanged();
  EXPECT_TRUE(ServiceHasATEnabled(AssistiveTechnologyType::kAutoClick));
  client.SetDictationEnabled(true);
  fake_service_->WaitForATChanged();
  EXPECT_TRUE(ServiceHasATEnabled(AssistiveTechnologyType::kDictation));
  client.SetMagnifierEnabled(true);
  fake_service_->WaitForATChanged();
  EXPECT_TRUE(ServiceHasATEnabled(AssistiveTechnologyType::kMagnifier));
  client.SetChromeVoxEnabled(false);
  fake_service_->WaitForATChanged();
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kChromeVox));
  client.SetSelectToSpeakEnabled(false);
  fake_service_->WaitForATChanged();
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kSelectToSpeak));
  client.SetSwitchAccessEnabled(false);
  fake_service_->WaitForATChanged();
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kSwitchAccess));
  client.SetAutoclickEnabled(false);
  fake_service_->WaitForATChanged();
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kAutoClick));
  client.SetDictationEnabled(false);
  fake_service_->WaitForATChanged();
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kDictation));
  client.SetMagnifierEnabled(false);
  fake_service_->WaitForATChanged();
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kMagnifier));
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       SendsAutomationToTheService) {
  AccessibilityServiceClient client;
  client.SetProfile(browser()->profile());
  // Enable an assistive technology. The service will not be started until
  // some AT needs it.
  client.SetChromeVoxEnabled(true);
  EXPECT_TRUE(ServiceHasATEnabled(AssistiveTechnologyType::kChromeVox));

  // The service may bind multiple Automations to the AutomationClient.
  for (int i = 0; i < 3; i++) {
    fake_service_->BindAnotherAutomation();
  }

  // TODO(crbug.com/1355633): Replace once mojom to Enable lands.
  ToggleAutomationEnabled(client, true);
  // Enable can be called multiple times (once for each bound Automation)
  // with no bad effects.
  // fake_service_->AutomationClientEnable(true);

  // Real accessibility events should have come through.
  fake_service_->WaitForAutomationEvents();

  // TODO(crbug.com/1355633): Replace once mojom to Disable lands.
  ToggleAutomationEnabled(client, false);
  // Disabling multiple times has no bad effect.
  // fake_service_->AutomationClientEnable(false);
}
}  // namespace ash
