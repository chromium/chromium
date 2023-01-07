// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dbus_memory_pressure_evaluator_linux.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace {

class MockMemoryPressureVoter : public memory_pressure::MemoryPressureVoter {
 public:
  MOCK_METHOD2(SetVote,
               void(base::MemoryPressureListener::MemoryPressureLevel, bool));
};

}  // namespace

class DbusMemoryPressureEvaluatorLinuxTest : public testing::Test {
 protected:
  static const char* const kLmmService;
  static const char* const kLmmInterface;
  static const char* const kXdgPortalService;
  static const char* const kXdgPortalMemoryMonitorInterface;

  const base::TimeDelta kResetVotePeriod =
      DbusMemoryPressureEvaluatorLinux::kResetVotePeriod;

  void SetUp() override {
    mock_bus_ = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());

    ON_CALL(*mock_bus_, GetDBusTaskRunner)
        .WillByDefault(
            Return(task_environment_.GetMainThreadTaskRunner().get()));

    SetupProxy(dbus_proxy_, DBUS_SERVICE_DBUS, DBUS_PATH_DBUS);
    SetupProxy(lmm_proxy_, kLmmService,
               DbusMemoryPressureEvaluatorLinux::kLmmObject);
    SetupProxy(portal_proxy_, kXdgPortalService,
               DbusMemoryPressureEvaluatorLinux::kXdgPortalObject);

    ON_CALL(*dbus_proxy_, DoCallMethod)
        .WillByDefault(Invoke(
            this, &DbusMemoryPressureEvaluatorLinuxTest::HandleDBusCalls));

    auto voter = std::make_unique<MockMemoryPressureVoter>();
    mock_voter_ = voter.get();
    evaluator_ = base::WrapUnique(new DbusMemoryPressureEvaluatorLinux(
        std::move(voter), mock_bus_, mock_bus_));
  }

  void EmitLowMemoryWarning(uint8_t level) {
    dbus::Signal signal(
        kLmmInterface,
        DbusMemoryPressureEvaluatorLinux::kLowMemoryWarningSignal);
    dbus::MessageWriter writer(&signal);
    writer.AppendByte(level);

    evaluator_->OnLowMemoryWarning(&signal);
  }

  // Any service name added via this call will be reported as running by any
  // HasNameOwner calls the evaluator makes.
  void AddRunningService(std::string service) {
    running_services_.push_back(std::move(service));
  }

  void RunServiceChecks() { evaluator()->CheckIfLmmIsAvailable(); }

  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

  dbus::MockObjectProxy* lmm_proxy() { return lmm_proxy_.get(); }
  dbus::MockObjectProxy* portal_proxy() { return portal_proxy_.get(); }

  MockMemoryPressureVoter* mock_voter() { return mock_voter_; }
  DbusMemoryPressureEvaluatorLinux* evaluator() { return evaluator_.get(); }

 private:
  void SetupProxy(scoped_refptr<dbus::MockObjectProxy>& proxy,
                  const char* service,
                  const char* object_path_value) {
    dbus::ObjectPath object_path(object_path_value);
    proxy = base::MakeRefCounted<dbus::MockObjectProxy>(mock_bus_.get(),
                                                        service, object_path);
    ON_CALL(*mock_bus_, GetObjectProxy(service, object_path))
        .WillByDefault(Return(proxy.get()));
  }

  void HandleDBusCalls(dbus::MethodCall* method_call,
                       int timeout_ms,
                       dbus::ObjectProxy::ResponseCallback* response_callback) {
    method_call->SetSerial(123);
    method_call->SetReplySerial(456);

    if (method_call->GetMember() ==
        DbusMemoryPressureEvaluatorLinux::kMethodNameHasOwner) {
      dbus::MessageReader reader(method_call);
      std::string service;
      CHECK(reader.PopString(&service));

      std::unique_ptr<dbus::Response> response =
          dbus::Response::FromMethodCall(method_call);
      dbus::MessageWriter writer(response.get());
      writer.AppendBool(base::Contains(running_services_, service));

      std::move(*response_callback).Run(response.get());
    } else if (method_call->GetMember() ==
               DbusMemoryPressureEvaluatorLinux::kMethodListActivatableNames) {
      std::unique_ptr<dbus::Response> response =
          dbus::Response::FromMethodCall(method_call);
      dbus::MessageWriter writer(response.get());
      writer.AppendArrayOfStrings({});

      std::move(*response_callback).Run(response.get());
    } else {
      CHECK(false) << method_call->GetMember();
    }
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> dbus_proxy_;
  scoped_refptr<dbus::MockObjectProxy> lmm_proxy_;
  scoped_refptr<dbus::MockObjectProxy> portal_proxy_;

  std::unique_ptr<DbusMemoryPressureEvaluatorLinux> evaluator_;
  raw_ptr<MockMemoryPressureVoter> mock_voter_ = nullptr;

  std::vector<std::string> running_services_;
};

const char* const DbusMemoryPressureEvaluatorLinuxTest::kLmmService =
    DbusMemoryPressureEvaluatorLinux::kLmmService;
const char* const DbusMemoryPressureEvaluatorLinuxTest::kLmmInterface =
    DbusMemoryPressureEvaluatorLinux::kLmmInterface;
const char* const DbusMemoryPressureEvaluatorLinuxTest::kXdgPortalService =
    DbusMemoryPressureEvaluatorLinux::kXdgPortalService;
const char* const
    DbusMemoryPressureEvaluatorLinuxTest::kXdgPortalMemoryMonitorInterface =
        DbusMemoryPressureEvaluatorLinux::kXdgPortalMemoryMonitorInterface;

TEST_F(DbusMemoryPressureEvaluatorLinuxTest, Basic) {
  EXPECT_CALL(
      *mock_voter(),
      SetVote(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE, false))
      .WillOnce(Return());

  EmitLowMemoryWarning(0);
  EXPECT_EQ(evaluator()->current_vote(),
            base::MemoryPressureMonitor::MemoryPressureLevel::
                MEMORY_PRESSURE_LEVEL_NONE);

  EXPECT_CALL(
      *mock_voter(),
      SetVote(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
              true))
      .Times(2)
      .WillRepeatedly(Return());

  EmitLowMemoryWarning(50);
  EXPECT_EQ(evaluator()->current_vote(),
            base::MemoryPressureMonitor::MemoryPressureLevel::
                MEMORY_PRESSURE_LEVEL_MODERATE);

  EmitLowMemoryWarning(100);
  EXPECT_EQ(evaluator()->current_vote(),
            base::MemoryPressureMonitor::MemoryPressureLevel::
                MEMORY_PRESSURE_LEVEL_MODERATE);

  EXPECT_CALL(
      *mock_voter(),
      SetVote(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
              true))
      .WillOnce(Return());

  EmitLowMemoryWarning(255);
  EXPECT_EQ(evaluator()->current_vote(),
            base::MemoryPressureMonitor::MemoryPressureLevel::
                MEMORY_PRESSURE_LEVEL_CRITICAL);
}

TEST_F(DbusMemoryPressureEvaluatorLinuxTest, PeriodicReset) {
  // Verify that the level will be reset after the time delta.
  EmitLowMemoryWarning(100);
  EXPECT_EQ(evaluator()->current_vote(),
            base::MemoryPressureMonitor::MemoryPressureLevel::
                MEMORY_PRESSURE_LEVEL_MODERATE);

  EXPECT_CALL(
      *mock_voter(),
      SetVote(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE, false))
      .WillOnce(Return());

  task_environment().FastForwardBy(kResetVotePeriod);
  EXPECT_EQ(evaluator()->current_vote(),
            base::MemoryPressureMonitor::MemoryPressureLevel::
                MEMORY_PRESSURE_LEVEL_NONE);

  // Verify that no reset is sent for already-none levels.
  EXPECT_CALL(
      *mock_voter(),
      SetVote(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE, false))
      .WillOnce(Return());

  EmitLowMemoryWarning(0);
  task_environment().FastForwardBy(kResetVotePeriod);
}

TEST_F(DbusMemoryPressureEvaluatorLinuxTest, PrefersConnectingToLmm) {
  EXPECT_CALL(*lmm_proxy(), DoConnectToSignal(kLmmInterface, _, _, _)).Times(1);
  EXPECT_CALL(*portal_proxy(),
              DoConnectToSignal(kXdgPortalMemoryMonitorInterface, _, _, _))
      .Times(0);

  AddRunningService(kLmmService);
  AddRunningService(kXdgPortalService);

  RunServiceChecks();
}

TEST_F(DbusMemoryPressureEvaluatorLinuxTest, FallsBackToPortal) {
  EXPECT_CALL(*lmm_proxy(), DoConnectToSignal(kLmmInterface, _, _, _)).Times(0);
  EXPECT_CALL(*portal_proxy(),
              DoConnectToSignal(kXdgPortalMemoryMonitorInterface, _, _, _))
      .Times(1);

  AddRunningService(kXdgPortalService);

  RunServiceChecks();
}
