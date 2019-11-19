// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/cups_wrapper.h"

#include <cups/cups.h>

#include "base/callback.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task_runner_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "printing/backend/cups_printer.h"
#include "url/gurl.h"

namespace chromeos {

CupsWrapper::QueryResult::QueryResult() = default;

CupsWrapper::QueryResult::~QueryResult() = default;

class CupsWrapper::Backend {
 public:
  Backend();
  Backend(const Backend&) = delete;
  Backend& operator=(const Backend&) = delete;
  ~Backend();

  std::unique_ptr<QueryResult> QueryCupsPrintJobs(
      const std::vector<std::string>& printer_ids);

  void CancelJob(const std::string& printer_id, int job_id);

  std::unique_ptr<::printing::PrinterStatus> QueryCupsPrinterStatus(
      const std::string& printer_id);

 private:
  ::printing::CupsConnection cups_connection_;

  SEQUENCE_CHECKER(sequence_checker_);
};

CupsWrapper::Backend::Backend()
    : cups_connection_(GURL(), HTTP_ENCRYPT_NEVER, false) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

CupsWrapper::Backend::~Backend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<CupsWrapper::QueryResult>
CupsWrapper::Backend::QueryCupsPrintJobs(
    const std::vector<std::string>& printer_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto result = std::make_unique<CupsWrapper::QueryResult>();
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  result->success = cups_connection_.GetJobs(printer_ids, &result->queues);
  return result;
}

void CupsWrapper::Backend::CancelJob(const std::string& printer_id,
                                     int job_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::unique_ptr<::printing::CupsPrinter> printer =
      cups_connection_.GetPrinter(printer_id);
  if (!printer) {
    LOG(WARNING) << "Printer not found: " << printer_id;
    return;
  }

  if (!printer->CancelJob(job_id)) {
    // This is not expected to fail but log it if it does.
    LOG(WARNING) << "Cancelling job failed.  Job may be stuck in queue.";
  }
}

std::unique_ptr<::printing::PrinterStatus>
CupsWrapper::Backend::QueryCupsPrinterStatus(const std::string& printer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto result = std::make_unique<::printing::PrinterStatus>();
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (!cups_connection_.GetPrinterStatus(printer_id, result.get()))
    return nullptr;
  return result;
}

CupsWrapper::CupsWrapper()
    : backend_(std::make_unique<Backend>()),
      backend_task_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}

CupsWrapper::~CupsWrapper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->DeleteSoon(FROM_HERE, backend_.release());
}

void CupsWrapper::QueryCupsPrintJobs(
    const std::vector<std::string>& printer_ids,
    base::OnceCallback<void(std::unique_ptr<CupsWrapper::QueryResult>)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // It's safe to pass unretained pointer here because we delete |backend_| on
  // the same task runner.
  base::PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&Backend::QueryCupsPrintJobs,
                     base::Unretained(backend_.get()), printer_ids),
      std::move(callback));
}

void CupsWrapper::CancelJob(const std::string& printer_id, int job_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // It's safe to pass unretained pointer here because we delete |backend_| on
  // the same task runner.
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Backend::CancelJob, base::Unretained(backend_.get()),
                     printer_id, job_id));
}

void CupsWrapper::QueryCupsPrinterStatus(
    const std::string& printer_id,
    base::OnceCallback<void(std::unique_ptr<::printing::PrinterStatus>)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // It's safe to pass unretained pointer here because we delete |backend_| on
  // the same task runner.
  base::PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&Backend::QueryCupsPrinterStatus,
                     base::Unretained(backend_.get()), printer_id),
      std::move(callback));
}

}  // namespace chromeos
