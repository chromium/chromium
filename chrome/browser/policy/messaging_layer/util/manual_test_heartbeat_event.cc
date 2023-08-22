// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/manual_test_heartbeat_event.h"

#include <memory>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "chrome/browser/policy/messaging_layer/util/report_queue_manual_test_context.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/task_runner_context.h"

namespace reporting {
namespace {

// Device heartbeat event.
BASE_FEATURE(kEncryptedReportingManualTestHeartbeatEvent,
             "EncryptedReportingManualTestHeartbeatEvent",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

ManualTestHeartbeatEvent::ManualTestHeartbeatEvent() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  managed_session_service_ = std::make_unique<policy::ManagedSessionService>();
  CHECK(managed_session_service_);
  managed_session_observation_.Observe(managed_session_service_.get());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  StartHeartbeatEvent();
}

ManualTestHeartbeatEvent::~ManualTestHeartbeatEvent() {
  Shutdown();
}

void ManualTestHeartbeatEvent::Shutdown() {}

void ManualTestHeartbeatEvent::StartHeartbeatEvent() const {
  // Device heartbeat
  if (base::FeatureList::IsEnabled(
          kEncryptedReportingManualTestHeartbeatEvent)) {
    Start<ReportQueueManualTestContext>(
        /*period=*/base::Seconds(1),
        /*number_of_messages_to_enqueue=*/10,
        /*destination=*/HEARTBEAT_EVENTS,
        /*priority=*/FAST_BATCH, EventType::kDevice,
        base::BindOnce([](Status status) {
          LOG(WARNING) << "Heartbeat Event completed with status: " << status;
        }),
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::TaskPriority::BEST_EFFORT, base::MayBlock()}));
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

namespace {
// User heartbeat event.
BASE_FEATURE(kEncryptedReportingManualTestUserHeartbeatEvent,
             "EncryptedReportingManualTestUserHeartbeatEvent",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace

// Enqueues a 10 heartbeat events with `EventType::kUser` upon login.
void ManualTestHeartbeatEvent::OnLogin(Profile* profile) {
  managed_session_observation_.Reset();
  CHECK_NE(profile, nullptr);

  if (base::FeatureList::IsEnabled(
          kEncryptedReportingManualTestUserHeartbeatEvent)) {
    Start<ReportQueueManualTestContext>(
        /*period=*/base::Seconds(1),
        /*number_of_messages_to_enqueue=*/10,
        /*destination=*/HEARTBEAT_EVENTS,
        /*priority=*/FAST_BATCH, EventType::kUser,
        base::BindOnce([](Status status) {
          LOG(WARNING) << "Heartbeat Event completed with status: " << status;
        }),
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::TaskPriority::BEST_EFFORT, base::MayBlock()}));
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace reporting
