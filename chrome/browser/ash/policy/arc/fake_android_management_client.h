// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ARC_FAKE_ANDROID_MANAGEMENT_CLIENT_H_
#define CHROME_BROWSER_ASH_POLICY_ARC_FAKE_ANDROID_MANAGEMENT_CLIENT_H_

#include "chrome/browser/ash/policy/arc/android_management_client.h"

namespace policy {

// Fake implementation of the AndroidManagementClient interface.
class FakeAndroidManagementClient : public AndroidManagementClient {
 public:
  FakeAndroidManagementClient();
  FakeAndroidManagementClient(const FakeAndroidManagementClient&) = delete;
  FakeAndroidManagementClient& operator=(const FakeAndroidManagementClient&) =
      delete;
  ~FakeAndroidManagementClient() override;

  int start_check_android_management_call_count() const {
    return start_check_android_management_call_count_;
  }

  void SetResult(Result result) { result_ = result; }

  // AndroidManagementClient override:
  void StartCheckAndroidManagement(StatusCallback callback) override;

 private:
  int start_check_android_management_call_count_ = 0;
  Result result_ = Result::ERROR;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ARC_FAKE_ANDROID_MANAGEMENT_CLIENT_H_
