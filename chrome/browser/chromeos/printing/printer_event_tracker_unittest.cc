// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/printer_event_tracker.h"

#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "chromeos/printing/printer_configuration.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/printer_event.pb.h"

namespace chromeos {
namespace {

constexpr int kVendorId = 0x3241;
constexpr int kProductId = 0x1337;
constexpr char kUsbManufacturer[] = "Usb MakesPrinters";
constexpr char kUsbModel[] = "Printer ModelName";

constexpr char kMakeAndModel[] = "Chromium RazLazer X4321er";
constexpr char kEffectiveMakeAndModel[] = "Generic PostScript";

constexpr char kEmptyNetworkAutomaticSetupSourceMetric[] =
    "Printing.CUPS.EmptyNetworkAutomaticSetupEventSource";
constexpr char kEmptyUsbAutomaticSetupSourceMetric[] =
    "Printing.CUPS.EmptyUsbAutomaticSetupEventSource";

class PrinterEventTrackerTest : public testing::Test {
 public:
  PrinterEventTrackerTest() = default;
  ~PrinterEventTrackerTest() override = default;

 protected:
  PrinterEventTracker tracker_;

  // Returns a copy of the first element recorded by the tracker.  Calls Flush
  // on the |tracker_|.
  std::vector<metrics::PrinterEventProto> GetEvents() {
    std::vector<metrics::PrinterEventProto> events;
    tracker_.FlushPrinterEvents(&events);
    return events;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PrinterEventTrackerTest);
};

TEST_F(PrinterEventTrackerTest, RecordsWhenEnabled) {
  tracker_.set_logging(true);

  Printer test_printer;
  test_printer.set_make_and_model(kMakeAndModel);
  test_printer.mutable_ppd_reference()->effective_make_and_model =
      kEffectiveMakeAndModel;

  tracker_.RecordIppPrinterInstalled(test_printer, PrinterEventTracker::kUser,
                                     PrinterSetupSource::kSettings);

  auto events = GetEvents();
  EXPECT_EQ(1U, events.size());
}

TEST_F(PrinterEventTrackerTest, DefaultLoggingOff) {
  Printer test_printer;
  test_printer.set_make_and_model(kMakeAndModel);
  test_printer.mutable_ppd_reference()->effective_make_and_model =
      kEffectiveMakeAndModel;

  tracker_.RecordIppPrinterInstalled(test_printer,
                                     PrinterEventTracker::kAutomatic,
                                     PrinterSetupSource::kSettings);

  auto events = GetEvents();
  EXPECT_TRUE(events.empty());
}

TEST_F(PrinterEventTrackerTest, DoesNotRecordWhileDisabled) {
  tracker_.set_logging(false);

  Printer test_printer;
  test_printer.set_make_and_model(kMakeAndModel);
  test_printer.mutable_ppd_reference()->effective_make_and_model =
      kEffectiveMakeAndModel;

  tracker_.RecordIppPrinterInstalled(test_printer,
                                     PrinterEventTracker::kAutomatic,
                                     PrinterSetupSource::kSettings);

  auto events = GetEvents();
  EXPECT_TRUE(events.empty());
}

TEST_F(PrinterEventTrackerTest, InstalledIppPrinter) {
  tracker_.set_logging(true);

  Printer test_printer;
  test_printer.set_make_and_model(kMakeAndModel);
  test_printer.mutable_ppd_reference()->effective_make_and_model =
      kEffectiveMakeAndModel;

  tracker_.RecordIppPrinterInstalled(test_printer, PrinterEventTracker::kUser,
                                     PrinterSetupSource::kSettings);

  auto events = GetEvents();
  ASSERT_FALSE(events.empty());
  metrics::PrinterEventProto recorded_event = events.front();
  EXPECT_EQ(metrics::PrinterEventProto::SETUP_MANUAL,
            recorded_event.event_type());
  EXPECT_EQ(kMakeAndModel, recorded_event.ipp_make_and_model());
  EXPECT_EQ(kEffectiveMakeAndModel, recorded_event.ppd_identifier());

  EXPECT_FALSE(recorded_event.has_usb_printer_manufacturer());
  EXPECT_FALSE(recorded_event.has_usb_printer_model());
  EXPECT_FALSE(recorded_event.has_usb_vendor_id());
  EXPECT_FALSE(recorded_event.has_usb_model_id());
  EXPECT_FALSE(recorded_event.user_ppd());
}

TEST_F(PrinterEventTrackerTest, InstalledPrinterAuto) {
  tracker_.set_logging(true);

  Printer test_printer;
  test_printer.set_make_and_model(kMakeAndModel);
  test_printer.mutable_ppd_reference()->autoconf = true;

  tracker_.RecordIppPrinterInstalled(test_printer,
                                     PrinterEventTracker::SetupMode::kAutomatic,
                                     PrinterSetupSource::kSettings);

  auto events = GetEvents();
  ASSERT_FALSE(events.empty());

  metrics::PrinterEventProto recorded_event = events.front();
  EXPECT_EQ(metrics::PrinterEventProto::SETUP_AUTOMATIC,
            recorded_event.event_type());
  EXPECT_EQ(kMakeAndModel, recorded_event.ipp_make_and_model());
  // For autoconf printers, ppd identifier is blank but a successful setup is
  // recorded.
  EXPECT_FALSE(recorded_event.has_ppd_identifier());

  EXPECT_FALSE(recorded_event.has_usb_printer_manufacturer());
  EXPECT_FALSE(recorded_event.has_usb_printer_model());
  EXPECT_FALSE(recorded_event.has_usb_vendor_id());
  EXPECT_FALSE(recorded_event.has_usb_model_id());
  EXPECT_FALSE(recorded_event.user_ppd());
}

TEST_F(PrinterEventTrackerTest, InstalledPrinterUserPpd) {
  tracker_.set_logging(true);

  Printer test_printer;
  test_printer.mutable_ppd_reference()->user_supplied_ppd_url =
      "file:///i_dont_record_this_field/blah/blah/blah/some_ppd.ppd";

  tracker_.RecordIppPrinterInstalled(test_printer,
                                     PrinterEventTracker::SetupMode::kUser,
                                     PrinterSetupSource::kSettings);

  auto events = GetEvents();
  ASSERT_FALSE(events.empty());

  metrics::PrinterEventProto recorded_event = events.front();
  EXPECT_EQ(metrics::PrinterEventProto::SETUP_MANUAL,
            recorded_event.event_type());

  // For user PPDs we just record that it was a user PPD, the value is not
  // recorded.
  EXPECT_TRUE(recorded_event.user_ppd());
  EXPECT_FALSE(recorded_event.has_ppd_identifier());

  // This is empty if it was not detected.
  EXPECT_FALSE(recorded_event.has_ipp_make_and_model());

  // Network printers do not have usb information.
  EXPECT_FALSE(recorded_event.has_usb_printer_manufacturer());
  EXPECT_FALSE(recorded_event.has_usb_printer_model());
  EXPECT_FALSE(recorded_event.has_usb_vendor_id());
  EXPECT_FALSE(recorded_event.has_usb_model_id());
}

TEST_F(PrinterEventTrackerTest, InstalledUsbPrinter) {
  tracker_.set_logging(true);
  PrinterDetector::DetectedPrinter usb_printer;
  usb_printer.ppd_search_data.usb_vendor_id = kVendorId;
  usb_printer.ppd_search_data.usb_product_id = kProductId;
  usb_printer.printer.set_manufacturer(kUsbManufacturer);
  usb_printer.printer.set_model(kUsbModel);
  usb_printer.printer.mutable_ppd_reference()->effective_make_and_model =
      kEffectiveMakeAndModel;

  tracker_.RecordUsbPrinterInstalled(usb_printer,
                                     PrinterEventTracker::SetupMode::kUser,
                                     PrinterSetupSource::kSettings);

  auto events = GetEvents();
  ASSERT_FALSE(events.empty());

  metrics::PrinterEventProto record = events.front();
  EXPECT_EQ(metrics::PrinterEventProto::SETUP_MANUAL, record.event_type());
  EXPECT_EQ(kVendorId, record.usb_vendor_id());
  EXPECT_EQ(kProductId, record.usb_model_id());
  EXPECT_EQ(kUsbManufacturer, record.usb_printer_manufacturer());
  EXPECT_EQ(kUsbModel, record.usb_printer_model());

  EXPECT_EQ(kEffectiveMakeAndModel, record.ppd_identifier());
  EXPECT_FALSE(record.user_ppd());

  // USB doesn't detect this field.
  EXPECT_FALSE(record.has_ipp_make_and_model());
}

TEST_F(PrinterEventTrackerTest, AbandonedNetworkPrinter) {
  tracker_.set_logging(true);

  Printer test_printer;
  test_printer.set_make_and_model(kMakeAndModel);

  tracker_.RecordSetupAbandoned(test_printer);

  auto events = GetEvents();
  ASSERT_FALSE(events.empty());

  metrics::PrinterEventProto recorded_event = events.front();
  EXPECT_EQ(metrics::PrinterEventProto::SETUP_ABANDONED,
            recorded_event.event_type());
  EXPECT_EQ(kMakeAndModel, recorded_event.ipp_make_and_model());

  // Abandoned setups should not record a chosen PPD or user PPD.
  EXPECT_FALSE(recorded_event.has_user_ppd());
  EXPECT_FALSE(recorded_event.has_ppd_identifier());

  EXPECT_FALSE(recorded_event.has_usb_printer_manufacturer());
  EXPECT_FALSE(recorded_event.has_usb_printer_model());
  EXPECT_FALSE(recorded_event.has_usb_vendor_id());
  EXPECT_FALSE(recorded_event.has_usb_model_id());
}

TEST_F(PrinterEventTrackerTest, AbandonedUsbPrinter) {
  tracker_.set_logging(true);

  PrinterDetector::DetectedPrinter usb_printer;
  usb_printer.ppd_search_data.usb_vendor_id = kVendorId;
  usb_printer.ppd_search_data.usb_product_id = kProductId;
  usb_printer.printer.set_manufacturer(kUsbManufacturer);
  usb_printer.printer.set_model(kUsbModel);

  tracker_.RecordUsbSetupAbandoned(usb_printer);

  auto events = GetEvents();
  ASSERT_FALSE(events.empty());

  metrics::PrinterEventProto record = events.front();
  EXPECT_EQ(metrics::PrinterEventProto::SETUP_ABANDONED, record.event_type());
  EXPECT_EQ(kVendorId, record.usb_vendor_id());
  EXPECT_EQ(kProductId, record.usb_model_id());
  EXPECT_EQ(kUsbManufacturer, record.usb_printer_manufacturer());
  EXPECT_EQ(kUsbModel, record.usb_printer_model());

  EXPECT_FALSE(record.has_user_ppd());
  EXPECT_FALSE(record.has_ppd_identifier());
}

TEST_F(PrinterEventTrackerTest, RemovedPrinter) {
  tracker_.set_logging(true);

  Printer test_printer;
  test_printer.set_make_and_model(kMakeAndModel);
  test_printer.mutable_ppd_reference()->effective_make_and_model =
      kEffectiveMakeAndModel;

  tracker_.RecordPrinterRemoved(test_printer);

  auto events = GetEvents();
  ASSERT_FALSE(events.empty());

  metrics::PrinterEventProto recorded_event = events.front();
  EXPECT_EQ(metrics::PrinterEventProto::PRINTER_DELETED,
            recorded_event.event_type());
  // All printers record make and model information here.
  EXPECT_EQ(kMakeAndModel, recorded_event.ipp_make_and_model());

  // PPD info.
  EXPECT_EQ(kEffectiveMakeAndModel, recorded_event.ppd_identifier());
  EXPECT_FALSE(recorded_event.user_ppd());

  // USB Info is not retained for removed printers.
  EXPECT_FALSE(recorded_event.has_usb_printer_manufacturer());
  EXPECT_FALSE(recorded_event.has_usb_printer_model());
  EXPECT_FALSE(recorded_event.has_usb_vendor_id());
  EXPECT_FALSE(recorded_event.has_usb_model_id());
}

// Tests that when a network printer is automatically installed that does not
// have any populated make and model values, that the appropriate UMA histogram
// will be updated.
TEST_F(PrinterEventTrackerTest, LogEmptyIppAutomaticSetupEvents) {
  tracker_.set_logging(true);
  base::HistogramTester histograms;

  // Create a printer which has no fields populated except for the ppd
  // identifier.
  Printer printer;
  printer.mutable_ppd_reference()->effective_make_and_model = "test_ppd";

  tracker_.RecordIppPrinterInstalled(printer, PrinterEventTracker::kAutomatic,
                                     PrinterSetupSource::kSettings);

  histograms.ExpectBucketCount(kEmptyNetworkAutomaticSetupSourceMetric,
                               PrinterSetupSource::kSettings, 1);
}

// Tests that when a USB printer is automatically installed that does not have
// any populated make and model values, that the appropriate UMA histogram will
// be updated.
TEST_F(PrinterEventTrackerTest, LogEmptyUsbAutomaticSetupEvents) {
  tracker_.set_logging(true);
  base::HistogramTester histograms;

  // Create a detected printer which has no fields populated except for the ppd
  // identifier.
  PrinterDetector::DetectedPrinter usb_printer;
  usb_printer.printer.mutable_ppd_reference()->effective_make_and_model =
      kEffectiveMakeAndModel;

  tracker_.RecordUsbPrinterInstalled(usb_printer,
                                     PrinterEventTracker::kAutomatic,
                                     PrinterSetupSource::kPrintPreview);

  histograms.ExpectBucketCount(kEmptyUsbAutomaticSetupSourceMetric,
                               PrinterSetupSource::kPrintPreview, 1);
}

// Test that when a non-empty network printer is installed automatically, that
// we don't log any UMA metrics for empty setup events.
TEST_F(PrinterEventTrackerTest, SkipLoggingValidNetworkInstallations) {
  tracker_.set_logging(true);
  base::HistogramTester histograms;

  // The histogram object isn't actually created until at least one enumeration
  // has been logged. So we log an event for print preview, and then expect that
  // after installing a valid printer that the histogram has not been changed.
  base::UmaHistogramEnumeration(kEmptyNetworkAutomaticSetupSourceMetric,
                                PrinterSetupSource::kPrintPreview);

  // Create a printer which has a value set for the make_and_model field.
  Printer printer;
  printer.set_make_and_model("make_and_model");
  printer.mutable_ppd_reference()->effective_make_and_model = "test_ppd";

  tracker_.RecordIppPrinterInstalled(printer, PrinterEventTracker::kAutomatic,
                                     PrinterSetupSource::kArcPrintService);

  // Since |printer| is not an "empty" setup event, we expect that the UMA
  // histogram has not been updated.
  histograms.ExpectBucketCount(kEmptyNetworkAutomaticSetupSourceMetric,
                               PrinterSetupSource::kPrintPreview, 1);
}

// Test that when a non-empty network printer is installed automatically, that
// we don't log any UMA metrics for empty setup events.
TEST_F(PrinterEventTrackerTest, SkipLoggingValidUsbInstallations) {
  tracker_.set_logging(true);
  base::HistogramTester histograms;

  // The histogram object isn't actually created until at least one enumeration
  // has been logged. So we log an event for print preview, and then expect that
  // after installing a valid printer that the histogram has not been changed.
  base::UmaHistogramEnumeration(kEmptyUsbAutomaticSetupSourceMetric,
                                PrinterSetupSource::kPrintPreview);

  // Create a detected printer which has fields set for the
  // usb_printer_manufacturer and usb_printer_model fields.
  PrinterDetector::DetectedPrinter usb_printer;
  usb_printer.ppd_search_data.usb_vendor_id = 1;
  usb_printer.ppd_search_data.usb_product_id = 1;
  usb_printer.printer.set_manufacturer("manufacturer");
  usb_printer.printer.set_model("model");
  usb_printer.printer.mutable_ppd_reference()->effective_make_and_model =
      kEffectiveMakeAndModel;

  tracker_.RecordUsbPrinterInstalled(usb_printer,
                                     PrinterEventTracker::kAutomatic,
                                     PrinterSetupSource::kPrintPreview);

  // Since |usb_printer| is not an "empty" setup event, we expect that the UMA
  // histogram has not been updated.
  histograms.ExpectBucketCount(kEmptyUsbAutomaticSetupSourceMetric,
                               PrinterSetupSource::kPrintPreview, 1);
}

}  // namespace
}  // namespace chromeos
