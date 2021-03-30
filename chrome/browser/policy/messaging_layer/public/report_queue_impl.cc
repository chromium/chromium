// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/public/report_queue_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/encryption/encryption_module.h"
#include "components/reporting/proto/record.pb.h"
#include "components/reporting/proto/record_constants.pb.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

std::unique_ptr<ReportQueueImpl> ReportQueueImpl::Create(
    std::unique_ptr<ReportQueueConfiguration> config,
    scoped_refptr<StorageModuleInterface> storage) {
  return base::WrapUnique<ReportQueueImpl>(
      new ReportQueueImpl(std::move(config), storage));
}

ReportQueueImpl::~ReportQueueImpl() = default;

ReportQueueImpl::ReportQueueImpl(
    std::unique_ptr<ReportQueueConfiguration> config,
    scoped_refptr<StorageModuleInterface> storage)
    : config_(std::move(config)),
      storage_(storage),
      sequenced_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(base::TaskTraits())) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void ReportQueueImpl::AddRecord(base::StringPiece record,
                                Priority priority,
                                EnqueueCallback callback) const {
  const Status status = config_->CheckPolicy();
  if (!status.ok()) {
    std::move(callback).Run(status);
    return;
  }

  if (priority == Priority::UNDEFINED_PRIORITY) {
    std::move(callback).Run(
        Status(error::INVALID_ARGUMENT, "Priority must be defined"));
    return;
  }

  sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ReportQueueImpl::SendRecordToStorage,
                                base::Unretained(this), std::string(record),
                                priority, std::move(callback)));
}

void ReportQueueImpl::SendRecordToStorage(base::StringPiece record_data,
                                          Priority priority,
                                          EnqueueCallback callback) const {
  storage_->AddRecord(priority, AugmentRecord(record_data),
                      std::move(callback));
}

Record ReportQueueImpl::AugmentRecord(base::StringPiece record_data) const {
  Record record;
  record.set_data(std::string(record_data));
  record.set_destination(config_->destination());
  record.set_dm_token(config_->dm_token());
  // Calculate timestamp in microseconds - to match Spanner expectations.
  const int64_t time_since_epoch_us =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds();
  record.set_timestamp_us(time_since_epoch_us);
  return record;
}

void ReportQueueImpl::Flush(Priority priority, FlushCallback callback) {
  storage_->Flush(priority, std::move(callback));
}

}  // namespace reporting
