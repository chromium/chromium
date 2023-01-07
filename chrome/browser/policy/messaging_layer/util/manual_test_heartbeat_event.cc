// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/manual_test_heartbeat_event.h"

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "chrome/browser/policy/messaging_layer/util/report_queue_manual_test_context.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/task_runner_context.h"

namespace reporting {
namespace {

BASE_FEATURE(kEncryptedReportingManualTestHeartbeatEvent,
             "EncryptedReportingManualTestHeartbeatEvent",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

ManualTestHeartbeatEvent::ManualTestHeartbeatEvent() {
  StartHeartbeatEvent();
}

ManualTestHeartbeatEvent::~ManualTestHeartbeatEvent() {
  Shutdown();
}

void ManualTestHeartbeatEvent::Shutdown() {}

void ManualTestHeartbeatEvent::StartHeartbeatEvent() const {
  if (!base::FeatureList::IsEnabled(
          kEncryptedReportingManualTestHeartbeatEvent)) {
    return;
  }

  Start<ReportQueueManualTestContext>(
      /*period=*/base::Seconds(1),
      /*number_of_messages_to_enqueue=*/10,
      /*destination=*/HEARTBEAT_EVENTS,
      /*priority=*/FAST_BATCH, base::BindOnce([](Status status) {
        LOG(WARNING) << "Heartbeat Event completed with status: " << status;
      }),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock()}));
}

}  // namespace reporting
