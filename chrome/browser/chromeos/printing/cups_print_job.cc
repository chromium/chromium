// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/cups_print_job.h"

#include "base/strings/stringprintf.h"

namespace chromeos {

CupsPrintJob::CupsPrintJob(const Printer& printer,
                           int job_id,
                           const std::string& document_title,
                           int total_page_number,
                           ::printing::PrintJob::Source source,
                           const std::string& source_id,
                           const printing::proto::PrintSettings& settings)
    : printer_(printer),
      job_id_(job_id),
      document_title_(document_title),
      total_page_number_(total_page_number),
      source_(source),
      source_id_(source_id),
      settings_(settings),
      creation_time_(base::Time::Now()) {}

CupsPrintJob::~CupsPrintJob() = default;

std::string CupsPrintJob::GetUniqueId() const {
  return CreateUniqueId(printer_.id(), job_id_);
}

base::WeakPtr<CupsPrintJob> CupsPrintJob::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool CupsPrintJob::IsExpired() const {
  return error_code_ == PrinterErrorCode::PRINTER_UNREACHABLE;
}

// static
std::string CupsPrintJob::CreateUniqueId(const std::string& printer_id,
                                         int job_id) {
  return base::StringPrintf("%s%d", printer_id.c_str(), job_id);
}

bool CupsPrintJob::IsJobFinished() const {
  return state_ == CupsPrintJob::State::STATE_CANCELLED ||
         state_ == CupsPrintJob::State::STATE_FAILED ||
         state_ == CupsPrintJob::State::STATE_DOCUMENT_DONE;
}

bool CupsPrintJob::PipelineDead() const {
  return error_code_ == PrinterErrorCode::FILTER_FAILED;
}

}  // namespace chromeos
