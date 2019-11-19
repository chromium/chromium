// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_CRASH_IDS_SOURCE_H_
#define CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_CRASH_IDS_SOURCE_H_

#include <vector>

#include "base/callback_forward.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "components/upload_list/upload_list.h"

namespace system_logs {

// Extract the most recent crash IDs (if any) and adds them to the system logs.
class CrashIdsSource : public SystemLogsSource {
 public:
  CrashIdsSource();
  ~CrashIdsSource() override;

  // SystemLogsSource:
  void Fetch(SysLogsSourceCallback callback) override;

 private:
  void OnUploadListAvailable();
  void RespondWithCrashIds(SysLogsSourceCallback callback);

  scoped_refptr<UploadList> crash_upload_list_;

  // A comma-separated list of crash IDs as expected by the server. The first
  // is for the last hour, the second is for the pat 120 days.
  std::string crash_ids_list_;
  std::string all_crash_ids_list_;

  // Contains any pending fetch requests waiting for the crash upload list to
  // finish loading.
  std::vector<base::OnceClosure> pending_requests_;

  // True if the crash list is currently being loaded.
  bool pending_crash_list_loading_;

  DISALLOW_COPY_AND_ASSIGN(CrashIdsSource);
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_CRASH_IDS_SOURCE_H_
