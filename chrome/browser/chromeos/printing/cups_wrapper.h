// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_WRAPPER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_WRAPPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "printing/backend/cups_connection.h"

namespace chromeos {

// A wrapper around the CUPS connection to ensure that it's always accessed on
// the same sequence and run in the appropriate sequence off of the calling
// sequence.
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

  CupsWrapper();
  CupsWrapper(const CupsWrapper&) = delete;
  CupsWrapper& operator=(const CupsWrapper&) = delete;
  ~CupsWrapper();

  // Queries CUPS for the current jobs for the given |printer_ids|. Passes
  // the result to |callback|.
  void QueryCupsPrintJobs(
      const std::vector<std::string>& printer_ids,
      base::OnceCallback<void(std::unique_ptr<QueryResult>)> callback);

  // Cancels the print job on the blocking thread.
  void CancelJob(const std::string& printer_id, int job_id);

  // Queries CUPS for the printer status for the given |printer_id|. Passes the
  // result to |callback|.
  void QueryCupsPrinterStatus(
      const std::string& printer_id,
      base::OnceCallback<void(std::unique_ptr<::printing::PrinterStatus>)>
          callback);

 private:
  class Backend;
  // The |backend_| handles all communication with CUPS.
  // It is instantiated on the thread |this| runs on but after that,
  // must only be accessed and eventually destroyed via the
  // |backend_task_runner_|.
  std::unique_ptr<Backend> backend_;

  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_WRAPPER_H_
