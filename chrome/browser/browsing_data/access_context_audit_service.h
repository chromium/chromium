// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_ACCESS_CONTEXT_AUDIT_SERVICE_H_
#define CHROME_BROWSER_BROWSING_DATA_ACCESS_CONTEXT_AUDIT_SERVICE_H_

#include "base/files/file_path.h"
#include "base/task/updateable_sequenced_task_runner.h"
#include "components/keyed_service/core/keyed_service.h"

class AccessContextAuditService : public KeyedService {
 public:
  AccessContextAuditService();

  AccessContextAuditService(const AccessContextAuditService&) = delete;
  AccessContextAuditService& operator=(const AccessContextAuditService&) =
      delete;

  ~AccessContextAuditService() override;

  void Init(const base::FilePath& database_dir);

  scoped_refptr<base::UpdateableSequencedTaskRunner> database_task_runner_;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_ACCESS_CONTEXT_AUDIT_SERVICE_H_
