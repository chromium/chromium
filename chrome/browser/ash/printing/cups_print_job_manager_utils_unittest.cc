// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/cups_print_job_manager_utils.h"

#include <memory>
#include <ostream>
#include <utility>
#include <vector>

#include "base/test/scoped_mock_clock_override.h"
#include "chrome/browser/ash/printing/cups_print_job.h"
#include "chrome/browser/ash/printing/history/print_job_info.pb.h"
#include "chrome/browser/printing/printer_query.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/test/browser_task_environment.h"
#include "printing/backend/cups_jobs.h"
#include "printing/print_settings.h"
#include "printing/printed_document.h"
#include "printing/printer_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

using State = CupsPrintJob::State;
using ::printing::CupsJob;
using PrinterReason = ::printing::PrinterStatus::PrinterReason;
using ::chromeos::PrinterErrorCode;

constexpr int kTotalPages = 2;

// Timeout value defined in cups_print_job_manager_utils.cc
constexpr int kTimeout = 30;

struct Params {
  Params(State state,
         int pages,
         CupsJob::JobState job_state,
         std::vector<CupsJob::JobStateReason> job_state_reasons,
         int job_pages,
         State expected_state,
         int expected_pages)
      : state(state),
        pages(pages),
        job_state(job_state),
        job_state_reasons(std::move(job_state_reasons)),
        job_pages(job_pages),
        expected_state(expected_state),
        expected_pages(expected_pages) {}

  // Test state transition that does not affect the page count.
  // `job_pages` is set to `kTotalPages` so that terminal events such as
  // `JobState::COMPLETED` work properly.
  Params(State state,
         CupsJob::JobState job_state,
         std::vector<CupsJob::JobStateReason> job_state_reasons,
         State expected_state)
      : Params(state,
               0,
               job_state,
               std::move(job_state_reasons),
               kTotalPages,
               expected_state,
               0) {}

  State state;
  int pages;
  CupsJob::JobState job_state;
  std::vector<CupsJob::JobStateReason> job_state_reasons;
  // Current printed page count.
  // The total print job page count for the tests is `kTotalPages`.
  int job_pages;
  State expected_state;
  int expected_pages;
};

std::ostream& operator<<(std::ostream& out, const Params& params) {
  out << "state: " << static_cast<int>(params.state) << ", ";
  out << "pages: " << params.pages << ", ";
  out << "job_state: " << params.job_state << ", ";
  for (const auto& job_state_reason : params.job_state_reasons) {
    out << "job_state_reason: "
        << ::printing::ToJobStateReasonString(job_state_reason) << ", ";
  }
  out << "job_pages: " << params.job_pages << ", ";
  out << "expected_state: " << static_cast<int>(params.expected_state) << ", ";
  out << "expected_pages: " << params.expected_pages << std::endl;
  return out;
}

std::vector<Params> GenerateParams(CupsJob::JobState job_state,
                                   State terminal_state) {
  std::vector<Params> params;
  for (State state :
       {State::STATE_WAITING, State::STATE_STARTED, State::STATE_PAGE_DONE}) {
    params.emplace_back(state, job_state,
                        std::vector<CupsJob::JobStateReason>(), terminal_state);
  }
  return params;
}

// Transition from 'STATE_WAITING' to `STATE_STARTED` before
// `STATE_PAGE_DONE`, even if a page has finished printing.
std::vector<Params> WaitingToStarted() {
  std::vector<Params> params;

  for (int i = 0; i <= kTotalPages; i++) {
    params.emplace_back(State::STATE_WAITING, 0, CupsJob::PROCESSING,
                        std::vector<CupsJob::JobStateReason>(), i,
                        State::STATE_STARTED, i);
  }

  return params;
}

// Transition from `STATE_STARTED` to `STATE_PAGE_DOWN`.
std::vector<Params> StartedToPageDone() {
  std::vector<Params> params;

  for (int i = 1; i <= kTotalPages; i++) {
    params.emplace_back(State::STATE_STARTED, 1, CupsJob::PROCESSING,
                        std::vector<CupsJob::JobStateReason>(), i,
                        State::STATE_PAGE_DONE, i);
  }

  return params;
}

// Process the second page and subsequent pages in `STATE_PAGE_DONE`.
std::vector<Params> PageDoneToPageDone() {
  std::vector<Params> params;

  for (int i = 1; i <= kTotalPages; i++) {
    for (int j = i; j <= kTotalPages; j++) {
      params.emplace_back(State::STATE_PAGE_DONE, i, CupsJob::PROCESSING,
                          std::vector<CupsJob::JobStateReason>(), j,
                          State::STATE_PAGE_DONE, j);
    }
  }

  return params;
}

using CupsPrintJobManagerUtilsTest = testing::TestWithParam<Params>;

INSTANTIATE_TEST_SUITE_P(
    Completed,
    CupsPrintJobManagerUtilsTest,
    testing::ValuesIn(GenerateParams(CupsJob::COMPLETED,
                                     State::STATE_DOCUMENT_DONE)));
INSTANTIATE_TEST_SUITE_P(
    Cancelled,
    CupsPrintJobManagerUtilsTest,
    testing::ValuesIn(GenerateParams(CupsJob::CANCELED,
                                     State::STATE_CANCELLED)));
INSTANTIATE_TEST_SUITE_P(
    Failed,
    CupsPrintJobManagerUtilsTest,
    testing::ValuesIn(GenerateParams(CupsJob::ABORTED, State::STATE_FAILED)));

INSTANTIATE_TEST_SUITE_P(
    ClientUnauthorizedToStateFailed,
    CupsPrintJobManagerUtilsTest,
    testing::Values(
        Params(State::STATE_SUSPENDED,
               CupsJob::HELD,
               std::vector<CupsJob::JobStateReason>{
                   CupsJob::JobStateReason::kCupsHeldForAuthentication},
               State::STATE_FAILED)));

INSTANTIATE_TEST_SUITE_P(
    FilterFailedtoStateFailed,
    CupsPrintJobManagerUtilsTest,
    testing::Values(
        Params(State::STATE_SUSPENDED,
               CupsJob::STOPPED,
               std::vector<CupsJob::JobStateReason>{
                   CupsJob::JobStateReason::kJobCompletedWithErrors},
               State::STATE_FAILED)));

INSTANTIATE_TEST_SUITE_P(
    Pending,
    CupsPrintJobManagerUtilsTest,
    testing::Values(Params(State::STATE_WAITING,
                           CupsJob::PENDING,
                           std::vector<CupsJob::JobStateReason>(),
                           State::STATE_WAITING)));

INSTANTIATE_TEST_SUITE_P(ProcessingWaitingToStarted,
                         CupsPrintJobManagerUtilsTest,
                         testing::ValuesIn(WaitingToStarted()));

INSTANTIATE_TEST_SUITE_P(
    ProcessingWaitingToStartedUnknownState,
    CupsPrintJobManagerUtilsTest,
    testing::Values(Params(State::STATE_WAITING,
                           0,
                           CupsJob::PROCESSING,
                           std::vector<CupsJob::JobStateReason>(),
                           -1,
                           State::STATE_STARTED,
                           0)));

INSTANTIATE_TEST_SUITE_P(
    ProcessingStartedToStarted,
    CupsPrintJobManagerUtilsTest,
    testing::Values(Params(State::STATE_STARTED,
                           0,
                           CupsJob::PROCESSING,
                           std::vector<CupsJob::JobStateReason>(),
                           0,
                           State::STATE_STARTED,
                           0)));

INSTANTIATE_TEST_SUITE_P(
    ProcessingStartedToStartedUnknownState,
    CupsPrintJobManagerUtilsTest,
    testing::Values(Params(State::STATE_STARTED,
                           0,
                           CupsJob::PROCESSING,
                           std::vector<CupsJob::JobStateReason>(),
                           -1,
                           State::STATE_STARTED,
                           0)));

INSTANTIATE_TEST_SUITE_P(ProcessingStartedToPageDone,
                         CupsPrintJobManagerUtilsTest,
                         testing::ValuesIn(StartedToPageDone()));

INSTANTIATE_TEST_SUITE_P(ProcessingPageDoneToPageDone,
                         CupsPrintJobManagerUtilsTest,
                         testing::ValuesIn(PageDoneToPageDone()));

TEST_P(CupsPrintJobManagerUtilsTest, UpdatePrintJob) {
  const Params& params = GetParam();
  CupsJob job;
  job.id = 0;
  job.state = params.job_state;
  job.current_pages = params.job_pages;
  for (const auto& job_state_reason : params.job_state_reasons) {
    job.state_reasons.push_back(
        ::printing::ToJobStateReasonString(job_state_reason).data());
  }
  CupsPrintJob print_job(chromeos::Printer(), 0, std::string(), kTotalPages,
                         crosapi::mojom::PrintJob::Source::kUnknown,
                         std::string(), printing::proto::PrintSettings());
  print_job.set_state(params.state);
  print_job.set_printed_page_number(params.pages);
  bool expected_print_job_updated = params.state != params.expected_state ||
                                    params.pages != params.expected_pages;
  EXPECT_EQ(expected_print_job_updated,
            UpdatePrintJob(::printing::PrinterStatus(), job, &print_job));
  EXPECT_EQ(params.expected_state, print_job.state());
  EXPECT_EQ(params.expected_pages, print_job.printed_page_number());
}

// Testing the behavior that CUPS is allowed to have a few seconds to reset a
// connection ot the printer.
TEST(CupsPrintJobManagerUtilsTest, UpdatePrintJobTimeout) {
  base::ScopedMockClockOverride mock_clock;

  CupsJob job;
  job.id = 0;
  job.state = CupsJob::PROCESSING;

  CupsPrintJob print_job(chromeos::Printer(), 0, std::string(), kTotalPages,
                         crosapi::mojom::PrintJob::Source::kUnknown,
                         std::string(), printing::proto::PrintSettings());
  print_job.set_state(State::STATE_STARTED);

  PrinterReason printer_reason;
  printer_reason.severity = PrinterReason::Severity::kError;
  printer_reason.reason = PrinterReason::Reason::kTimedOut;

  ::printing::PrinterStatus printer_status;
  printer_status.reasons.push_back(printer_reason);

  // Idle time is less than CUPS timeout limit. No error should be found.
  mock_clock.Advance(base::Seconds(kTimeout - 1));

  UpdatePrintJob(printer_status, job, &print_job);

  EXPECT_EQ(print_job.error_code(), PrinterErrorCode::NO_ERROR);
  EXPECT_EQ(print_job.state(), State::STATE_STARTED);

  // Idle time is more than CUPS timeout limit. Error should be returned.
  mock_clock.Advance(base::Seconds(kTimeout + 1));

  UpdatePrintJob(printer_status, job, &print_job);

  EXPECT_EQ(print_job.error_code(), PrinterErrorCode::PRINTER_UNREACHABLE);
  EXPECT_EQ(print_job.state(), CupsPrintJob::State::STATE_FAILED);
}

// Parameters required for CalculatePrintJobPageTotalTest.
struct PrintJobPageTotalParams {
  // Pages in document.
  int pages = 0;
  // Copies requested in settings.
  int copies = 0;
  // Expected total pages to be returned.
  int expected_total = 0;
};

// Test configuration required for setting up fake PrintedDocument.
class CalculatePrintJobPageTotalTest
    : public testing::TestWithParam<PrintJobPageTotalParams> {
 public:
  CalculatePrintJobPageTotalTest()
      : task_environment_(std::make_unique<content::BrowserTaskEnvironment>()) {
  }
  ~CalculatePrintJobPageTotalTest() override = default;

 private:
  // We need to create a MessageLoop, otherwise a bunch of things fails.
  std::unique_ptr<content::BrowserTaskEnvironment> task_environment_;
};

INSTANTIATE_TEST_SUITE_P(NoCopies,
                         CalculatePrintJobPageTotalTest,
                         testing::Values(PrintJobPageTotalParams{
                             .pages = 1,
                             .copies = 0,
                             .expected_total = 1}));

INSTANTIATE_TEST_SUITE_P(WithCopies,
                         CalculatePrintJobPageTotalTest,
                         testing::Values(PrintJobPageTotalParams{
                             .pages = 2,
                             .copies = 5,
                             .expected_total = 10}));

// Verify correct total number of pages generated based on incoming
// `::printing::PrintedDocument`.
TEST_P(CalculatePrintJobPageTotalTest, CalculatesPrintJobPageTotal) {
  const PrintJobPageTotalParams& param = GetParam();
  auto query =
      ::printing::PrinterQuery::Create(content::GlobalRenderFrameHostId());
  auto settings = std::make_unique<::printing::PrintSettings>();
  settings->set_copies(param.copies);
  auto new_doc = base::MakeRefCounted<::printing::PrintedDocument>(
      std::move(settings), u"fake.pdf", query->cookie());
  new_doc->set_page_count(param.pages);

  // Get total pages for document.
  int total_pages = CalculatePrintJobTotalPages(new_doc.get());

  // Verify totals match.
  ASSERT_EQ(param.expected_total, total_pages);
}

}  // namespace
}  // namespace ash
