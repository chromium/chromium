// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/vmm/arc_vmm_manager.h"
#include <memory>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/vmm/arcvm_working_set_trim_executor.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
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
  static void Initialize() {
    // Shut down stale ConciergeClient if any. See b/294290463.
    if (ash::ConciergeClient::Get()) {
      ash::ConciergeClient::Shutdown();
    }
    new TestConciergeClient(ash::FakeCiceroneClient::Get());
  }

  static void Shutdown() { ash::ConciergeClient::Shutdown(); }

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

  void SetAggressiveBalloonLatencyAndResponse(
      std::optional<base::TimeDelta> latency,
      std::optional<vm_tools::concierge::AggressiveBalloonResponse> response) {
    aggressive_balloon_latency_ = latency;
    aggressive_balloon_response_ = response;
  }

  void AggressiveBalloon(
      const vm_tools::concierge::AggressiveBalloonRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::concierge::AggressiveBalloonResponse> callback) override {
    if (!aggressive_balloon_latency_.has_value()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), aggressive_balloon_response_));
      return;
    }
    if (aggressive_balloon_timer_.IsRunning()) {
      LOG(WARNING) << "Already waiting for last AggressiveBalloon return.";
    }
    aggressive_balloon_timer_.Start(
        FROM_HERE, aggressive_balloon_latency_.value(),
        base::BindOnce(std::move(callback), aggressive_balloon_response_));
  }

  int enable_count() { return enable_count_; }
  int swap_out_count() { return swap_out_count_; }
  int disable_count() { return disable_count_; }
  int force_enable_count() { return force_enable_count_; }

 private:
  explicit TestConciergeClient(ash::FakeCiceroneClient* fake_cicerone_client)
      : ash::FakeConciergeClient(fake_cicerone_client) {}

  std::optional<base::TimeDelta> aggressive_balloon_latency_;
  std::optional<vm_tools::concierge::AggressiveBalloonResponse>
      aggressive_balloon_response_;

  int enable_count_ = 0;
  int swap_out_count_ = 0;
  int disable_count_ = 0;
  int force_enable_count_ = 0;
  base::OneShotTimer aggressive_balloon_timer_;
};

}  // namespace

class ArcVmmManagerTest : public testing::Test {
 public:
  ArcVmmManagerTest() = default;
  ArcVmmManagerTest(const ArcVmmManagerTest&) = delete;
  ArcVmmManagerTest& operator=(const ArcVmmManagerTest&) = delete;

  ~ArcVmmManagerTest() override = default;

  void SetUp() override {
    TestConciergeClient::Initialize();
    // This is needed for setting up ArcBridge.
    arc_service_manager_ = std::make_unique<ArcServiceManager>();

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    testing_profile_ = profile_manager_->CreateTestingProfile("test_name");

    trim_type_reclaim_counter_ = 0;
    trim_type_drop_pages_counter_ = 0;
  }

  void TearDown() override {
    profile_manager_.reset();
    arc_service_manager_.reset();
    TestConciergeClient::Shutdown();
  }

  void EnableAndConnectArcVm() {
    auto* command_line = base::CommandLine::ForCurrentProcess();
    command_line->InitFromArgv({"", "--enable-arcvm"});
    if (manager_) {
      manager_->OnConnectionReady();
    }
  }

  void InitVmmManager() {
    manager_ = ArcVmmManager::GetForBrowserContextForTesting(testing_profile_);
    manager_->set_user_id_hash("test_user_hash_id");
  }

  void InitAggressiveBallonResponse(bool delay_response) {
    vm_tools::concierge::AggressiveBalloonResponse response;
    response.set_success(true);
    if (delay_response) {
      client()->SetAggressiveBalloonLatencyAndResponse(base::Seconds(5),
                                                       response);
    } else {
      client()->SetAggressiveBalloonLatencyAndResponse(std::nullopt, response);
    }
  }

  void InitEmptyAggressiveBallonResponse() {
    client()->SetAggressiveBalloonLatencyAndResponse(std::nullopt,
                                                     std::nullopt);
  }

  void SetTrimCall(bool trim_result) {
    manager()->trim_call_ = base::BindLambdaForTesting(
        [trim_result, this](
            ArcVmWorkingSetTrimExecutor::ResultCallback callback,
            ArcVmReclaimType reclaim_type, int page_limit) {
          if (reclaim_type == ArcVmReclaimType::kReclaimAllGuestOnly) {
            trim_type_reclaim_counter_++;
          } else if (reclaim_type ==
                     ArcVmReclaimType::kReclaimGuestPageCaches) {
            trim_type_drop_pages_counter_++;
          }
          std::move(callback).Run(trim_result, "");
        });
  }

  void SendVmSwappingSignal(const std::string vm_name, bool out) {
    vm_tools::concierge::VmSwappingSignal signal;
    signal.set_name(vm_name);
    signal.set_state(out ? vm_tools::concierge::SWAPPING_OUT
                         : vm_tools::concierge::SWAPPING_IN);
    for (auto& observer : client()->vm_observer_list()) {
      observer.OnVmSwapping(signal);
    }
  }

  ArcVmmManager* manager() { return manager_; }
  TestConciergeClient* client() {
    return static_cast<TestConciergeClient*>(ash::ConciergeClient::Get());
  }

  int reclaim_guest_conter() { return trim_type_reclaim_counter_; }
  int drop_pages_counter() { return trim_type_drop_pages_counter_; }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  int trim_type_reclaim_counter_;
  int trim_type_drop_pages_counter_;

  base::test::ScopedFeatureList scoped_features_;
  TestingPrefServiceSimple local_state_;

  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile, DanglingUntriaged> testing_profile_ = nullptr;
  raw_ptr<ArcVmmManager, DanglingUntriaged> manager_ = nullptr;

  std::unique_ptr<ArcServiceManager> arc_service_manager_;
};

TEST_F(ArcVmmManagerTest, DBusFailedNoCrash) {
  InitVmmManager();
  EnableAndConnectArcVm();
  SetTrimCall(true);
  InitEmptyAggressiveBallonResponse();

  manager()->SetSwapState(SwapState::ENABLE);
  base::RunLoop().RunUntilIdle();

  // No crash when aggressive ballon failed.
}

TEST_F(ArcVmmManagerTest, EnableSwapWhenTrimSuccess) {
  InitVmmManager();
  EnableAndConnectArcVm();
  SetTrimCall(true);
  InitAggressiveBallonResponse(false);

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
  EnableAndConnectArcVm();
  SetTrimCall(false);
  InitAggressiveBallonResponse(false);

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
  EnableAndConnectArcVm();
  SetTrimCall(true);
  InitAggressiveBallonResponse(false);

  manager()->SetSwapState(SwapState::FORCE_ENABLE);
  base::RunLoop().RunUntilIdle();
  // Send "FORCE_ENABLE".
  EXPECT_EQ(1, client()->force_enable_count());
  EXPECT_EQ(0, client()->enable_count());
  EXPECT_EQ(0, client()->swap_out_count());
  EXPECT_EQ(0, client()->disable_count());
}

TEST_F(ArcVmmManagerTest, NotSendSwapRequestIfArcNotReady) {
  InitVmmManager();
  SetTrimCall(true);
  InitAggressiveBallonResponse(false);

  manager()->SetSwapState(SwapState::FORCE_ENABLE);
  base::RunLoop().RunUntilIdle();
  // Not send "FORCE_ENABLE".
  EXPECT_EQ(0, client()->force_enable_count());
  EXPECT_EQ(0, client()->enable_count());
  EXPECT_EQ(0, client()->swap_out_count());
  EXPECT_EQ(0, client()->disable_count());
}

TEST_F(ArcVmmManagerTest, DropCachesAfterEnableSuccess) {
  InitVmmManager();
  EnableAndConnectArcVm();
  SetTrimCall(true);
  InitAggressiveBallonResponse(false);

  manager()->SetSwapState(SwapState::FORCE_ENABLE);
  base::RunLoop().RunUntilIdle();
  // Send "FORCE_ENABLE".
  EXPECT_EQ(1, client()->force_enable_count());
  EXPECT_EQ(0, client()->enable_count());
  EXPECT_EQ(0, client()->swap_out_count());
  EXPECT_EQ(0, client()->disable_count());

  EXPECT_EQ(1, reclaim_guest_conter());
  EXPECT_EQ(1, drop_pages_counter());
}

TEST_F(ArcVmmManagerTest, EnableSwapRequestWillEnableHeartbeat) {
  InitVmmManager();
  EnableAndConnectArcVm();
  SetTrimCall(true);
  InitAggressiveBallonResponse(false);

  manager()->SetSwapState(SwapState::FORCE_ENABLE);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, client()->force_enable_count());
  EXPECT_EQ(0, client()->enable_count());
  EXPECT_EQ(0, client()->swap_out_count());
  EXPECT_EQ(0, client()->disable_count());

  task_environment_.FastForwardBy(kVmmSwapTrimInterval.Get());
  EXPECT_EQ(2, client()->force_enable_count());
  EXPECT_EQ(0, client()->enable_count());
  EXPECT_EQ(0, client()->swap_out_count());
  EXPECT_EQ(0, client()->disable_count());
  task_environment_.RunUntilIdle();

  task_environment_.FastForwardBy(kVmmSwapTrimInterval.Get());
  EXPECT_EQ(3, client()->force_enable_count());
  EXPECT_EQ(0, client()->enable_count());
  EXPECT_EQ(0, client()->swap_out_count());
  EXPECT_EQ(0, client()->disable_count());
  task_environment_.RunUntilIdle();

  manager()->SetSwapState(SwapState::ENABLE);
  task_environment_.RunUntilIdle();
  // Send "ENABLE", should overwrite original timer.
  EXPECT_EQ(3, client()->force_enable_count());
  EXPECT_EQ(1, client()->enable_count());
  EXPECT_EQ(0, client()->swap_out_count());
  EXPECT_EQ(0, client()->disable_count());

  task_environment_.FastForwardBy(kVmmSwapTrimInterval.Get());
  EXPECT_EQ(3, client()->force_enable_count());
  EXPECT_EQ(2, client()->enable_count());
  EXPECT_EQ(0, client()->swap_out_count());
  EXPECT_EQ(0, client()->disable_count());
  task_environment_.RunUntilIdle();

  task_environment_.FastForwardBy(kVmmSwapTrimInterval.Get());
  EXPECT_EQ(3, client()->force_enable_count());
  EXPECT_EQ(3, client()->enable_count());
  EXPECT_EQ(0, client()->swap_out_count());
  EXPECT_EQ(0, client()->disable_count());
  task_environment_.RunUntilIdle();
}

TEST_F(ArcVmmManagerTest, NotResendSameStateRequestButHeartbeat) {
  InitVmmManager();
  EnableAndConnectArcVm();
  SetTrimCall(true);
  InitAggressiveBallonResponse(false);

  manager()->SetSwapState(SwapState::ENABLE);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, client()->force_enable_count());
  EXPECT_EQ(1, client()->enable_count());
  EXPECT_EQ(0, client()->swap_out_count());
  EXPECT_EQ(0, client()->disable_count());

  // Not resend same request, but leave to next heart beat.
  manager()->SetSwapState(SwapState::ENABLE);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, client()->force_enable_count());
  EXPECT_EQ(1, client()->enable_count());
  EXPECT_EQ(0, client()->swap_out_count());
  EXPECT_EQ(0, client()->disable_count());

  task_environment_.FastForwardBy(base::Seconds(1));
  // Not resend same request, but leave to next heart beat.
  manager()->SetSwapState(SwapState::ENABLE);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, client()->force_enable_count());
  EXPECT_EQ(1, client()->enable_count());
  EXPECT_EQ(0, client()->swap_out_count());
  EXPECT_EQ(0, client()->disable_count());

  task_environment_.FastForwardBy(kVmmSwapTrimInterval.Get());
  EXPECT_EQ(0, client()->force_enable_count());
  EXPECT_EQ(2, client()->enable_count());
  EXPECT_EQ(0, client()->swap_out_count());
  EXPECT_EQ(0, client()->disable_count());
  task_environment_.RunUntilIdle();
}

TEST_F(ArcVmmManagerTest, ReForceEnable) {
  InitVmmManager();
  EnableAndConnectArcVm();
  SetTrimCall(true);
  InitAggressiveBallonResponse(false);

  manager()->SetSwapState(SwapState::FORCE_ENABLE);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, client()->force_enable_count());
  EXPECT_EQ(0, client()->enable_count());
  EXPECT_EQ(0, client()->swap_out_count());
  EXPECT_EQ(0, client()->disable_count());

  task_environment_.FastForwardBy(base::Seconds(10));
  // Disable.
  manager()->SetSwapState(SwapState::DISABLE);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, client()->force_enable_count());
  EXPECT_EQ(0, client()->enable_count());
  EXPECT_EQ(0, client()->swap_out_count());
  EXPECT_EQ(1, client()->disable_count());

  task_environment_.FastForwardBy(base::Seconds(10));
  // Re-enable, expect re-send force_enable to concierge.
  manager()->SetSwapState(SwapState::FORCE_ENABLE);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(2, client()->force_enable_count());
  EXPECT_EQ(0, client()->enable_count());
  EXPECT_EQ(0, client()->swap_out_count());
  EXPECT_EQ(1, client()->disable_count());

  task_environment_.FastForwardBy(base::Seconds(10));
  // Re-disable, expect re-send disable to concierge.
  manager()->SetSwapState(SwapState::DISABLE);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(2, client()->force_enable_count());
  EXPECT_EQ(0, client()->enable_count());
  EXPECT_EQ(0, client()->swap_out_count());
  EXPECT_EQ(2, client()->disable_count());
}

TEST_F(ArcVmmManagerTest, ForceEnableAlwaysSendCall) {
  InitVmmManager();
  EnableAndConnectArcVm();
  SetTrimCall(true);
  InitAggressiveBallonResponse(false);

  manager()->SetSwapState(SwapState::FORCE_ENABLE);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, client()->force_enable_count());
  EXPECT_EQ(0, client()->enable_count());
  EXPECT_EQ(0, client()->swap_out_count());
  EXPECT_EQ(0, client()->disable_count());

  task_environment_.FastForwardBy(base::Seconds(10));
  // Re-enable, expect re-send force_enable to concierge.
  manager()->SetSwapState(SwapState::FORCE_ENABLE);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(2, client()->force_enable_count());
  EXPECT_EQ(0, client()->enable_count());
  EXPECT_EQ(0, client()->swap_out_count());
  EXPECT_EQ(0, client()->disable_count());

  task_environment_.FastForwardBy(base::Seconds(10));
  // Re-enable, expect re-send force_enable to concierge.
  manager()->SetSwapState(SwapState::FORCE_ENABLE);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(3, client()->force_enable_count());
  EXPECT_EQ(0, client()->enable_count());
  EXPECT_EQ(0, client()->swap_out_count());
  EXPECT_EQ(0, client()->disable_count());
}

TEST_F(ArcVmmManagerTest, EnableAndDisableRaceCondition) {
  InitVmmManager();
  EnableAndConnectArcVm();
  SetTrimCall(true);

  // Mock real system, aggressive balloon need take time.
  InitAggressiveBallonResponse(true);

  manager()->SetSwapState(SwapState::FORCE_ENABLE);
  task_environment_.FastForwardBy(base::Seconds(1));
  // The request haven't been sent since the aggressive balloon haven't
  // finished.
  EXPECT_EQ(0, client()->force_enable_count());
  EXPECT_EQ(0, client()->enable_count());
  EXPECT_EQ(0, client()->swap_out_count());
  EXPECT_EQ(0, client()->disable_count());

  // Disable immediately.
  manager()->SetSwapState(SwapState::DISABLE);
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(0, client()->force_enable_count());
  EXPECT_EQ(0, client()->enable_count());
  EXPECT_EQ(0, client()->swap_out_count());
  // Expect the disable will be sent immediately.
  EXPECT_EQ(1, client()->disable_count());

  task_environment_.FastForwardBy(base::Minutes(1));
  // Still not expect any enable request be sent.
  EXPECT_EQ(0, client()->force_enable_count());
  EXPECT_EQ(0, client()->enable_count());
  EXPECT_EQ(0, client()->swap_out_count());
  EXPECT_EQ(1, client()->disable_count());
}

TEST_F(ArcVmmManagerTest, ObserveSwappingInAndOut) {
  InitVmmManager();
  EnableAndConnectArcVm();
  SetTrimCall(true);
  class TestObs : public ArcVmmManager::Observer {
   public:
    void OnArcVmSwappingIn() override { swapping_in_++; }
    void OnArcVmSwappingOut() override { swapping_out_++; }

    int swapping_in() { return swapping_in_; }
    int swapping_out() { return swapping_out_; }

   private:
    int swapping_in_ = 0;
    int swapping_out_ = 0;
  } test_observer;
  manager()->AddObserver(&test_observer);

  SendVmSwappingSignal(kArcVmName, /*out=*/true);
  EXPECT_EQ(1, test_observer.swapping_out());
  EXPECT_EQ(0, test_observer.swapping_in());

  // Not response no-arcvm swapping signal.
  SendVmSwappingSignal("not_arcvm", /*out=*/true);
  EXPECT_EQ(1, test_observer.swapping_out());
  EXPECT_EQ(0, test_observer.swapping_in());

  SendVmSwappingSignal(kArcVmName, /*out=*/false);
  EXPECT_EQ(1, test_observer.swapping_out());
  EXPECT_EQ(1, test_observer.swapping_in());

  SendVmSwappingSignal("not_arcvm", /*out=*/false);
  EXPECT_EQ(1, test_observer.swapping_out());
  EXPECT_EQ(1, test_observer.swapping_in());
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
