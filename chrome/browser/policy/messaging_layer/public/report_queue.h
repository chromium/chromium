// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_QUEUE_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_QUEUE_H_

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue_configuration.h"
#include "chrome/browser/policy/messaging_layer/storage/storage_module.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "components/policy/proto/record.pb.h"
#include "components/policy/proto/record_constants.pb.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace reporting {

// A |ReportQueue| is configured with a |ReportQueueConfiguration|.  A
// |ReportQueue| allows a user to |Enqueue| a message for delivery to a handler
// specified by the |Destination| held by the provided
// |ReportQueueConfiguration|. |ReportQueue| handles scheduling storage and
// delivery.
//
// ReportQueues are not meant to be created directly, instead use the
// reporting::ReportingClient::CreateReportQueue(...) function. See the comments
// for reporting::ReportingClient for example usage.
//
// Enqueue can also be used with a |base::Value| or |std::string|.
class ReportQueue {
 public:
  // An EnqueueCallbacks are called on the completion of any |Enqueue| call.
  using EnqueueCallback = base::OnceCallback<void(Status)>;

  // Factory
  static std::unique_ptr<ReportQueue> Create(
      std::unique_ptr<ReportQueueConfiguration> config,
      scoped_refptr<StorageModule> storage);

  virtual ~ReportQueue();
  ReportQueue(const ReportQueue& other) = delete;
  ReportQueue& operator=(const ReportQueue& other) = delete;

  // Enqueue asynchronously stores and delivers a record. Enqueue will return an
  // OK status if the task is successfully scheduled. The |callback| will be
  // called on any errors. If storage is successful |callback| will be called
  // with an OK status.
  //
  // The current destinations have the following data requirements:
  // (destination : requirement)
  // UPLOAD_EVENTS : UploadEventsRequest
  //
  // |record| will be sent as a string with no conversion.
  virtual Status Enqueue(base::StringPiece record,
                         EnqueueCallback callback) const;

  // |record| will be converted to a JSON string with base::JsonWriter::Write.
  virtual Status Enqueue(const base::Value& record,
                         EnqueueCallback callback) const;

  // |record| will be converted to a string with SerializeToString(). The
  // handler is responsible for converting the record back to a proto with a
  // ParseFromString() call.
  virtual Status Enqueue(google::protobuf::MessageLite* record,
                         EnqueueCallback callback) const;

 protected:
  ReportQueue(std::unique_ptr<ReportQueueConfiguration> config,
              scoped_refptr<StorageModule> storage);

 private:
  Status AddRecord(base::StringPiece record, EnqueueCallback callback) const;
  void SendRecordToStorage(base::StringPiece record,
                           EnqueueCallback callback) const;

  reporting::Record AugmentRecord(base::StringPiece record_data) const;

  std::unique_ptr<ReportQueueConfiguration> config_;
  scoped_refptr<StorageModule> storage_;
  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_QUEUE_H_
