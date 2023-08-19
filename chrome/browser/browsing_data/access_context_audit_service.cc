// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/access_context_audit_service.h"
#include "base/files/file_util.h"
#include "base/task/thread_pool.h"
#include "base/task/updateable_sequenced_task_runner.h"

namespace {
const base::FilePath::CharType kDatabaseName[] =
    FILE_PATH_LITERAL("AccessContextAudit");
}

AccessContextAuditService::AccessContextAuditService() = default;
AccessContextAuditService::~AccessContextAuditService() = default;

void AccessContextAuditService::Init(const base::FilePath& database_dir) {
  database_task_runner_ = base::ThreadPool::CreateUpdateableSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::ThreadPolicy::PREFER_BACKGROUND,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});

  database_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::FilePath database_dir) {
                       base::DeleteFile(database_dir.Append(kDatabaseName));
                     },
                     database_dir));
}
