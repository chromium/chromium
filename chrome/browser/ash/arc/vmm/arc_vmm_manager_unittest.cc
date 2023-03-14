// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/vmm/arc_vmm_manager.h"

#include "ash/components/arc/session/arc_service_manager.h"
#include "base/test/bind.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {
using SwapOperation = vm_tools::concierge::SwapOperation;

// The time gap between "enable" and "swapout" operation. It's depends on the
// time set in ArcVmmManager.
constexpr auto kSwapoutGap = base::Seconds(5);

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

 private:
  int enable_count_ = 0;
  int swap_out_count_ = 0;
  int disable_count_ = 0;
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

    testing_profile_ = std::make_unique<TestingProfile>();
    manager_ =
        ArcVmmManager::GetForBrowserContextForTesting(testing_profile_.get());
    manager_->set_user_id_hash("test_user_hash_id");

    concierge_client_ =
        std::make_unique<TestConciergeClient>(ash::FakeCiceroneClient::Get());
  }

  ArcVmmManager* manager() { return manager_; }
  TestConciergeClient* client() { return concierge_client_.get(); }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<TestConciergeClient> concierge_client_;
  ArcVmmManager* manager_ = nullptr;

  std::unique_ptr<ArcServiceManager> arc_service_manager_;
};

TEST_F(ArcVmmManagerTest, SwapSuccess) {
  manager()->SetSwapState(true);
  base::RunLoop().RunUntilIdle();
  // Send "ENABLE" first.
  EXPECT_EQ(1, client()->enable_count());
  EXPECT_EQ(0, client()->swap_out_count());
  EXPECT_EQ(0, client()->disable_count());

  // After seconds, send "SWAPOUT".
  task_environment_.FastForwardBy(kSwapoutGap);
  EXPECT_EQ(1, client()->enable_count());
  EXPECT_EQ(1, client()->swap_out_count());
  EXPECT_EQ(0, client()->disable_count());
}
}  // namespace arc
