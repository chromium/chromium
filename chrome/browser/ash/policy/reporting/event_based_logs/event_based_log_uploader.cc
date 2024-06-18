// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/event_based_logs/event_based_log_uploader.h"

#include <algorithm>
#include <optional>
#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/uuid.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/log_upload_event.pb.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "components/reporting/util/status.h"

namespace policy {

EventBasedLogUploader::EventBasedLogUploader() = default;
EventBasedLogUploader::~EventBasedLogUploader() = default;

// static
std::string EventBasedLogUploader::GenerateUploadId() {
  return base::Uuid::GenerateRandomV4().AsLowercaseString();
}

EventBasedLogUploaderImpl::EventBasedLogUploaderImpl() = default;
EventBasedLogUploaderImpl::~EventBasedLogUploaderImpl() = default;

void EventBasedLogUploaderImpl::UploadEventBasedLogs(
    std::set<support_tool::DataCollectorType> data_collectors,
    ash::reporting::TriggerEventType event_type,
    std::optional<std::string> upload_id,
    UploadCallback on_upload_completed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO: b/330675989 - Add real log upload logic when the blocker is resolved
  // and File Storage Server accepts event based log uploads.
  // For now, we don't do anything here.
  std::move(on_upload_completed).Run(reporting::Status::StatusOK());
}

}  // namespace policy
