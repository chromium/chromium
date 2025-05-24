// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_METRICS_PRINTING_METRICS_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_METRICS_PRINTING_METRICS_API_H_

#include <vector>

#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace ash::printing::proto {
class PrintJobInfo;
}  // namespace ash::printing::proto

namespace extensions {

class PrintingMetricsGetPrintJobsFunction : public ExtensionFunction {
 protected:
  ~PrintingMetricsGetPrintJobsFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnPrintJobsRetrieved(
      bool success,
      std::vector<ash::printing::proto::PrintJobInfo> proto_infos);

  DECLARE_EXTENSION_FUNCTION("printingMetrics.getPrintJobs",
                             PRINTINGMETRICS_GETPRINTJOBS)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_METRICS_PRINTING_METRICS_API_H_
