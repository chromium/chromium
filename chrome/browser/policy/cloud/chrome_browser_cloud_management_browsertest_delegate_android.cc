// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/chrome_browser_cloud_management_browsertest_delegate_android.h"

#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#include "components/policy/core/common/android/android_combined_policy_provider.h"
#include "components/policy/core/common/android/policy_converter.h"
#include "components/policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

// Simple observer for enrollment completion. Provides an interface to wait for
// both registration confirmed and metrics recorded.
class EnrollmentObserver
    : public ChromeBrowserCloudManagementController::Observer {
 public:
  EnrollmentObserver() {
    g_browser_process->browser_policy_connector()
        ->chrome_browser_cloud_management_controller()
        ->AddObserver(this);
  }

  ~EnrollmentObserver() override {
    g_browser_process->browser_policy_connector()
        ->chrome_browser_cloud_management_controller()
        ->RemoveObserver(this);
  }

  void WaitForEnrollmentConfirmation() {
    run_loop_registration_.Run();
    run_loop_result_recording_.Run();
  }

  // ChromeBrowserCloudManagementController::Observer:
  void OnEnrollmentResultRecorded() override {
    run_loop_result_recording_.Quit();
  }

  void OnPolicyRegisterFinished(bool succeeded) override {
    run_loop_registration_.Quit();
  }

 private:
  base::RunLoop run_loop_registration_;
  base::RunLoop run_loop_result_recording_;
};

}  // namespace

void ChromeBrowserCloudManagementBrowserTestDelegateAndroid::
    MaybeCheckDialogClosingAfterPolicyRegistration(bool popup_expected) const {
  // There is no popup shown on Android, no need to check.
}

bool ChromeBrowserCloudManagementBrowserTestDelegateAndroid::
    ExpectManagerImmediatelyInitialized(bool enrollment_succeeded) const {
  // Manager is always initialized immediately on Android, so it unblocks
  // startup routines that wait for policy service to be available (e.g. FRE).
  //
  // TODO(http://crbug.com/1203435): this may no longer be needed now that we
  // use ProxyPolicyProvider as an indirection to the manager.
  return true;
}

bool ChromeBrowserCloudManagementBrowserTestDelegateAndroid::
    AcceptEmptyMachineNameOnBrowserRegistration() const {
  return true;
}

bool ChromeBrowserCloudManagementBrowserTestDelegateAndroid::
    ExpectOnStoreEventFired() const {
  return true;
}

void ChromeBrowserCloudManagementBrowserTestDelegateAndroid::
    MaybeCheckTotalBrowserCount(size_t expected_browser_count) const {
  // No checks needed, as enrollment is never required on Android.
}

void ChromeBrowserCloudManagementBrowserTestDelegateAndroid::
    MaybeWaitForEnrollmentConfirmation(const std::string& enrollment_token) {
  EnrollmentObserver enrollment_observer;

  android::AndroidCombinedPolicyProvider* platform_provider =
      static_cast<android::AndroidCombinedPolicyProvider*>(
          g_browser_process->browser_policy_connector()->GetPlatformProvider());
  ASSERT_TRUE(platform_provider);

  platform_provider
      ->GetPolicyConverterForTesting()  // IN-TEST
      ->SetPolicyValueForTesting(       // IN-TEST
          key::kCloudManagementEnrollmentToken, base::Value(enrollment_token));
  platform_provider->FlushPolicies(nullptr, nullptr);

  enrollment_observer.WaitForEnrollmentConfirmation();
}

}  // namespace policy
