// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chromeos/ash/components/dbus/cec_service/cec_service_client.h"
#include "chromeos/ash/components/dbus/cec_service/fake_cec_service_client.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/features/feature_session_type.h"
#include "extensions/common/mojom/feature_session_type.mojom.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"

namespace extensions {

namespace {

constexpr char kTestAppId[] = "jabiebdnficieldhmegebckfhpfidfla";

class CecPrivateKioskApiTest : public ExtensionApiTest {
 public:
  CecPrivateKioskApiTest()
      : session_type_(ScopedCurrentFeatureSessionType(
            mojom::FeatureSessionType::kKiosk)) {}

  CecPrivateKioskApiTest(const CecPrivateKioskApiTest&) = delete;
  CecPrivateKioskApiTest& operator=(const CecPrivateKioskApiTest&) = delete;

  ~CecPrivateKioskApiTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    // In browser tests, D-Bus clients are fakes.
    cec_ =
        static_cast<ash::FakeCecServiceClient*>(ash::CecServiceClient::Get());
  }

  void TearDownOnMainThread() override {
    cec_ = nullptr;
    // Tear down will delete the fake CecService client.
    ExtensionApiTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID, kTestAppId);
    ExtensionApiTest::SetUpCommandLine(command_line);
  }

 protected:
  raw_ptr<ash::FakeCecServiceClient> cec_ = nullptr;

 private:
  std::unique_ptr<base::AutoReset<mojom::FeatureSessionType>> session_type_;
};

using CecPrivateNonKioskApiTest = ExtensionApiTest;

}  // namespace

IN_PROC_BROWSER_TEST_F(CecPrivateKioskApiTest, TestAllApiFunctions) {
  cec_->set_tv_power_states({ash::CecServiceClient::PowerState::kOn,
                             ash::CecServiceClient::PowerState::kOn});
  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener standby_call_count("standby_call_count",
                                                  ReplyBehavior::kWillReply);
  ExtensionTestMessageListener wakeup_call_count("wakeup_call_count",
                                                 ReplyBehavior::kWillReply);

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("cec_private/api")));

  ASSERT_TRUE(standby_call_count.WaitUntilSatisfied())
      << standby_call_count.message();
  standby_call_count.Reply(cec_->stand_by_call_count());

  ASSERT_TRUE(wakeup_call_count.WaitUntilSatisfied())
      << wakeup_call_count.message();
  wakeup_call_count.Reply(cec_->wake_up_call_count());

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(CecPrivateNonKioskApiTest, TestCecPrivateNotAvailable) {
  // ChromeTestExtensionLoader emits a warning that the manifest wants
  // cecPrivate permission but this is not a kiosk session. Ignore the warning
  // so we can verify the API is not available to JavaScript.
  LoadOptions load_options;
  load_options.ignore_manifest_warnings = true;
  ASSERT_TRUE(RunExtensionTest("cec_private/non_kiosk_api_not_available",
                               RunOptions(), load_options))
      << message_;
}

}  // namespace extensions
