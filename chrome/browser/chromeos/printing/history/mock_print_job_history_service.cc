// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/history/mock_print_job_history_service.h"

namespace chromeos {

MockPrintJobHistoryService::MockPrintJobHistoryService() = default;

MockPrintJobHistoryService::~MockPrintJobHistoryService() = default;

void MockPrintJobHistoryService::SavePrintJobProto(
    const printing::proto::PrintJobInfo& print_job_info) {
  for (auto& observer : observers_) {
    observer.OnPrintJobFinished(print_job_info);
  }
}

}  // namespace chromeos
