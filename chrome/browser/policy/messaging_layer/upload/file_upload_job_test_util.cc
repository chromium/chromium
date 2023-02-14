// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/file_upload_job_test_util.h"

#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/policy/messaging_layer/upload/file_upload_job.h"
#include "components/reporting/util/test_support_callbacks.h"

namespace reporting {

FileUploadJob::TestEnvironment::TestEnvironment() {
  FileUploadJob::Manager::instance_ref().reset(new Manager());
}

FileUploadJob::TestEnvironment::~TestEnvironment() {
  {
    test::TestCallbackAutoWaiter waiter;
    Manager::GetInstance()->sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](test::TestCallbackAutoWaiter* waiter) {
              auto* const self = Manager::GetInstance();
              DCHECK_CALLED_ON_VALID_SEQUENCE(self->manager_sequence_checker_);
              self->uploads_in_progress_.clear();
              waiter->Signal();
            },
            base::Unretained(&waiter)));
  }
  FileUploadJob::Manager::instance_ref().reset();
}
}  // namespace reporting
