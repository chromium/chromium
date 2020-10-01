// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_MOCK_REPORT_QUEUE_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_MOCK_REPORT_QUEUE_H_

#include "base/callback.h"
#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
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

  Status Enqueue(base::StringPiece record,
                 EnqueueCallback callback) const override {
    return StringPieceEnqueue_(record, std::move(callback));
  }

  Status Enqueue(const base::Value& record,
                 EnqueueCallback callback) const override {
    return ValueEnqueue_(record, std::move(callback));
  }

  Status Enqueue(google::protobuf::MessageLite* record,
                 EnqueueCallback callback) const override {
    return MessageLiteEnqueue_(record, std::move(callback));
  }

  MOCK_METHOD(Status,
              StringPieceEnqueue_,
              (base::StringPiece record, EnqueueCallback callback),
              (const));

  MOCK_METHOD(Status,
              ValueEnqueue_,
              (const base::Value& record, EnqueueCallback callback),
              (const));

  MOCK_METHOD(Status,
              MessageLiteEnqueue_,
              (google::protobuf::MessageLite * record,
               EnqueueCallback callback),
              (const));
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_MOCK_REPORT_QUEUE_H_
