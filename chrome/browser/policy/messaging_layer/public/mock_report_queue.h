// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_MOCK_REPORT_QUEUE_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_MOCK_REPORT_QUEUE_H_

#include "base/callback.h"
#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "components/policy/proto/record.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace reporting {

// A mock of ReportQueue for use in testing.
class MockReportQueue : public ReportQueue {
 public:
  // An EnqueueCallbacks are called on the completion of any |Enqueue| call.
  // using EnqueueCallback = base::OnceCallback<void(Status)>;

  MockReportQueue();
  ~MockReportQueue() override;

  void Enqueue(base::StringPiece record,
               Priority priority,
               EnqueueCallback callback) const override {
    StringPieceEnqueue_(record, priority, std::move(callback));
  }

  void Enqueue(const base::Value& record,
               Priority priority,
               EnqueueCallback callback) const override {
    ValueEnqueue_(record, priority, std::move(callback));
  }

  void Enqueue(google::protobuf::MessageLite* record,
               Priority priority,
               EnqueueCallback callback) const override {
    MessageLiteEnqueue_(record, priority, std::move(callback));
  }

  MOCK_METHOD(void,
              StringPieceEnqueue_,
              (base::StringPiece record,
               Priority priority,
               EnqueueCallback callback),
              (const));

  MOCK_METHOD(void,
              ValueEnqueue_,
              (const base::Value& record,
               Priority priority,
               EnqueueCallback callback),
              (const));

  MOCK_METHOD(void,
              MessageLiteEnqueue_,
              (google::protobuf::MessageLite * record,
               Priority priority,
               EnqueueCallback callback),
              (const));
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_MOCK_REPORT_QUEUE_H_
