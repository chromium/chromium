// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#endif

namespace extensions {

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, GetIncognitoModeAvailability) {
  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetInteger(policy::policy_prefs::kIncognitoModeAvailability, 1);

  EXPECT_TRUE(RunExtensionTest("system/get_incognito_mode_availability", {},
                               {.load_as_component = true}))
      << message_;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

class GetUpdateStatusApiTest : public ExtensionApiTest {
 public:
  GetUpdateStatusApiTest() = default;

  GetUpdateStatusApiTest(const GetUpdateStatusApiTest&) = delete;
  GetUpdateStatusApiTest& operator=(const GetUpdateStatusApiTest&) = delete;

  void SetUpInProcessBrowserTestFixture() override {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    fake_update_engine_client_ =
        ash::UpdateEngineClient::InitializeFakeForTest();
  }

  void TearDownInProcessBrowserTestFixture() override {
    ExtensionApiTest::TearDownInProcessBrowserTestFixture();
  }

 protected:
  raw_ptr<ash::FakeUpdateEngineClient, DanglingUntriaged>
      fake_update_engine_client_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(GetUpdateStatusApiTest, Progress) {
  update_engine::StatusResult status_not_available;
  status_not_available.set_current_operation(update_engine::Operation::IDLE);
  update_engine::StatusResult status_updating;
  status_updating.set_current_operation(update_engine::Operation::DOWNLOADING);
  status_updating.set_progress(0.5);
  update_engine::StatusResult status_boot_needed;
  status_boot_needed.set_current_operation(
      update_engine::Operation::UPDATED_NEED_REBOOT);

  // The fake client returns the last status in this order.
  fake_update_engine_client_->PushLastStatus(status_not_available);
  fake_update_engine_client_->PushLastStatus(status_updating);
  fake_update_engine_client_->PushLastStatus(status_boot_needed);

  ASSERT_TRUE(RunExtensionTest("system/get_update_status", {},
                               {.load_as_component = true}))
      << message_;
}

#endif

}  // namespace extensions
