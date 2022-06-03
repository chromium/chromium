// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_METRICS_PRINTING_METRICS_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_METRICS_PRINTING_METRICS_API_H_

#include <vector>

#include "chrome/browser/chromeos/printing/history/print_job_info.pb.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace extensions {

class PrintingMetricsGetPrintJobsFunction : public ExtensionFunction {
 protected:
  ~PrintingMetricsGetPrintJobsFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnPrintJobsRetrieved(bool success,
                            std::vector<chromeos::printing::proto::PrintJobInfo>
                                print_job_info_protos);
  DECLARE_EXTENSION_FUNCTION("printingMetrics.getPrintJobs",
                             PRINTINGMETRICS_GETPRINTJOBS)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_METRICS_PRINTING_METRICS_API_H_
