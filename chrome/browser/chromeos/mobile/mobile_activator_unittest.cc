// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/mobile/mobile_activator.h"

#include <stddef.h>
#include <utility>

#include "base/macros.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using std::string;

using content::BrowserThread;
using testing::_;
using testing::Eq;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::WithArgs;

namespace {

const char kTestServicePath[] = "/a/service/path";

const size_t kNumOTASPStates = 3;

chromeos::MobileActivator::PlanActivationState kOTASPStates[kNumOTASPStates] = {
  chromeos::MobileActivator::PLAN_ACTIVATION_TRYING_OTASP,
  chromeos::MobileActivator::PLAN_ACTIVATION_INITIATING_ACTIVATION,
  chromeos::MobileActivator::PLAN_ACTIVATION_OTASP,
};

}  // namespace
namespace chromeos {

class TestMobileActivator : public MobileActivator {
 public:
  explicit TestMobileActivator(NetworkState* cellular_network) :
        cellular_network_(cellular_network) {
    // Provide reasonable defaults for basic things we're usually not testing.
    ON_CALL(*this, ChangeState(_, _, _))
        .WillByDefault(WithArgs<1>(
            Invoke(this, &TestMobileActivator::set_state_for_test)));
  }
  virtual ~TestMobileActivator() {}

  MOCK_METHOD3(RequestCellularActivation,
               void(const NetworkState*,
                    const base::Closure&,
                    const network_handler::ErrorCallback&));
  MOCK_METHOD3(ChangeState,
               void(const NetworkState*,
                    MobileActivator::PlanActivationState,
                    MobileActivator::ActivationError));
  MOCK_METHOD0(GetDefaultNetwork, const NetworkState*());
  MOCK_METHOD1(EvaluateCellularNetwork, void(const NetworkState*));
  MOCK_METHOD0(SignalCellularPlanPayment, void(void));
  MOCK_METHOD0(StartOTASPTimer, void(void));
  MOCK_CONST_METHOD0(HasRecentCellularPlanPayment, bool(void));
  MOCK_METHOD1(ConnectNetwork, void(const NetworkState*));

  virtual const NetworkState* GetNetworkState(const std::string& service_path) {
    return cellular_network_;
  }

  void InvokeStartActivation() {
    StartActivation();
  }

  void InvokeHandlePortalLoaded(bool success) {
    HandlePortalLoaded(success);
  }

  void InvokeHandleSetTransactionStatus(bool success) {
    HandleSetTransactionStatus(success);
  }

  PlanActivationState InvokePickNextState(const NetworkState* network) const {
    return PickNextState(network);
  }

  void InvokeChangeState(const NetworkState* network,
                         MobileActivator::PlanActivationState new_state) {
    MobileActivator::ChangeState(network, new_state,
                                 MobileActivator::ActivationError::kNone);
  }

 private:
  void DCheckOnThread(const BrowserThread::ID id) const {}

  NetworkState* cellular_network_;

  DISALLOW_COPY_AND_ASSIGN(TestMobileActivator);
};

class MobileActivatorTest : public testing::Test {
 public:
  MobileActivatorTest()
      : cellular_network_(string(kTestServicePath)),
        mobile_activator_(&cellular_network_) {
    cellular_network_.PropertyChanged(shill::kTypeProperty,
                                      base::Value(shill::kTypeCellular));
  }
  ~MobileActivatorTest() override {}

 protected:
  void SetUp() override {
    DBusThreadManager::Initialize();
    NetworkHandler::Initialize();
  }
  void TearDown() override {
    mobile_activator_.TerminateActivation();
    NetworkHandler::Shutdown();
    DBusThreadManager::Shutdown();
  }

  void set_activator_state(const MobileActivator::PlanActivationState state) {
    mobile_activator_.set_state_for_test(state);
  }
  void set_network_activation_type(const std::string& activation_type) {
    cellular_network_.activation_type_ = activation_type;
  }
  void set_network_activation_state(const std::string& activation_state) {
    cellular_network_.activation_state_ = activation_state;
  }
  void set_connection_state(const std::string& state) {
    cellular_network_.visible_ = true;
    cellular_network_.connection_state_ = state;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  NetworkState cellular_network_;
  TestMobileActivator mobile_activator_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MobileActivatorTest);
};

TEST_F(MobileActivatorTest, OTAHasNetworkConnection) {
  // Make sure if we have a network connection, the mobile activator does not
  // connect to the network.
  EXPECT_CALL(mobile_activator_, GetDefaultNetwork())
      .WillRepeatedly(Return(&cellular_network_));
  EXPECT_CALL(mobile_activator_, ConnectNetwork(_))
      .Times(0);
  set_connection_state(shill::kStateNoConnectivity);
  set_network_activation_type(shill::kActivationTypeOTA);
  set_network_activation_state(shill::kActivationStateNotActivated);
  mobile_activator_.InvokeStartActivation();
  EXPECT_EQ(mobile_activator_.state(),
            MobileActivator::PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING);
}

TEST_F(MobileActivatorTest, OTANoNetworkConnection) {
  // Make sure if we don't have a network connection, the mobile activator
  // connects to the network.
  EXPECT_CALL(mobile_activator_, GetDefaultNetwork())
      .WillRepeatedly(Return(static_cast<NetworkState*>(NULL)));
  EXPECT_CALL(mobile_activator_, ConnectNetwork(&cellular_network_));
  set_connection_state(shill::kStateIdle);
  set_network_activation_type(shill::kActivationTypeOTA);
  set_network_activation_state(shill::kActivationStateNotActivated);
  mobile_activator_.InvokeStartActivation();
  EXPECT_EQ(mobile_activator_.state(),
            MobileActivator::PLAN_ACTIVATION_WAITING_FOR_CONNECTION);
}

TEST_F(MobileActivatorTest, OTAActivationFlow) {
  // Once a network connection is available, the OTA flow should look like the
  // following:
  //   - Loading payment portal
  //   - Showing payment portal
  //   - (User fills out payment portal and submits information)
  //   - Activation complete
  EXPECT_CALL(mobile_activator_, GetDefaultNetwork())
      .WillRepeatedly(Return(&cellular_network_));
  set_connection_state(shill::kStateOnline);
  set_network_activation_type(shill::kActivationTypeOTA);
  set_network_activation_state(shill::kActivationStateNotActivated);
  mobile_activator_.InvokeStartActivation();
  EXPECT_EQ(mobile_activator_.state(),
            MobileActivator::PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING);
  mobile_activator_.InvokeHandlePortalLoaded(true);
  EXPECT_EQ(mobile_activator_.state(),
            MobileActivator::PLAN_ACTIVATION_SHOWING_PAYMENT);
  mobile_activator_.InvokeHandleSetTransactionStatus(true);
  EXPECT_EQ(mobile_activator_.state(), MobileActivator::PLAN_ACTIVATION_DONE);
}

TEST_F(MobileActivatorTest, OTASPBasicFlowForNewDevices) {
  // In a new device, we aren't connected to Verizon, we start at START
  // because we haven't paid Verizon (ever), and the modem isn't even partially
  // activated.
  set_activator_state(MobileActivator::PLAN_ACTIVATION_START);
  set_connection_state(shill::kStateIdle);
  set_network_activation_type(shill::kActivationTypeOTASP);
  set_network_activation_state(shill::kActivationStateNotActivated);
  EXPECT_EQ(MobileActivator::PLAN_ACTIVATION_INITIATING_ACTIVATION,
            mobile_activator_.InvokePickNextState(&cellular_network_));
  // Now behave as if ChangeState() has initiated an activation.
  set_activator_state(MobileActivator::PLAN_ACTIVATION_INITIATING_ACTIVATION);
  set_network_activation_state(shill::kActivationStateActivating);
  // We'll sit in this state while we wait for the OTASP to finish.
  EXPECT_EQ(MobileActivator::PLAN_ACTIVATION_INITIATING_ACTIVATION,
            mobile_activator_.InvokePickNextState(&cellular_network_));
  set_network_activation_state(shill::kActivationStatePartiallyActivated);
  // We'll sit in this state until we go online as well.
  EXPECT_EQ(MobileActivator::PLAN_ACTIVATION_INITIATING_ACTIVATION,
            mobile_activator_.InvokePickNextState(&cellular_network_));
  set_connection_state(shill::kStateNoConnectivity);
  // After we go online, we go back to START, which acts as a jumping off
  // point for the two types of initial OTASP.
  EXPECT_EQ(MobileActivator::PLAN_ACTIVATION_START,
            mobile_activator_.InvokePickNextState(&cellular_network_));
  set_activator_state(MobileActivator::PLAN_ACTIVATION_START);
  EXPECT_EQ(MobileActivator::PLAN_ACTIVATION_TRYING_OTASP,
            mobile_activator_.InvokePickNextState(&cellular_network_));
  // Very similar things happen while we're trying OTASP.
  set_activator_state(MobileActivator::PLAN_ACTIVATION_TRYING_OTASP);
  set_network_activation_state(shill::kActivationStateActivating);
  EXPECT_EQ(MobileActivator::PLAN_ACTIVATION_TRYING_OTASP,
            mobile_activator_.InvokePickNextState(&cellular_network_));
  set_network_activation_state(shill::kActivationStatePartiallyActivated);
  set_connection_state(shill::kStateNoConnectivity);
  // And when we come back online again and aren't activating, load the portal.
  EXPECT_EQ(MobileActivator::PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING,
            mobile_activator_.InvokePickNextState(&cellular_network_));
  // The JS drives us through the payment portal.
  set_activator_state(MobileActivator::PLAN_ACTIVATION_SHOWING_PAYMENT);
  // The JS also calls us to signal that the portal is done.  This triggers us
  // to start our final OTASP via the aptly named StartOTASP().
  EXPECT_CALL(mobile_activator_, SignalCellularPlanPayment());
  EXPECT_CALL(mobile_activator_,
              ChangeState(Eq(&cellular_network_),
                          Eq(MobileActivator::PLAN_ACTIVATION_START_OTASP),
                          _));
  EXPECT_CALL(mobile_activator_,
              EvaluateCellularNetwork(Eq(&cellular_network_)));
  mobile_activator_.InvokeHandleSetTransactionStatus(true);
  // Evaluate state will defer to PickNextState to select what to do now that
  // we're in START_ACTIVATION.  PickNextState should decide to start a final
  // OTASP.
  set_activator_state(MobileActivator::PLAN_ACTIVATION_START_OTASP);
  EXPECT_EQ(MobileActivator::PLAN_ACTIVATION_OTASP,
            mobile_activator_.InvokePickNextState(&cellular_network_));
  // Similarly to TRYING_OTASP and INITIATING_OTASP above...
  set_activator_state(MobileActivator::PLAN_ACTIVATION_OTASP);
  set_network_activation_state(shill::kActivationStateActivating);
  EXPECT_EQ(MobileActivator::PLAN_ACTIVATION_OTASP,
            mobile_activator_.InvokePickNextState(&cellular_network_));
  set_network_activation_state(shill::kActivationStateActivated);
  EXPECT_EQ(MobileActivator::PLAN_ACTIVATION_DONE,
            mobile_activator_.InvokePickNextState(&cellular_network_));
}

// A fake for MobileActivator::RequestCellularActivation that always succeeds.
void FakeRequestCellularActivationSuccess(
    const NetworkState* network,
    const base::Closure& success_callback,
    const network_handler::ErrorCallback& error_callback) {
  success_callback.Run();
}

// A fake for MobileActivator::RequestCellularActivation that always fails.
void FakeRequestCellularActivationFailure(
    const NetworkState* network,
    const base::Closure& success_callback,
    const network_handler::ErrorCallback& error_callback) {
  std::unique_ptr<base::DictionaryValue> value;
  error_callback.Run("", std::move(value));
}

TEST_F(MobileActivatorTest, OTASPScheduling) {
  for (size_t i = 0; i < kNumOTASPStates; ++i) {
    // When activation works, we start a timer to watch for success.
    EXPECT_CALL(mobile_activator_, RequestCellularActivation(_, _, _))
        .Times(1)
        .WillOnce(Invoke(FakeRequestCellularActivationSuccess));
    EXPECT_CALL(mobile_activator_, StartOTASPTimer())
         .Times(1);
    set_activator_state(MobileActivator::PLAN_ACTIVATION_START);
    mobile_activator_.InvokeChangeState(&cellular_network_, kOTASPStates[i]);

    // When activation fails, it's an error, unless we're trying for the final
    // OTASP, in which case we try again via DELAY_OTASP.
    EXPECT_CALL(mobile_activator_, RequestCellularActivation(_, _, _))
        .Times(1)
        .WillOnce(Invoke(FakeRequestCellularActivationFailure));
    if (kOTASPStates[i] == MobileActivator::PLAN_ACTIVATION_OTASP) {
      EXPECT_CALL(mobile_activator_, ChangeState(
          Eq(&cellular_network_),
          Eq(MobileActivator::PLAN_ACTIVATION_DELAY_OTASP),
          _));
    } else {
      EXPECT_CALL(mobile_activator_, ChangeState(
          Eq(&cellular_network_),
          Eq(MobileActivator::PLAN_ACTIVATION_ERROR),
          _));
    }
    set_activator_state(MobileActivator::PLAN_ACTIVATION_START);
    mobile_activator_.InvokeChangeState(&cellular_network_, kOTASPStates[i]);
  }
}

TEST_F(MobileActivatorTest, OTASPStartAtStart) {
  set_network_activation_type(shill::kActivationTypeOTASP);
  EXPECT_CALL(mobile_activator_, HasRecentCellularPlanPayment())
      .WillOnce(Return(false));
  EXPECT_CALL(mobile_activator_,
              EvaluateCellularNetwork(Eq(&cellular_network_)));
  mobile_activator_.InvokeStartActivation();
  EXPECT_EQ(mobile_activator_.state(), MobileActivator::PLAN_ACTIVATION_START);
}

TEST_F(MobileActivatorTest, ReconnectOnDisconnectFromPaymentPortal) {
  // Most states either don't care if we're offline or expect to be offline at
  // some point.  For instance the OTASP states expect to go offline during
  // activation and eventually come back.  There are a few transitions states
  // like START_OTASP and DELAY_OTASP which don't really depend on the state of
  // the modem (offline or online) to work correctly.  A few places however,
  // like when we're displaying the portal care quite a bit about going
  // offline.  Lets test for those cases.
  set_connection_state(shill::kStateFailure);
  set_network_activation_state(shill::kActivationStatePartiallyActivated);
  set_activator_state(MobileActivator::PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING);
  EXPECT_EQ(MobileActivator::PLAN_ACTIVATION_RECONNECTING,
            mobile_activator_.InvokePickNextState(&cellular_network_));
  set_activator_state(MobileActivator::PLAN_ACTIVATION_SHOWING_PAYMENT);
  EXPECT_EQ(MobileActivator::PLAN_ACTIVATION_RECONNECTING,
            mobile_activator_.InvokePickNextState(&cellular_network_));
}

}  // namespace chromeos
