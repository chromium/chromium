// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PRINTING_PRINTING_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_PRINTING_PRINTING_API_H_

#include <memory>
#include <string>

#include "chrome/common/extensions/api/printing.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Value;
}  // namespace base

namespace extensions {

class PrintingSubmitJobFunction : public ExtensionFunction {
 protected:
  ~PrintingSubmitJobFunction() override;

  // ExtensionFunction:
  void GetQuotaLimitHeuristics(QuotaLimitHeuristics* heuristics) const override;
  ResponseAction Run() override;

 private:
  void OnPrintJobSubmitted(
      absl::optional<api::printing::SubmitJobStatus> status,
      absl::optional<std::string> job_id,
      absl::optional<std::string> error);
  DECLARE_EXTENSION_FUNCTION("printing.submitJob", PRINTING_SUBMITJOB)
};

class PrintingCancelJobFunction : public ExtensionFunction {
 protected:
  ~PrintingCancelJobFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("printing.cancelJob", PRINTING_CANCELJOB)
};

class PrintingGetPrintersFunction : public ExtensionFunction {
 public:
  PrintingGetPrintersFunction();

 protected:
  ~PrintingGetPrintersFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnPrintersReady(std::vector<api::printing::Printer> printers);
  DECLARE_EXTENSION_FUNCTION("printing.getPrinters", PRINTING_GETPRINTERS)
};

class PrintingGetPrinterInfoFunction : public ExtensionFunction {
 protected:
  ~PrintingGetPrinterInfoFunction() override;

  // ExtensionFunction:
  void GetQuotaLimitHeuristics(QuotaLimitHeuristics* heuristics) const override;
  ResponseAction Run() override;

 private:
  void OnPrinterInfoRetrieved(
      absl::optional<base::Value> capabilities,
      absl::optional<api::printing::PrinterStatus> status,
      absl::optional<std::string> error);
  DECLARE_EXTENSION_FUNCTION("printing.getPrinterInfo", PRINTING_GETPRINTERINFO)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PRINTING_PRINTING_API_H_
