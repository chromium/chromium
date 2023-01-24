// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/printer_info.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/printing/cups_printer_status.h"
#include "printing/printer_status.h"

namespace ash {

void QueryIppPrinter(const std::string& host,
                     const int port,
                     const std::string& path,
                     bool encrypted,
                     PrinterInfoCallback callback) {
  DCHECK(!host.empty());

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback), printing::PrinterQueryResult::kUnknownFailure,
          printing::PrinterStatus(), /*make_and_model=*/"Foo Bar",
          /*document_formats=*/std::vector<std::string>{},
          /*ipp_everywhere=*/false, chromeos::PrinterAuthenticationInfo{}));
}

}  // namespace ash
