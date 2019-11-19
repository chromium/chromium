// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINT_JOB_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINT_JOB_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/printing/history/print_job_info.pb.h"
#include "chrome/browser/chromeos/printing/printer_error_codes.h"
#include "chrome/browser/printing/print_job.h"
#include "chromeos/printing/printer_configuration.h"

namespace chromeos {

class CupsPrintJob {
 public:
  enum class State {
    STATE_NONE,
    STATE_WAITING,
    STATE_STARTED,
    STATE_PAGE_DONE,
    STATE_CANCELLED,
    STATE_SUSPENDED,
    STATE_RESUMED,
    STATE_DOCUMENT_DONE,
    STATE_FAILED,
    STATE_ERROR,
  };

  CupsPrintJob(const Printer& printer,
               int job_id,
               const std::string& document_title,
               int total_page_number,
               ::printing::PrintJob::Source source,
               const std::string& source_id,
               const printing::proto::PrintSettings& settings);
  ~CupsPrintJob();

  // Create a unique id for a print job using the |printer_id| and |job_id|.
  static std::string CreateUniqueId(const std::string& printer_id, int job_id);

  // Returns a unique id for the print job.
  std::string GetUniqueId() const;

  // Returns weak pointer to |this| CupsPrintJob
  base::WeakPtr<CupsPrintJob> GetWeakPtr();

  // Returns whether this print_job has timed out or not.
  bool IsExpired() const;

  // Getters.
  const Printer& printer() const { return printer_; }
  int job_id() const { return job_id_; }
  const std::string& document_title() const { return document_title_; }
  int total_page_number() const { return total_page_number_; }
  int printed_page_number() const { return printed_page_number_; }
  ::printing::PrintJob::Source source() const { return source_; }
  const std::string& source_id() const { return source_id_; }
  const printing::proto::PrintSettings& settings() const { return settings_; }
  base::Time creation_time() const { return creation_time_; }
  State state() const { return state_; }
  PrinterErrorCode error_code() const { return error_code_; }

  // Setters.
  void set_printed_page_number(int page_number) {
    printed_page_number_ = page_number;
  }
  void set_state(State state) { state_ = state; }
  void set_error_code(PrinterErrorCode error_code) { error_code_ = error_code; }

  // Returns true if |state_| represents a terminal state.
  bool IsJobFinished() const;

  // Returns true if cups pipeline failed.
  bool PipelineDead() const;

 private:
  const Printer printer_;
  const int job_id_;

  std::string document_title_;
  const int total_page_number_;
  int printed_page_number_ = 0;
  const ::printing::PrintJob::Source source_;
  const std::string source_id_;
  const printing::proto::PrintSettings settings_;
  const base::Time creation_time_;

  State state_ = State::STATE_NONE;
  PrinterErrorCode error_code_ = PrinterErrorCode::NO_ERROR;

  base::WeakPtrFactory<CupsPrintJob> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CupsPrintJob);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINT_JOB_H_
