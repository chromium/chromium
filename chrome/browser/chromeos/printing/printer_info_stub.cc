// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/printer_info.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace chromeos {

void QueryIppPrinter(const std::string& host,
                     const int port,
                     const std::string& path,
                     bool encrypted,
                     PrinterInfoCallback callback) {
  DCHECK(!host.empty());

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     printing::PrinterQueryResult::UNKNOWN_FAILURE, "Foo",
                     "Bar", "Foo Bar", std::vector<std::string>{}, false));
}

}  // namespace chromeos
