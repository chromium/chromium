// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/attestation/platform_verification_flow.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using chromeos::attestation::PlatformVerificationFlow;

namespace policy {

class AttestationDevicePolicyTest
    : public DevicePolicyCrosBrowserTest,
      public chromeos::DeviceSettingsService::Observer {
 public:
    // DeviceSettingsService::Observer
  void DeviceSettingsUpdated() override { operation_complete_ = true; }

 protected:
  AttestationDevicePolicyTest() : operation_complete_(false) {}

  void SetUpInProcessBrowserTestFixture() override {
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();
    RefreshDevicePolicy();
  }

  // Refreshes device policy and waits for it to be applied.
  virtual void SyncRefreshDevicePolicy() {
    chromeos::DeviceSettingsService::Get()->AddObserver(this);
    RefreshDevicePolicy();
    WaitForAsyncOperation();
    chromeos::DeviceSettingsService::Get()->RemoveObserver(this);
  }

  enterprise_management::AttestationSettingsProto* GetDevicePolicyProto() {
    return device_policy()->payload().mutable_attestation_settings();
  }

  // A callback for PlatformVerificationFlow::ChallengePlatformKey.
  void Callback(PlatformVerificationFlow::Result result,
                const std::string& signed_data,
                const std::string& signature,
                const std::string& platform_key_certificate) {
    result_ = result;
    operation_complete_ = true;
  }

  // Synchronously do what the content protection code path does when it wants
  // to verify a Chrome OS platform.
  PlatformVerificationFlow::Result SyncContentProtectionAttestation() {
    scoped_refptr<PlatformVerificationFlow> verifier(
        new PlatformVerificationFlow(
            nullptr, nullptr, chromeos::FakeCryptohomeClient::Get(), nullptr));
    verifier->ChallengePlatformKey(
        browser()->tab_strip_model()->GetActiveWebContents(), "fake_service_id",
        "fake_challenge", base::Bind(&AttestationDevicePolicyTest::Callback,
                                     base::Unretained(this)));
    WaitForAsyncOperation();
    return result_;
  }

 private:
  bool operation_complete_;
  PlatformVerificationFlow::Result result_;

  void WaitForAsyncOperation() {
    while (!operation_complete_) {
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(10));
      base::RunLoop pump;
      pump.RunUntilIdle();
    }
    // Reset for the next call.
    operation_complete_ = false;
  }

  DISALLOW_COPY_AND_ASSIGN(AttestationDevicePolicyTest);
};

IN_PROC_BROWSER_TEST_F(AttestationDevicePolicyTest, ContentProtectionTest) {
  EXPECT_NE(PlatformVerificationFlow::POLICY_REJECTED,
            SyncContentProtectionAttestation());

  GetDevicePolicyProto()->set_content_protection_enabled(false);
  SyncRefreshDevicePolicy();

  EXPECT_EQ(PlatformVerificationFlow::POLICY_REJECTED,
            SyncContentProtectionAttestation());

  GetDevicePolicyProto()->set_content_protection_enabled(true);
  SyncRefreshDevicePolicy();

  EXPECT_NE(PlatformVerificationFlow::POLICY_REJECTED,
            SyncContentProtectionAttestation());
}

}  // namespace policy
