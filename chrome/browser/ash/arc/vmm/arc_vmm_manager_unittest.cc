// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/vmm/arc_vmm_manager.h"
#include <memory>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/arc/vmm/arcvm_working_set_trim_executor.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {
using SwapOperation = vm_tools::concierge::SwapOperation;

// Customized FakeConciergeClient to add more complex logic on SwapVm function.
class TestConciergeClient : public ash::FakeConciergeClient {
 public:
  explicit TestConciergeClient(ash::FakeCiceroneClient* fake_cicerone_client)
      : ash::FakeConciergeClient(fake_cicerone_client) {}

  void SwapVm(const vm_tools::concierge::SwapVmRequest& request,
              chromeos::DBusMethodCallback<vm_tools::concierge::SwapVmResponse>
                  callback) override {
    vm_tools::concierge::SwapVmResponse response;
    switch (request.operation()) {
      case SwapOperation::ENABLE:
        enable_count_++;
        response.set_success(true);
        break;
      case SwapOperation::SWAPOUT:
        swap_out_count_++;
        response.set_success(true);
        break;
      case SwapOperation::DISABLE:
        disable_count_++;
        response.set_success(true);
        break;
      case SwapOperation::FORCE_ENABLE:
        force_enable_count_++;
        response.set_success(true);
        break;
      default:
        response.set_success(false);
        response.set_failure_reason("Unknown operation");
        break;
    }
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), response));
  }

  int enable_count() { return enable_count_; }
  int swap_out_count() { return swap_out_count_; }
  int disable_count() { return disable_count_; }
  int force_enable_count() { return force_enable_count_; }

 private:
  int enable_count_ = 0;
  int swap_out_count_ = 0;
  int disable_count_ = 0;
  int force_enable_count_ = 0;
};

}  // namespace

class ArcVmmManagerTest : public testing::Test {
 public:
  ArcVmmManagerTest() = default;
  ArcVmmManagerTest(const ArcVmmManagerTest&) = delete;
  ArcVmmManagerTest& operator=(const ArcVmmManagerTest&) = delete;

  ~ArcVmmManagerTest() override = default;

  void SetUp() override {
    // This is needed for setting up ArcBridge.
    arc_service_manager_ = std::make_unique<ArcServiceManager>();

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    testing_profile_ = profile_manager_->CreateTestingProfile("test_name");

    concierge_client_ =
        std::make_unique<TestConciergeClient>(ash::FakeCiceroneClient::Get());
  }

  void InitVmmManager() {
    manager_ = ArcVmmManager::GetForBrowserContextForTesting(testing_profile_);
    manager_->set_user_id_hash("test_user_hash_id");
  }

  void InitAggressiveBallonResponse() {
    vm_tools::concierge::AggressiveBalloonResponse response;
    response.set_success(true);
    client()->set_aggressive_balloon_response(response);
  }

  void SetTrimCall(bool trim_result) {
    manager()->trim_call_ = base::BindLambdaForTesting(
        [trim_result](ArcVmWorkingSetTrimExecutor::ResultCallback callback,
                      ArcVmReclaimType reclaim_type, int page_limit) {
          std::move(callback).Run(trim_result, "");
        });
  }

  ArcVmmManager* manager() { return manager_; }
  TestConciergeClient* client() { return concierge_client_.get(); }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  base::test::ScopedFeatureList scoped_features_;
  TestingPrefServiceSimple local_state_;

  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile, ExperimentalAsh> testing_profile_ = nullptr;
  std::unique_ptr<TestConciergeClient> concierge_client_;
  raw_ptr<ArcVmmManager, ExperimentalAsh> manager_ = nullptr;

  std::unique_ptr<ArcServiceManager> arc_service_manager_;
};

TEST_F(ArcVmmManagerTest, EnableSwapWhenTrimSuccess) {
  InitVmmManager();
  SetTrimCall(true);
  InitAggressiveBallonResponse();

  // Send "ENABLE".
  EXPECT_EQ(0, client()->enable_count());
  manager()->SetSwapState(SwapState::ENABLE);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, client()->enable_count());
  EXPECT_EQ(0, client()->swap_out_count());
  EXPECT_EQ(0, client()->disable_count());
}

TEST_F(ArcVmmManagerTest, NotEnableSwapWhenTrimFail) {
  InitVmmManager();
  SetTrimCall(false);
  InitAggressiveBallonResponse();

  // Send "ENABLE".
  EXPECT_EQ(0, client()->enable_count());
  manager()->SetSwapState(SwapState::ENABLE);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, client()->enable_count());
  EXPECT_EQ(0, client()->swap_out_count());
  EXPECT_EQ(0, client()->disable_count());
}

TEST_F(ArcVmmManagerTest, ForceSwapSuccess) {
  InitVmmManager();
  SetTrimCall(true);
  InitAggressiveBallonResponse();

  manager()->SetSwapState(SwapState::FORCE_ENABLE);
  base::RunLoop().RunUntilIdle();
  // Send "FORCE_ENABLE".
  EXPECT_EQ(1, client()->force_enable_count());
  EXPECT_EQ(0, client()->enable_count());
  EXPECT_EQ(0, client()->swap_out_count());
  EXPECT_EQ(0, client()->disable_count());
}

// This test verify the weak ptr safety in scheduler.
TEST_F(ArcVmmManagerTest, WeakPtrRef) {
  class TestClass {
   public:
    void add(int x) { value += x; }

    int value = 0;

    base::WeakPtrFactory<TestClass> weak_ptr_factory_{this};
  };

  TestClass* test_class = new TestClass;
  auto cb = base::BindRepeating(
      [](base::WeakPtr<TestClass> c, int v) {
        if (c) {
          c->add(v);
        }
      },
      test_class->weak_ptr_factory_.GetWeakPtr());

  EXPECT_EQ(test_class->value, 0);
  cb.Run(1);
  EXPECT_EQ(test_class->value, 1);

  delete test_class;
  cb.Run(2);
  // Expect no crash here.
}

}  // namespace arc
