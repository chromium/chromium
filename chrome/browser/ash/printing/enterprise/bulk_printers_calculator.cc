// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/enterprise/bulk_printers_calculator.h"

#include <optional>
#include <set>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/values.h"
#include "chrome/browser/ash/printing/enterprise/managed_printer_translator.h"
#include "components/device_event_log/device_event_log.h"

namespace ash {

namespace {

constexpr int kMaxRecords = 20000;

// Represents a task scheduled to process in the Restrictions class.
struct TaskDataInternal {
  const unsigned task_id;  // unique ID in increasing order
  std::unordered_map<std::string, chromeos::Printer>
      printers;  // resultant list (output)
  explicit TaskDataInternal(unsigned id) : task_id(id) {}
};

using PrinterCache = std::vector<std::optional<chromeos::Printer>>;
using TaskData = std::unique_ptr<TaskDataInternal>;

// Parses |data|, a JSON blob, into a vector of Printers.  If |data| cannot be
// parsed, returns nullptr.  This is run off the UI thread as it could be very
// slow.
std::optional<PrinterCache> ParsePrinters(std::unique_ptr<std::string> data) {
  if (!data) {
    PRINTER_LOG(ERROR) << "Failed to parse printers policy ("
                       << "received null data)";
    return std::nullopt;
  }

  // This could be really slow.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  auto value_with_error = base::JSONReader::ReadAndReturnValueWithError(
      *data, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);

  if (!value_with_error.has_value()) {
    PRINTER_LOG(ERROR) << "Failed to parse printers policy ("
                       << value_with_error.error().message << ") on line "
                       << value_with_error.error().line << " at position "
                       << value_with_error.error().column;
    return std::nullopt;
  }

  base::Value& json_blob = *value_with_error;
  if (!json_blob.is_list()) {
    PRINTER_LOG(ERROR) << "Failed to parse printers policy "
                       << "(an array was expected)";
    return std::nullopt;
  }

  const base::Value::List& printer_list = json_blob.GetList();
  if (printer_list.size() > kMaxRecords) {
    PRINTER_LOG(ERROR) << "Failed to parse printers policy ("
                       << "too many records: " << printer_list.size() << ")";
    return std::nullopt;
  }

  PrinterCache parsed_printers;
  parsed_printers.reserve(printer_list.size());
  for (const base::Value& val : printer_list) {
    if (!val.is_dict()) {
      PRINTER_LOG(ERROR) << "Entry in printers policy skipped ("
                         << "not a dictionary)";
      continue;
    }

    auto managed_printer =
        chromeos::ManagedPrinterConfigFromDict(val.GetDict());
    if (!managed_printer) {
      PRINTER_LOG(ERROR) << "Entry in printers policy skipped ("
                         << "failed to parse printer configuration)";
      continue;
    }
    auto printer = chromeos::PrinterFromManagedPrinterConfig(*managed_printer);
    if (!printer) {
      PRINTER_LOG(ERROR) << "Entry in printers policy skipped ("
                         << "does not represent a printer)";
      continue;
    }
    parsed_printers.push_back(std::move(printer));
  }

  return parsed_printers;
}

// Computes the effective printer list using the access mode and
// blocklist/allowlist.  Methods are required to be sequenced.  This object is
// the owner of all the policy data. Methods updating the list of available
// printers take TaskData (see above) as |task_data| parameter and returned it.
class Restrictions : public base::RefCountedThreadSafe<Restrictions> {
 public:
  Restrictions() { DETACH_FROM_SEQUENCE(sequence_checker_); }

  Restrictions(const Restrictions&) = delete;
  Restrictions& operator=(const Restrictions&) = delete;

  // Sets the printer cache using the policy blob |data|.
  TaskData SetData(TaskData task_data, std::unique_ptr<std::string> data) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    printers_cache_ = ParsePrinters(std::move(data));
    return ComputePrinters(std::move(task_data));
  }

  // Clear the printer cache.
  void ClearData() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    printers_cache_.reset();
  }

  // Sets the access mode to |mode|.
  TaskData UpdateAccessMode(TaskData task_data,
                            BulkPrintersCalculator::AccessMode mode) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    mode_ = mode;
    return ComputePrinters(std::move(task_data));
  }

  // Sets the blocklist to |blocklist|.
  TaskData UpdateBlocklist(TaskData task_data,
                           const std::vector<std::string>& blocklist) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    has_blocklist_ = true;
    blocklist_ = std::set<std::string>(blocklist.begin(), blocklist.end());
    return ComputePrinters(std::move(task_data));
  }

  // Sets the allowlist to |allowlist|.
  TaskData UpdateAllowlist(TaskData task_data,
                           const std::vector<std::string>& allowlist) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    has_allowlist_ = true;
    allowlist_ = std::set<std::string>(allowlist.begin(), allowlist.end());
    return ComputePrinters(std::move(task_data));
  }

 private:
  friend class base::RefCountedThreadSafe<Restrictions>;
  ~Restrictions() {}

  // Returns true if we have enough data to compute the effective printer list.
  bool IsReady() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!printers_cache_) {
      return false;
    }
    switch (mode_) {
      case BulkPrintersCalculator::AccessMode::ALL_ACCESS:
        return true;
      case BulkPrintersCalculator::AccessMode::BLOCKLIST_ONLY:
        return has_blocklist_;
      case BulkPrintersCalculator::AccessMode::ALLOWLIST_ONLY:
        return has_allowlist_;
      case BulkPrintersCalculator::AccessMode::UNSET:
        return false;
    }
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  // Calculates resultant list of available printers.
  TaskData ComputePrinters(TaskData task_data) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!IsReady()) {
      return task_data;
    }

    switch (mode_) {
      case BulkPrintersCalculator::UNSET:
        NOTREACHED_IN_MIGRATION();
        break;
      case BulkPrintersCalculator::ALLOWLIST_ONLY:
        for (const auto& printer : *printers_cache_) {
          if (base::Contains(allowlist_, printer->id())) {
            task_data->printers.insert({printer->id(), *printer});
          }
        }
        break;
      case BulkPrintersCalculator::BLOCKLIST_ONLY:
        for (const auto& printer : *printers_cache_) {
          if (!base::Contains(blocklist_, printer->id())) {
            task_data->printers.insert({printer->id(), *printer});
          }
        }
        break;
      case BulkPrintersCalculator::ALL_ACCESS:
        for (const auto& printer : *printers_cache_) {
          task_data->printers.insert({printer->id(), *printer});
        }
        break;
    }

    return task_data;
  }

  // Cache of the parsed printer configuration file.
  std::optional<PrinterCache> printers_cache_;
  // The type of restriction which is enforced.
  BulkPrintersCalculator::AccessMode mode_ = BulkPrintersCalculator::UNSET;
  // Blocklist: the list of ids which should not appear in the final list.
  bool has_blocklist_ = false;
  std::set<std::string> blocklist_;
  // Allowlist: the list of the only ids which should appear in the final list.
  bool has_allowlist_ = false;
  std::set<std::string> allowlist_;

  SEQUENCE_CHECKER(sequence_checker_);
};

class BulkPrintersCalculatorImpl : public BulkPrintersCalculator {
 public:
  BulkPrintersCalculatorImpl()
      : restrictions_(base::MakeRefCounted<Restrictions>()),
        restrictions_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}

  BulkPrintersCalculatorImpl(const BulkPrintersCalculatorImpl&) = delete;
  BulkPrintersCalculatorImpl& operator=(const BulkPrintersCalculatorImpl&) =
      delete;

  void AddObserver(Observer* observer) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    observers_.RemoveObserver(observer);
  }

  void ClearData() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    data_is_set_ = false;
    last_processed_task_ = ++last_received_task_;
    printers_.clear();
    // Forward data to Restrictions to clear "Data".
    restrictions_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Restrictions::ClearData, restrictions_));
    // Notify observers.
    for (auto& observer : observers_) {
      observer.OnPrintersChanged(this);
    }
  }

  void SetData(std::unique_ptr<std::string> data) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (data) {
      PRINTER_LOG(DEBUG) << "BulkPrintersCalculator::SetData() with "
                         << data->size() << " bytes.";
    } else {
      PRINTER_LOG(ERROR) << "BulkPrintersCalculator::SetData() with nullptr.";
    }
    data_is_set_ = true;
    TaskData task_data =
        std::make_unique<TaskDataInternal>(++last_received_task_);
    restrictions_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&Restrictions::SetData, restrictions_,
                       std::move(task_data), std::move(data)),
        base::BindOnce(&BulkPrintersCalculatorImpl::OnComputationComplete,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void SetAccessMode(AccessMode mode) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TaskData task_data =
        std::make_unique<TaskDataInternal>(++last_received_task_);
    restrictions_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&Restrictions::UpdateAccessMode, restrictions_,
                       std::move(task_data), mode),
        base::BindOnce(&BulkPrintersCalculatorImpl::OnComputationComplete,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void SetBlocklist(const std::vector<std::string>& blocklist) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TaskData task_data =
        std::make_unique<TaskDataInternal>(++last_received_task_);
    restrictions_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&Restrictions::UpdateBlocklist, restrictions_,
                       std::move(task_data), blocklist),
        base::BindOnce(&BulkPrintersCalculatorImpl::OnComputationComplete,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void SetAllowlist(const std::vector<std::string>& allowlist) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TaskData task_data =
        std::make_unique<TaskDataInternal>(++last_received_task_);
    restrictions_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&Restrictions::UpdateAllowlist, restrictions_,
                       std::move(task_data), allowlist),
        base::BindOnce(&BulkPrintersCalculatorImpl::OnComputationComplete,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  bool IsDataPolicySet() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return data_is_set_;
  }

  bool IsComplete() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return (last_processed_task_ == last_received_task_);
  }

  std::unordered_map<std::string, chromeos::Printer> GetPrinters()
      const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return printers_;
  }

  base::WeakPtr<BulkPrintersCalculator> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // Called on computation completion. |task_data| corresponds to finalized
  // task.
  void OnComputationComplete(TaskData task_data) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!task_data || task_data->task_id <= last_processed_task_) {
      // The task is outdated (ClearData() was called in the meantime).
      return;
    }
    last_processed_task_ = task_data->task_id;
    if (last_processed_task_ < last_received_task_ && printers_.empty() &&
        task_data->printers.empty()) {
      // No changes in the object's state.
      return;
    }
    printers_.swap(task_data->printers);
    task_data.reset();
    // Notifies observers about changes.
    for (auto& observer : observers_) {
      observer.OnPrintersChanged(this);
    }
  }

  // Holds the blocklist and allowlist.  Computes the effective printer list.
  scoped_refptr<Restrictions> restrictions_;
  // Off UI sequence for computing the printer view.
  scoped_refptr<base::SequencedTaskRunner> restrictions_runner_;

  // True if printers_ is based on a current policy.
  bool data_is_set_ = false;
  // Id of the last scheduled task.
  unsigned last_received_task_ = 0;
  // Id of the last completed task.
  unsigned last_processed_task_ = 0;
  // The computed set of printers.
  std::unordered_map<std::string, chromeos::Printer> printers_;

  base::ObserverList<BulkPrintersCalculator::Observer>::Unchecked observers_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<BulkPrintersCalculatorImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
std::unique_ptr<BulkPrintersCalculator> BulkPrintersCalculator::Create() {
  return std::make_unique<BulkPrintersCalculatorImpl>();
}

}  // namespace ash
