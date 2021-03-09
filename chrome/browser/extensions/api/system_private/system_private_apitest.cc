// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_update_engine_client.h"

using chromeos::UpdateEngineClient;
#endif

namespace extensions {

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, GetIncognitoModeAvailability) {
  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetInteger(prefs::kIncognitoModeAvailability, 1);

  EXPECT_TRUE(
      RunExtensionTest({.name = "system/get_incognito_mode_availability",
                        .load_as_component = true}))
      << message_;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

class GetUpdateStatusApiTest : public ExtensionApiTest {
 public:
  GetUpdateStatusApiTest() : fake_update_engine_client_(NULL) {}

  void SetUpInProcessBrowserTestFixture() override {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    fake_update_engine_client_ = new chromeos::FakeUpdateEngineClient;
    chromeos::DBusThreadManager::GetSetterForTesting()->SetUpdateEngineClient(
        std::unique_ptr<UpdateEngineClient>(fake_update_engine_client_));
  }

  void TearDownInProcessBrowserTestFixture() override {
    ExtensionApiTest::TearDownInProcessBrowserTestFixture();
  }

 protected:
  chromeos::FakeUpdateEngineClient* fake_update_engine_client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GetUpdateStatusApiTest);
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

  ASSERT_TRUE(RunExtensionTest(
      {.name = "system/get_update_status", .load_as_component = true}))
      << message_;
}

#endif

}  // namespace extensions
