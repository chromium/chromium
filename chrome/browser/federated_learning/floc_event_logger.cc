// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/federated_learning/floc_event_logger.h"

#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/federated_learning/floc_remote_permission_service.h"
#include "components/federated_learning/features/features.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync_user_events/user_event_service.h"
#include "content/public/browser/browser_thread.h"

namespace federated_learning {

namespace {

const base::TimeDelta kSecondAttemptDelay = base::TimeDelta::FromSeconds(10);

}  // namespace

FlocEventLogger::FlocEventLogger(
    syncer::SyncService* sync_service,
    FlocRemotePermissionService* floc_remote_permission_service,
    syncer::UserEventService* user_event_service)
    : sync_service_(sync_service),
      floc_remote_permission_service_(floc_remote_permission_service),
      user_event_service_(user_event_service) {}

FlocEventLogger::~FlocEventLogger() = default;

void FlocEventLogger::LogFlocComputedEvent(Event event) {
  if (!base::FeatureList::IsEnabled(kFlocIdComputedEventLogging))
    return;

  auto can_log_event_decided_callback =
      base::BindOnce(&FlocEventLogger::OnCanLogEventDecided,
                     weak_ptr_factory_.GetWeakPtr(), std::move(event));

  if (!IsSyncHistoryEnabled()) {
    // Give it a second chance 10 seconds later. This is because the first floc
    // event logging for a browser session can happen before sync finishes
    // setting up, and we want to ensure that the event will eventually be
    // logged if sync is supposed to be enabled.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FlocEventLogger::CheckCanLogEvent,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(can_log_event_decided_callback)),
        kSecondAttemptDelay);
    return;
  }

  CheckCanLogEvent(std::move(can_log_event_decided_callback));
}

void FlocEventLogger::CheckCanLogEvent(CanLogEventCallback callback) {
  if (!IsSyncHistoryEnabled()) {
    std::move(callback).Run(false);
    return;
  }

  IsSwaaNacAccountEnabled(std::move(callback));
}

void FlocEventLogger::OnCanLogEventDecided(Event event, bool can_log_event) {
  if (!can_log_event)
    return;

  auto specifics = std::make_unique<sync_pb::UserEventSpecifics>();
  specifics->set_event_time_usec(
      event.time.ToDeltaSinceWindowsEpoch().InMicroseconds());

  sync_pb::UserEventSpecifics_FlocIdComputed* const floc_id_computed_event =
      specifics->mutable_floc_id_computed_event();

  if (event.sim_hash_computed)
    floc_id_computed_event->set_floc_id(event.sim_hash);

  user_event_service_->RecordUserEvent(std::move(specifics));
}

bool FlocEventLogger::IsSyncHistoryEnabled() const {
  return sync_service_->IsSyncFeatureActive() &&
         sync_service_->GetActiveDataTypes().Has(
             syncer::HISTORY_DELETE_DIRECTIVES);
}

void FlocEventLogger::IsSwaaNacAccountEnabled(CanLogEventCallback callback) {
  net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation =
      net::DefinePartialNetworkTrafficAnnotation(
          "floc_event_logger", "floc_remote_permission_service",
          R"(
        semantics {
          description:
            "Queries Google to find out if user has enabled 'web and app "
            "activity' and 'ad personalization', and if the account type is "
            "NOT a child account. Those permission bits will be checked before "
            "logging the FLoC (Federated Learning of Cohorts) ID - an "
            "anonymous similarity hash value of user’s navigation history. "
            "This ensures that the logged ID is derived from data that Google "
            "already owns and the user has explicitly granted permission on "
            "what they will be used for."
          trigger:
            "This request is sent after each time the FLoC (Federated Learning "
            "of Cohorts) ID is computed. A FLoC ID is an anonymous similarity "
            "hash value of user’s navigation history. It'll be computed at the "
            "start of each browser profile session and will be refreshed "
            "regularly."
          data:
            "Google credentials if user is signed in."
        }
        policy {
            setting:
              "This feature can be disabled by disabling sync or third-party "
              "cookies."
        })");

  floc_remote_permission_service_->QueryFlocPermission(
      std::move(callback), partial_traffic_annotation);
}

}  // namespace federated_learning
