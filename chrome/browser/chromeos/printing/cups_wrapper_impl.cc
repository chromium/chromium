// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cups/cups.h>

#include <utility>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/chromeos/printing/cups_wrapper.h"
#include "printing/backend/cups_printer.h"
#include "url/gurl.h"

namespace chromeos {

namespace {
CupsWrapper::CupsWrapperFactory& GetCupsWrapperFactoryForTesting() {
  static base::NoDestructor<CupsWrapper::CupsWrapperFactory>
      factory_for_testing;
  return *factory_for_testing;
}
}  // namespace

// A wrapper around the CUPS connection to ensure that it's always accessed on
// the same sequence and run in the appropriate sequence off of the calling
// sequence.
class CupsWrapperImpl : public CupsWrapper {
 public:
  CupsWrapperImpl()
      : backend_(std::make_unique<Backend>()),
        backend_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}

  ~CupsWrapperImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    backend_task_runner_->DeleteSoon(FROM_HERE, backend_.release());
  }

  CupsWrapperImpl(const CupsWrapperImpl&) = delete;
  CupsWrapperImpl& operator=(const CupsWrapperImpl&) = delete;

  // CupsWrapper:
  void QueryCupsPrintJobs(
      const std::vector<std::string>& printer_ids,
      base::OnceCallback<void(std::unique_ptr<CupsWrapperImpl::QueryResult>)>
          callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // It's safe to pass unretained pointer here because we delete |backend_| on
    // the same task runner.
    backend_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&Backend::QueryCupsPrintJobs,
                       base::Unretained(backend_.get()), printer_ids),
        std::move(callback));
  }

  void CancelJob(const std::string& printer_id, int job_id) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // It's safe to pass unretained pointer here because we delete |backend_| on
    // the same task runner.
    backend_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&Backend::CancelJob, base::Unretained(backend_.get()),
                       printer_id, job_id));
  }

  void QueryCupsPrinterStatus(
      const std::string& printer_id,
      base::OnceCallback<void(std::unique_ptr<::printing::PrinterStatus>)>
          callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // It's safe to pass unretained pointer here because we delete |backend_| on
    // the same task runner.
    backend_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&Backend::QueryCupsPrinterStatus,
                       base::Unretained(backend_.get()), printer_id),
        std::move(callback));
  }

 private:
  class Backend {
   public:
    Backend() : cups_connection_(::printing::CupsConnection::Create()) {
      DETACH_FROM_SEQUENCE(sequence_checker_);
    }
    ~Backend() { DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_); }

    Backend(const Backend&) = delete;
    Backend& operator=(const Backend&) = delete;

    std::unique_ptr<QueryResult> QueryCupsPrintJobs(
        const std::vector<std::string>& printer_ids) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      auto result = std::make_unique<CupsWrapperImpl::QueryResult>();
      base::ScopedBlockingCall scoped_blocking_call(
          FROM_HERE, base::BlockingType::MAY_BLOCK);
      result->success = cups_connection_->GetJobs(printer_ids, &result->queues);
      return result;
    }

    void CancelJob(const std::string& printer_id, int job_id) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      base::ScopedBlockingCall scoped_blocking_call(
          FROM_HERE, base::BlockingType::MAY_BLOCK);

      std::unique_ptr<::printing::CupsPrinter> printer =
          cups_connection_->GetPrinter(printer_id);
      if (!printer) {
        LOG(WARNING) << "Printer not found: " << printer_id;
        return;
      }

      if (!printer->CancelJob(job_id)) {
        // This is not expected to fail but log it if it does.
        LOG(WARNING) << "Cancelling job failed.  Job may be stuck in queue.";
      }
    }

    std::unique_ptr<::printing::PrinterStatus> QueryCupsPrinterStatus(
        const std::string& printer_id) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      auto result = std::make_unique<::printing::PrinterStatus>();
      base::ScopedBlockingCall scoped_blocking_call(
          FROM_HERE, base::BlockingType::MAY_BLOCK);
      if (!cups_connection_->GetPrinterStatus(printer_id, result.get()))
        return nullptr;
      return result;
    }

   private:
    std::unique_ptr<::printing::CupsConnection> cups_connection_;

    SEQUENCE_CHECKER(sequence_checker_);
  };

  // The |backend_| handles all communication with CUPS.
  // It is instantiated on the thread |this| runs on but after that,
  // must only be accessed and eventually destroyed via the
  // |backend_task_runner_|.
  std::unique_ptr<Backend> backend_;

  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// static
std::unique_ptr<CupsWrapper> CupsWrapper::Create() {
  if (auto& testing_factory = GetCupsWrapperFactoryForTesting()) {
    return testing_factory.Run();
  }
  return std::make_unique<CupsWrapperImpl>();
}

// static
void CupsWrapper::SetCupsWrapperFactoryForTesting(CupsWrapperFactory factory) {
  GetCupsWrapperFactoryForTesting() = std::move(factory);
}

}  // namespace chromeos
