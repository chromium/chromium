// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_QUEUE_IMPL_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_QUEUE_IMPL_H_

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/values.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/proto/record.pb.h"
#include "components/reporting/proto/record_constants.pb.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace reporting {

// A |ReportQueueImpl| is configured with a |ReportQueueConfiguration|.  A
// |ReportQueueImpl| allows a user to |Enqueue| a message for delivery to a
// handler specified by the |Destination| held by the provided
// |ReportQueueConfiguration|. |ReportQueueImpl| handles scheduling storage and
// delivery.
//
// ReportQueues are not meant to be created directly, instead use the
// reporting::ReportQueueProvider::CreateQueue(...) function. See the
// comments for reporting::ReportingClient for example usage.
//
// Enqueue can also be used with a |base::Value| or |std::string|.
class ReportQueueImpl : public ReportQueue {
 public:
  // Factory
  static std::unique_ptr<ReportQueueImpl> Create(
      std::unique_ptr<ReportQueueConfiguration> config,
      scoped_refptr<StorageModuleInterface> storage);

  ~ReportQueueImpl() override;
  ReportQueueImpl(const ReportQueueImpl& other) = delete;
  ReportQueueImpl& operator=(const ReportQueueImpl& other) = delete;

  void Flush(Priority priority, FlushCallback callback) override;

 protected:
  ReportQueueImpl(std::unique_ptr<ReportQueueConfiguration> config,
                  scoped_refptr<StorageModuleInterface> storage);

 private:
  void AddRecord(base::StringPiece record,
                 Priority priority,
                 EnqueueCallback callback) const override;

  void SendRecordToStorage(base::StringPiece record,
                           Priority priority,
                           EnqueueCallback callback) const;

  reporting::Record AugmentRecord(base::StringPiece record_data) const;

  std::unique_ptr<ReportQueueConfiguration> config_;
  scoped_refptr<StorageModuleInterface> storage_;
  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_QUEUE_IMPL_H_
