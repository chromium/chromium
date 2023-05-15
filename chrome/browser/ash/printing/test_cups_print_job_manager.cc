// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/test_cups_print_job_manager.h"

#include "chrome/browser/ash/printing/cups_print_job.h"

namespace ash {

TestCupsPrintJobManager::TestCupsPrintJobManager(Profile* profile)
    : CupsPrintJobManager(profile) {}

TestCupsPrintJobManager::~TestCupsPrintJobManager() = default;

void TestCupsPrintJobManager::CancelPrintJob(CupsPrintJob* job) {
  job->set_state(CupsPrintJob::State::STATE_CANCELLED);
  NotifyJobCanceled(job->GetWeakPtr());
}

bool TestCupsPrintJobManager::SuspendPrintJob(CupsPrintJob* job) {
  job->set_state(CupsPrintJob::State::STATE_SUSPENDED);
  NotifyJobSuspended(job->GetWeakPtr());
  return true;
}

bool TestCupsPrintJobManager::ResumePrintJob(CupsPrintJob* job) {
  job->set_state(CupsPrintJob::State::STATE_RESUMED);
  NotifyJobResumed(job->GetWeakPtr());
  return true;
}

bool TestCupsPrintJobManager::CreatePrintJob(
    const std::string& printer_id,
    const std::string& title,
    uint32_t job_id,
    int total_page_number,
    ::printing::PrintJob::Source source,
    const std::string& source_id,
    const printing::proto::PrintSettings& settings) {
  return false;
}

void TestCupsPrintJobManager::CreatePrintJob(CupsPrintJob* job) {
  job->set_state(CupsPrintJob::State::STATE_NONE);
  NotifyJobCreated(job->GetWeakPtr());
}

void TestCupsPrintJobManager::StartPrintJob(CupsPrintJob* job) {
  job->set_state(CupsPrintJob::State::STATE_STARTED);
  NotifyJobStarted(job->GetWeakPtr());
}

void TestCupsPrintJobManager::FailPrintJob(CupsPrintJob* job) {
  job->set_state(CupsPrintJob::State::STATE_FAILED);
  NotifyJobFailed(job->GetWeakPtr());
}

void TestCupsPrintJobManager::CompletePrintJob(CupsPrintJob* job) {
  job->set_state(CupsPrintJob::State::STATE_DOCUMENT_DONE);
  NotifyJobDone(job->GetWeakPtr());
}

}  // namespace ash
