// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/odfs_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace ash::file_system_provider {

namespace {

const char* ToString(RequestType request_type) {
  switch (request_type) {
    case RequestType::kAbort:
      return "onAbortRequested";
    case RequestType::kAddWatcher:
      return "onAddWatcherRequested";
    case RequestType::kCloseFile:
      return "onCloseFileRequested";
    case RequestType::kConfigure:
      return "onConfigureRequested";
    case RequestType::kCopyEntry:
      return "onCopyEntryRequested";
    case RequestType::kCreateDirectory:
      return "onCreateDirectoryRequested";
    case RequestType::kCreateFile:
      return "onCreateFileRequested";
    case RequestType::kDeleteEntry:
      return "onDeleteEntryRequested";
    case RequestType::kExecuteAction:
      return "onExecuteActionRequested";
    case RequestType::kGetActions:
      return "onGetActionsRequested";
    case RequestType::kGetMetadata:
      return "onGetMetadataRequested";
    case RequestType::kMount:
      return "onMountRequested";
    case RequestType::kMoveEntry:
      return "onMoveEntryRequested";
    case RequestType::kOpenFile:
      return "onOpenFileRequested";
    case RequestType::kReadDirectory:
      return "onReadDirectoryRequested";
    case RequestType::kReadFile:
      return "onReadFileRequested";
    case RequestType::kRemoveWatcher:
      return "onRemoveWatcherRequested";
    case RequestType::kTruncate:
      return "onTruncateRequested";
    case RequestType::kUnmount:
      return "onUnmountRequested";
    case RequestType::kWriteFile:
      return "onWriteFileRequested";
  }
}

std::string GetHistogramName(const char* metric, RequestType request_type) {
  return base::StrCat({"FileBrowser.OfficeFiles.ODFS.FileSystemProvider.",
                       metric, ".", ToString(request_type)});
}

}  // namespace

struct ODFSMetrics::Request {
  RequestType request_type;
  base::File::Error result = base::File::FILE_OK;
  base::ElapsedTimer latency_timer;
};

ODFSMetrics::ODFSMetrics() = default;

ODFSMetrics::~ODFSMetrics() = default;

void ODFSMetrics::OnRequestCreated(int request_id, RequestType type) {
  Request& request = requests_[request_id];
  request.request_type = type;
}

void ODFSMetrics::OnRequestDestroyed(int request_id,
                                     OperationCompletion completion) {
  auto it = requests_.find(request_id);
  if (it == requests_.end()) {
    return;
  }
  Request& request = it->second;
  base::UmaHistogramMediumTimes(GetHistogramName("Time", request.request_type),
                                request.latency_timer.Elapsed());
  base::UmaHistogramEnumeration(
      GetHistogramName("Completion", request.request_type), completion);
  requests_.erase(it);
}

void ODFSMetrics::OnRequestExecuted(int request_id) {}

void ODFSMetrics::OnRequestFulfilled(int request_id,
                                     const RequestValue& result,
                                     bool has_more) {
  if (!has_more) {
    RecordResult(request_id, base::File::FILE_OK);
  }
}

void ODFSMetrics::OnRequestRejected(int request_id,
                                    const RequestValue& result,
                                    base::File::Error error) {
  RecordResult(request_id, error);
}

void ODFSMetrics::OnRequestTimedOut(int request_id) {}

void ODFSMetrics::RecordResult(int request_id, base::File::Error error) {
  auto it = requests_.find(request_id);
  if (it == requests_.end()) {
    return;
  }
  Request& request = it->second;
  base::UmaHistogramExactLinear(GetHistogramName("Error", request.request_type),
                                -error, -base::File::FILE_ERROR_MAX);
}

}  // namespace ash::file_system_provider
