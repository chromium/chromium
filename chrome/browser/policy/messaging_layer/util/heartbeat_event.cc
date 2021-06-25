// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/heartbeat_event.h"

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "chrome/browser/policy/messaging_layer/util/report_queue_manual_test_context.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/reporting/proto/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/task_runner_context.h"

namespace reporting {
namespace {

const base::Feature kEncryptedReportingHeartbeatEvent{
    "EncryptedReportingHeartbeatEvent", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace

HeartbeatEvent::CloudPolicyServiceObserver::CloudPolicyServiceObserver(
    base::RepeatingCallback<void()> start_heartbeat_event)
    : start_heartbeat_event_(std::move(start_heartbeat_event)) {}

HeartbeatEvent::CloudPolicyServiceObserver::~CloudPolicyServiceObserver() =
    default;

void HeartbeatEvent::CloudPolicyServiceObserver::
    OnCloudPolicyServiceInitializationCompleted() {
  std::move(start_heartbeat_event_).Run();
}

HeartbeatEvent::HeartbeatEvent(policy::CloudPolicyManager* manager)
    : manager_(manager) {
  if (!manager_->core()->service()->IsInitializationComplete()) {
    // base::Unretained is safe here because cloud_policy_service_observer_ will
    // be destroyed before HeartbeatEvent.
    cloud_policy_service_observer_ =
        std::make_unique<CloudPolicyServiceObserver>(base::BindRepeating(
            &HeartbeatEvent::HandleNotification, base::Unretained(this)));
    manager_->core()->service()->AddObserver(
        cloud_policy_service_observer_.get());
    return;
  }
  StartHeartbeatEvent();
}

HeartbeatEvent::~HeartbeatEvent() {
  Shutdown();
}

void HeartbeatEvent::Shutdown() {
  if (cloud_policy_service_observer_) {
    manager_->core()->service()->RemoveObserver(
        cloud_policy_service_observer_.get());
    cloud_policy_service_observer_.reset();
  }
}

void HeartbeatEvent::HandleNotification() {
  bool expected = false;
  if (!notified_.compare_exchange_strong(expected, /*desired=*/true)) {
    return;
  }
  Shutdown();
  StartHeartbeatEvent();
}

void HeartbeatEvent::StartHeartbeatEvent() const {
  if (!base::FeatureList::IsEnabled(kEncryptedReportingHeartbeatEvent)) {
    return;
  }

  Start<ReportQueueManualTestContext>(
      /*frequency=*/base::TimeDelta::FromSeconds(1),
      /*number_of_messages_to_enqueue=*/10,
      /*destination=*/HEARTBEAT_EVENTS,
      /*priority=*/FAST_BATCH, base::BindOnce([](Status status) {
        LOG(WARNING) << "Heartbeat Event completed with status: " << status;
      }),
      base::ThreadPool::CreateSequencedTaskRunner(base::TaskTraits()));
}

}  // namespace reporting
