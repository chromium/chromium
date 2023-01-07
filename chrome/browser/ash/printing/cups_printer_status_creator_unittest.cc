// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/cups_printer_status_creator.h"

#include <vector>

#include "chromeos/printing/cups_printer_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using ::chromeos::CupsPrinterStatus;
using CupsReason = CupsPrinterStatus::CupsPrinterStatusReason::Reason;
using CupsSeverity = CupsPrinterStatus::CupsPrinterStatusReason::Severity;
using ReasonFromPrinter = printing::PrinterStatus::PrinterReason::Reason;
using SeverityFromPrinter = printing::PrinterStatus::PrinterReason::Severity;

TEST(CupsPrinterStatusCreatorTest, PrinterStatusToCupsPrinterStatus) {
  printing::PrinterStatus::PrinterReason reason1;
  reason1.reason = ReasonFromPrinter::kNone;
  reason1.severity = SeverityFromPrinter::kReport;

  printing::PrinterStatus::PrinterReason reason2;
  reason2.reason = ReasonFromPrinter::kCoverOpen;
  reason2.severity = SeverityFromPrinter::kWarning;

  printing::PrinterStatus printer_status;
  printer_status.reasons.push_back(reason1);
  printer_status.reasons.push_back(reason2);

  std::string printer_id = "id";
  chromeos::PrinterAuthenticationInfo auth_info{.oauth_server = "a",
                                                .oauth_scope = "b"};
  CupsPrinterStatus cups_printer_status =
      PrinterStatusToCupsPrinterStatus(printer_id, printer_status, auth_info);

  EXPECT_EQ("id", cups_printer_status.GetPrinterId());
  EXPECT_EQ(2u, cups_printer_status.GetStatusReasons().size());

  std::vector<CupsPrinterStatus::CupsPrinterStatusReason> expected_reasons{
      CupsPrinterStatus::CupsPrinterStatusReason(CupsReason::kNoError,
                                                 CupsSeverity::kReport),
      CupsPrinterStatus::CupsPrinterStatusReason(CupsReason::kDoorOpen,
                                                 CupsSeverity::kWarning)};
  EXPECT_THAT(cups_printer_status.GetStatusReasons(), expected_reasons);

  EXPECT_EQ(cups_printer_status.GetAuthenticationInfo().oauth_server, "a");
  EXPECT_EQ(cups_printer_status.GetAuthenticationInfo().oauth_scope, "b");
}

TEST(CupsPrinterStatusCreatorTest, PrinterSeverityToCupsSeverity) {
  EXPECT_EQ(
      CupsSeverity::kUnknownSeverity,
      PrinterSeverityToCupsSeverity(SeverityFromPrinter::kUnknownSeverity));
  EXPECT_EQ(CupsSeverity::kReport,
            PrinterSeverityToCupsSeverity(SeverityFromPrinter::kReport));
  EXPECT_EQ(CupsSeverity::kWarning,
            PrinterSeverityToCupsSeverity(SeverityFromPrinter::kWarning));
  EXPECT_EQ(CupsSeverity::kError,
            PrinterSeverityToCupsSeverity(SeverityFromPrinter::kError));
}

TEST(CupsPrinterStatusCreatorTest, PrinterReasonToCupsReason) {
  EXPECT_EQ(CupsReason::kDeviceError,
            PrinterReasonToCupsReason(ReasonFromPrinter::kFuserOverTemp));
  EXPECT_EQ(CupsReason::kDeviceError,
            PrinterReasonToCupsReason(ReasonFromPrinter::kFuserUnderTemp));
  EXPECT_EQ(CupsReason::kDeviceError,
            PrinterReasonToCupsReason(
                ReasonFromPrinter::kInterpreterResourceUnavailable));
  EXPECT_EQ(CupsReason::kDeviceError,
            PrinterReasonToCupsReason(ReasonFromPrinter::kOpcLifeOver));
  EXPECT_EQ(CupsReason::kDeviceError,
            PrinterReasonToCupsReason(ReasonFromPrinter::kOpcNearEol));

  EXPECT_EQ(CupsReason::kDoorOpen,
            PrinterReasonToCupsReason(ReasonFromPrinter::kCoverOpen));
  EXPECT_EQ(CupsReason::kDoorOpen,
            PrinterReasonToCupsReason(ReasonFromPrinter::kDoorOpen));
  EXPECT_EQ(CupsReason::kDoorOpen,
            PrinterReasonToCupsReason(ReasonFromPrinter::kInterlockOpen));

  EXPECT_EQ(CupsReason::kLowOnInk,
            PrinterReasonToCupsReason(ReasonFromPrinter::kDeveloperLow));
  EXPECT_EQ(CupsReason::kLowOnInk,
            PrinterReasonToCupsReason(ReasonFromPrinter::kMarkerSupplyLow));
  EXPECT_EQ(
      CupsReason::kLowOnInk,
      PrinterReasonToCupsReason(ReasonFromPrinter::kMarkerWasteAlmostFull));
  EXPECT_EQ(CupsReason::kLowOnInk,
            PrinterReasonToCupsReason(ReasonFromPrinter::kTonerLow));

  EXPECT_EQ(CupsReason::kLowOnPaper,
            PrinterReasonToCupsReason(ReasonFromPrinter::kMediaLow));

  EXPECT_EQ(CupsReason::kNoError,
            PrinterReasonToCupsReason(ReasonFromPrinter::kNone));

  EXPECT_EQ(CupsReason::kOutOfInk,
            PrinterReasonToCupsReason(ReasonFromPrinter::kDeveloperEmpty));
  EXPECT_EQ(CupsReason::kOutOfInk,
            PrinterReasonToCupsReason(ReasonFromPrinter::kMarkerSupplyEmpty));
  EXPECT_EQ(CupsReason::kOutOfInk,
            PrinterReasonToCupsReason(ReasonFromPrinter::kMarkerWasteFull));
  EXPECT_EQ(CupsReason::kOutOfInk,
            PrinterReasonToCupsReason(ReasonFromPrinter::kTonerEmpty));

  EXPECT_EQ(CupsReason::kOutOfPaper,
            PrinterReasonToCupsReason(ReasonFromPrinter::kMediaEmpty));
  EXPECT_EQ(CupsReason::kOutOfPaper,
            PrinterReasonToCupsReason(ReasonFromPrinter::kMediaNeeded));

  EXPECT_EQ(
      CupsReason::kOutputAreaAlmostFull,
      PrinterReasonToCupsReason(ReasonFromPrinter::kOutputAreaAlmostFull));

  EXPECT_EQ(CupsReason::kOutputFull,
            PrinterReasonToCupsReason(ReasonFromPrinter::kOutputAreaFull));

  EXPECT_EQ(CupsReason::kPaperJam,
            PrinterReasonToCupsReason(ReasonFromPrinter::kMediaJam));

  EXPECT_EQ(CupsReason::kPaused,
            PrinterReasonToCupsReason(ReasonFromPrinter::kMovingToPaused));
  EXPECT_EQ(CupsReason::kPaused,
            PrinterReasonToCupsReason(ReasonFromPrinter::kPaused));

  EXPECT_EQ(CupsReason::kPrinterQueueFull,
            PrinterReasonToCupsReason(ReasonFromPrinter::kSpoolAreaFull));

  EXPECT_EQ(CupsReason::kPrinterUnreachable,
            PrinterReasonToCupsReason(ReasonFromPrinter::kConnectingToDevice));
  EXPECT_EQ(CupsReason::kPrinterUnreachable,
            PrinterReasonToCupsReason(ReasonFromPrinter::kShutdown));
  EXPECT_EQ(CupsReason::kPrinterUnreachable,
            PrinterReasonToCupsReason(ReasonFromPrinter::kTimedOut));

  EXPECT_EQ(CupsReason::kStopped,
            PrinterReasonToCupsReason(ReasonFromPrinter::kStoppedPartly));
  EXPECT_EQ(CupsReason::kStopped,
            PrinterReasonToCupsReason(ReasonFromPrinter::kStopping));

  EXPECT_EQ(CupsReason::kTrayMissing,
            PrinterReasonToCupsReason(ReasonFromPrinter::kInputTrayMissing));
  EXPECT_EQ(CupsReason::kTrayMissing,
            PrinterReasonToCupsReason(ReasonFromPrinter::kOutputTrayMissing));

  EXPECT_EQ(CupsReason::kUnknownReason,
            PrinterReasonToCupsReason(ReasonFromPrinter::kUnknownReason));
}

}  // namespace ash
