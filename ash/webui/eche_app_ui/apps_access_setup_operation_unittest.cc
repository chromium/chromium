// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/apps_access_setup_operation.h"

#include <ostream>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace eche_app {

class FakeDelegate : public AppsAccessSetupOperation::Delegate {
 public:
  FakeDelegate() = default;
  ~FakeDelegate() override = default;

  size_t apps_status_change_num_calls() const {
    return apps_status_change_num_calls_;
  }

  AppsAccessSetupOperation::Status get_last_apps_status_() const {
    return last_apps_status_;
  }

 private:
  // AppsAccessSetupOperation::Delegate:
  void OnAppsStatusChange(
      AppsAccessSetupOperation::Status new_status) override {
    ++apps_status_change_num_calls_;
    last_apps_status_ = new_status;
  }

  size_t apps_status_change_num_calls_ = 0;
  AppsAccessSetupOperation::Status last_apps_status_;
};

class AppsAccessSetupOperationTest : public testing::Test {
 protected:
  AppsAccessSetupOperationTest()
      : fake_delegate_(std::make_unique<FakeDelegate>()) {}
  AppsAccessSetupOperationTest(const AppsAccessSetupOperationTest&) = delete;
  AppsAccessSetupOperationTest& operator=(const AppsAccessSetupOperationTest&) =
      delete;
  ~AppsAccessSetupOperationTest() override = default;

  void SetUp() override {
    apps_access_setup_operation_ =
        base::WrapUnique(new AppsAccessSetupOperation(
            fake_delegate_.get(),
            base::BindOnce(&AppsAccessSetupOperationTest::OnSetupOperationDone,
                           weak_ptr_factory_.GetWeakPtr())));
  }

  void TearDown() override {
    apps_access_setup_operation_.reset();
    fake_delegate_.reset();
  }

  void NotifyAppsStatusChanged(AppsAccessSetupOperation::Status status) {
    apps_access_setup_operation_->NotifyAppsStatusChanged(status);
  }

  void OnSetupOperationDone() {
    // do nothing
  }

  size_t GetNumAppsStatusCanageCalls() const {
    return fake_delegate_->apps_status_change_num_calls();
  }

  AppsAccessSetupOperation::Status GetLastAppsStatus() const {
    return fake_delegate_->get_last_apps_status_();
  }

  std::unique_ptr<AppsAccessSetupOperation> apps_access_setup_operation_;
  std::unique_ptr<FakeDelegate> fake_delegate_;
  base::WeakPtrFactory<AppsAccessSetupOperationTest> weak_ptr_factory_{this};
};

TEST_F(AppsAccessSetupOperationTest, OnAppsStatusChange) {
  NotifyAppsStatusChanged(AppsAccessSetupOperation::Status::kConnecting);

  EXPECT_EQ(1u, GetNumAppsStatusCanageCalls());
  EXPECT_EQ(AppsAccessSetupOperation::Status::kConnecting, GetLastAppsStatus());

  NotifyAppsStatusChanged(
      AppsAccessSetupOperation::Status::kCompletedSuccessfully);

  EXPECT_EQ(2u, GetNumAppsStatusCanageCalls());
  EXPECT_EQ(AppsAccessSetupOperation::Status::kCompletedSuccessfully,
            GetLastAppsStatus());
}

TEST_F(AppsAccessSetupOperationTest, IsFinalStatus) {
  EXPECT_EQ(false, AppsAccessSetupOperation::IsFinalStatus(
                       AppsAccessSetupOperation::Status::kConnecting));
  EXPECT_EQ(true, AppsAccessSetupOperation::IsFinalStatus(
                      AppsAccessSetupOperation::Status::kTimedOutConnecting));
  EXPECT_EQ(true,
            AppsAccessSetupOperation::IsFinalStatus(
                AppsAccessSetupOperation::Status::kConnectionDisconnected));
  EXPECT_EQ(false, AppsAccessSetupOperation::IsFinalStatus(
                       AppsAccessSetupOperation::Status::
                           kSentMessageToPhoneAndWaitingForResponse));
  EXPECT_EQ(true,
            AppsAccessSetupOperation::IsFinalStatus(
                AppsAccessSetupOperation::Status::kCompletedSuccessfully));
}

}  // namespace eche_app
}  // namespace ash
