// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_TEST_CUPS_WRAPPER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_TEST_CUPS_WRAPPER_H_

#include "base/containers/flat_map.h"
#include "chrome/browser/chromeos/printing/cups_wrapper.h"

namespace chromeos {

// A test wrapper which allows to substitute the status of requested printer.
class TestCupsWrapper : public CupsWrapper {
 public:
  TestCupsWrapper();
  ~TestCupsWrapper() override;

  // CupsWrapper:
  void QueryCupsPrintJobs(
      const std::vector<std::string>& printer_ids,
      base::OnceCallback<void(std::unique_ptr<QueryResult>)> callback) override;
  void CancelJob(const std::string& printer_id, int job_id) override;
  void QueryCupsPrinterStatus(
      const std::string& printer_id,
      base::OnceCallback<void(std::unique_ptr<::printing::PrinterStatus>)>
          callback) override;

  void SetPrinterStatus(
      const std::string& printer_id,
      const ::printing::PrinterStatus::PrinterReason& printer_reason);

 private:
  base::flat_map<std::string, ::printing::PrinterStatus::PrinterReason>
      printer_reasons_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_TEST_CUPS_WRAPPER_H_
