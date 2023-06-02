// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_ODFS_METRICS_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_ODFS_METRICS_H_

#include <map>

#include "base/files/file.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ash/file_system_provider/request_manager.h"

namespace ash::file_system_provider {

class ODFSMetrics : public RequestManager::Observer {
 public:
  ODFSMetrics();
  ~ODFSMetrics() override;
  // RequestManager::Observer overrides:
  void OnRequestCreated(int request_id, RequestType type) override;
  void OnRequestDestroyed(int request_id,
                          OperationCompletion completion) override;
  void OnRequestExecuted(int request_id) override;
  void OnRequestFulfilled(int request_id,
                          const RequestValue& result,
                          bool has_more) override;
  void OnRequestRejected(int request_id,
                         const RequestValue& result,
                         base::File::Error error) override;
  void OnRequestTimedOut(int request_id) override;

 private:
  struct Request;
  void RecordResult(int request_id, base::File::Error error);

  std::map<int, Request> requests_;
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_ODFS_METRICS_H_
