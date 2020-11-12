// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/public/report_queue.h"

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
#include "chrome/browser/policy/messaging_layer/encryption/encryption_module.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue_configuration.h"
#include "chrome/browser/policy/messaging_layer/storage/storage_module.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/status_macros.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/proto/record.pb.h"
#include "components/policy/proto/record_constants.pb.h"

namespace reporting {

std::unique_ptr<ReportQueue> ReportQueue::Create(
    std::unique_ptr<ReportQueueConfiguration> config,
    scoped_refptr<StorageModule> storage) {
  return base::WrapUnique<ReportQueue>(
      new ReportQueue(std::move(config), storage));
}

ReportQueue::~ReportQueue() = default;

ReportQueue::ReportQueue(std::unique_ptr<ReportQueueConfiguration> config,
                         scoped_refptr<StorageModule> storage)
    : config_(std::move(config)),
      storage_(storage),
      sequenced_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(base::TaskTraits())) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void ReportQueue::Enqueue(base::StringPiece record,
                          Priority priority,
                          EnqueueCallback callback) const {
  AddRecord(record, priority, std::move(callback));
}

void ReportQueue::Enqueue(const base::Value& record,
                          Priority priority,
                          EnqueueCallback callback) const {
  ASSIGN_OR_ONCE_CALLBACK_AND_RETURN(std::string json_record, callback,
                                     ValueToJson(record));
  AddRecord(json_record, priority, std::move(callback));
}

void ReportQueue::Enqueue(google::protobuf::MessageLite* record,
                          Priority priority,
                          EnqueueCallback callback) const {
  ASSIGN_OR_ONCE_CALLBACK_AND_RETURN(std::string protobuf_record, callback,
                                     ProtoToString(record));
  AddRecord(protobuf_record, priority, std::move(callback));
}

void ReportQueue::AddRecord(base::StringPiece record,
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
      FROM_HERE,
      base::BindOnce(&ReportQueue::SendRecordToStorage, base::Unretained(this),
                     std::string(record), priority, std::move(callback)));
}

void ReportQueue::SendRecordToStorage(base::StringPiece record_data,
                                      Priority priority,
                                      EnqueueCallback callback) const {
  storage_->AddRecord(priority, AugmentRecord(record_data),
                      std::move(callback));
}

StatusOr<std::string> ReportQueue::ValueToJson(
    const base::Value& record) const {
  std::string json_record;
  if (!base::JSONWriter::Write(record, &json_record)) {
    return Status(error::INVALID_ARGUMENT,
                  "Provided record was not convertable to a std::string");
  }
  return json_record;
}

StatusOr<std::string> ReportQueue::ProtoToString(
    google::protobuf::MessageLite* record) const {
  std::string protobuf_record;
  if (!record->SerializeToString(&protobuf_record)) {
    return Status(error::INVALID_ARGUMENT,
                  "Unabled to serialize record to string. Most likely due to "
                  "unset required fields.");
  }
  return protobuf_record;
}

Record ReportQueue::AugmentRecord(base::StringPiece record_data) const {
  Record record;
  record.set_data(std::string(record_data));
  record.set_destination(config_->destination());
  record.set_dm_token(config_->dm_token().value());
  // Calculate timestamp in microseconds - to match Spanner expectations.
  const int64_t time_since_epoch_us =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds();
  record.set_timestamp_us(time_since_epoch_us);
  return record;
}

}  // namespace reporting
