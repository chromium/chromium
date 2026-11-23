// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/tailored_security/chrome_tailored_security_service.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/tailored_security/tailored_security_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toasts/test/mock_toast_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/views/widget/any_widget_observer.h"

namespace safe_browsing {

class ChromeTailoredSecurityServiceBrowserTest : public InProcessBrowserTest {
 public:
  ChromeTailoredSecurityServiceBrowserTest() {
    feature_list_.InitAndEnableFeature(kEsbAsASyncedSetting);
    toast_controller_override_ =
        BrowserWindowFeatures::GetUserDataFactoryForTesting()
            .AddOverrideForTesting(
                base::BindRepeating([](BrowserWindowInterface& browser) {
                  return std::make_unique<
                      testing::NiceMock<MockToastController>>(&browser);
                }));
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  ui::UserDataFactory::ScopedOverride toast_controller_override_;
};

IN_PROC_BROWSER_TEST_F(ChromeTailoredSecurityServiceBrowserTest,
                       OnSyncNotificationTriggersDialogButSuppressesToast) {
  base::UserActionTester uat;
  Profile* profile = browser()->GetProfile();
  ChromeTailoredSecurityService* service =
      static_cast<ChromeTailoredSecurityService*>(
          TailoredSecurityServiceFactory::GetForProfile(profile));

  // Expect no generic toasts during sync notification handling.
  auto* controller =
      static_cast<MockToastController*>(ToastController::From(browser()));
  ASSERT_TRUE(controller);
  EXPECT_CALL(*controller, MaybeShowToast(testing::_)).Times(0);

  // Observe the Tailored Security dialog.
  views::NamedWidgetShownWaiter waiter(
      views::test::AnyWidgetTestPasskey{},
      safe_browsing::kTailoredSecurityNoticeDialog);

  // Trigger the sync notification.
  service->OnSyncNotificationMessageRequest(true);

  // Verify the Tailored Security dialog is shown.
  EXPECT_TRUE(waiter.WaitIfNeededAndGet());
  EXPECT_EQ(
      uat.GetActionCount("SafeBrowsing.AccountIntegration.EnabledDialog.Shown"),
      1);
}

}  // namespace safe_browsing
