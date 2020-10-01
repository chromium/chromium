// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/public/mock_report_queue.h"

namespace reporting {

MockReportQueue::MockReportQueue()
    : ReportQueue(/*config=*/nullptr,
                  /*storage=*/nullptr) {}

MockReportQueue::~MockReportQueue() = default;

}  // namespace reporting
