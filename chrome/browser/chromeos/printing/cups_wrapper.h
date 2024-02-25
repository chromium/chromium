// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_WRAPPER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_WRAPPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "printing/backend/cups_connection.h"

namespace chromeos {

// A wrapper around the CUPS connection.
class CupsWrapper {
 public:
  // Container for results from CUPS queries.
  struct QueryResult {
    QueryResult();
    QueryResult(const QueryResult& other) = delete;
    QueryResult& operator=(const QueryResult& other) = delete;
    ~QueryResult();

    bool success;
    std::vector<::printing::QueueStatus> queues;
  };

  static std::unique_ptr<CupsWrapper> Create();

  using CupsWrapperFactory = base::RepeatingCallback<decltype(Create)>;
  static void SetCupsWrapperFactoryForTesting(CupsWrapperFactory factory);

  virtual ~CupsWrapper();

  // Queries CUPS for the current jobs for the given |printer_ids|. Passes
  // the result to |callback|.
  virtual void QueryCupsPrintJobs(
      const std::vector<std::string>& printer_ids,
      base::OnceCallback<void(std::unique_ptr<QueryResult>)> callback) = 0;

  // Cancels the print job on the blocking thread.
  virtual void CancelJob(const std::string& printer_id, int job_id) = 0;

  // Queries CUPS for the printer status for the given |printer_id|. Passes the
  // result to |callback|.
  virtual void QueryCupsPrinterStatus(
      const std::string& printer_id,
      base::OnceCallback<void(std::unique_ptr<::printing::PrinterStatus>)>
          callback) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_WRAPPER_H_
