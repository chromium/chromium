// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/printer_event_tracker.h"

#include "base/time/time.h"
#include "chromeos/printing/printer_configuration.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/printer_event.pb.h"

namespace ash {
namespace {

using ::chromeos::IppPrinterInfo;
using ::chromeos::Printer;

constexpr int kVendorId = 0x3241;
constexpr int kProductId = 0x1337;
constexpr char kUsbManufacturer[] = "Usb MakesPrinters";
constexpr char kUsbModel[] = "Printer ModelName";

constexpr char kMakeAndModel[] = "Chromium RazLazer X4321er";
constexpr char kEffectiveMakeAndModel[] = "Generic PostScript";

constexpr char kDocumentFormatDefault[] = "Default";
constexpr char kDocumentFormatPreferred[] = "Preferred";
constexpr char kFirstDocumentFormatSupported[] = "First";
constexpr char kSecondDocumentFormatSupported[] = "Second";
constexpr char kFirstUrfSupported[] = "IS4-20";
constexpr char kSecondUrfSupported[] = "W8";
constexpr char kFirstIppFeatureString[] = "adf";
constexpr char kSecondIppFeatureString[] = "platen";
constexpr metrics::PrinterEventProto::IppFeature kFirstIppFeatureSupported =
    metrics::PrinterEventProto::ADF;
constexpr metrics::PrinterEventProto::IppFeature kSecondIppFeatureSupported =
    metrics::PrinterEventProto::PLATEN;
constexpr char kFirstPdfVersionString[] = "iso-32000-1_2008";
constexpr char kSecondPdfVersionString[] = "adobe-1.4";
constexpr metrics::PrinterEventProto::PdfVersion kFirstPdfVersionSupported =
    metrics::PrinterEventProto::ISO_32000_1_2008;
constexpr metrics::PrinterEventProto::PdfVersion kSecondPdfVersionSupported =
    metrics::PrinterEventProto::ADOBE_1_4;
constexpr char kMopriaCertified[] = "1.3";
constexpr char kFirstPrinterKind[] = "document";
constexpr char kSecondPrinterKind[] = "photo";

class PrinterEventTrackerTest : public testing::Test {
 public:
  PrinterEventTrackerTest() = default;

  PrinterEventTrackerTest(const PrinterEventTrackerTest&) = delete;
  PrinterEventTrackerTest& operator=(const PrinterEventTrackerTest&) = delete;

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
};

TEST_F(PrinterEventTrackerTest, RecordsWhenEnabled) {
  tracker_.set_logging(true);

  Printer test_printer;
  test_printer.set_make_and_model(kMakeAndModel);
  test_printer.mutable_ppd_reference()->effective_make_and_model =
      kEffectiveMakeAndModel;

  tracker_.RecordIppPrinterInstalled(test_printer, PrinterEventTracker::kUser,
                                     chromeos::IppPrinterInfo{});

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
                                     chromeos::IppPrinterInfo{});

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
                                     chromeos::IppPrinterInfo{});

  auto events = GetEvents();
  EXPECT_TRUE(events.empty());
}

TEST_F(PrinterEventTrackerTest, InstalledIppPrinter) {
  tracker_.set_logging(true);

  Printer test_printer;
  test_printer.set_make_and_model(kMakeAndModel);
  test_printer.mutable_ppd_reference()->effective_make_and_model =
      kEffectiveMakeAndModel;

  IppPrinterInfo ipp_printer_info;
  ipp_printer_info.document_format_default = kDocumentFormatDefault;
  ipp_printer_info.document_format_preferred = kDocumentFormatPreferred;
  ipp_printer_info.document_formats.push_back(kFirstDocumentFormatSupported);
  ipp_printer_info.document_formats.push_back(kSecondDocumentFormatSupported);
  ipp_printer_info.document_formats.push_back(kDocumentFormatDefault);
  ipp_printer_info.document_formats.push_back(kDocumentFormatPreferred);
  ipp_printer_info.urf_supported.push_back(kFirstUrfSupported);
  ipp_printer_info.urf_supported.push_back(kSecondUrfSupported);
  ipp_printer_info.pdf_versions.push_back(kFirstPdfVersionString);
  ipp_printer_info.pdf_versions.push_back(kSecondPdfVersionString);
  ipp_printer_info.ipp_features.push_back(kFirstIppFeatureString);
  ipp_printer_info.ipp_features.push_back(kSecondIppFeatureString);
  ipp_printer_info.mopria_certified = kMopriaCertified;
  ipp_printer_info.printer_kind.push_back(kFirstPrinterKind);
  ipp_printer_info.printer_kind.push_back(kSecondPrinterKind);

  tracker_.RecordIppPrinterInstalled(test_printer, PrinterEventTracker::kUser,
                                     ipp_printer_info);

  auto events = GetEvents();
  ASSERT_FALSE(events.empty());
  metrics::PrinterEventProto recorded_event = events.front();
  EXPECT_EQ(metrics::PrinterEventProto::SETUP_MANUAL,
            recorded_event.event_type());
  EXPECT_EQ(kMakeAndModel, recorded_event.ipp_make_and_model());
  EXPECT_EQ(kEffectiveMakeAndModel, recorded_event.ppd_identifier());

  EXPECT_EQ(kDocumentFormatDefault, recorded_event.document_format_default());
  EXPECT_EQ(kDocumentFormatPreferred,
            recorded_event.document_format_preferred());
  EXPECT_THAT(
      recorded_event.document_format_supported(),
      testing::UnorderedElementsAreArray(
          {kFirstDocumentFormatSupported, kSecondDocumentFormatSupported,
           kDocumentFormatDefault, kDocumentFormatPreferred}));
  EXPECT_THAT(recorded_event.urf_supported(),
              testing::UnorderedElementsAreArray(
                  {kFirstUrfSupported, kSecondUrfSupported}));
  EXPECT_THAT(recorded_event.pdf_versions_supported(),
              testing::UnorderedElementsAreArray(
                  {kFirstPdfVersionSupported, kSecondPdfVersionSupported}));
  EXPECT_THAT(recorded_event.ipp_features_supported(),
              testing::UnorderedElementsAreArray(
                  {kFirstIppFeatureSupported, kSecondIppFeatureSupported}));
  EXPECT_EQ(recorded_event.mopria_certified(), kMopriaCertified);
  EXPECT_THAT(recorded_event.printer_kind(),
              testing::UnorderedElementsAreArray(
                  {kFirstPrinterKind, kSecondPrinterKind}));

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

  IppPrinterInfo ipp_printer_info;
  ipp_printer_info.document_format_default = kDocumentFormatDefault;
  ipp_printer_info.document_format_preferred = kDocumentFormatPreferred;
  ipp_printer_info.document_formats.push_back(kFirstDocumentFormatSupported);
  ipp_printer_info.document_formats.push_back(kSecondDocumentFormatSupported);
  ipp_printer_info.document_formats.push_back(kDocumentFormatDefault);
  ipp_printer_info.document_formats.push_back(kDocumentFormatPreferred);
  ipp_printer_info.urf_supported.push_back(kFirstUrfSupported);
  ipp_printer_info.urf_supported.push_back(kSecondUrfSupported);
  ipp_printer_info.pdf_versions.push_back(kFirstPdfVersionString);
  ipp_printer_info.pdf_versions.push_back(kSecondPdfVersionString);
  ipp_printer_info.ipp_features.push_back(kFirstIppFeatureString);
  ipp_printer_info.ipp_features.push_back(kSecondIppFeatureString);
  ipp_printer_info.mopria_certified = kMopriaCertified;
  ipp_printer_info.printer_kind.push_back(kFirstPrinterKind);
  ipp_printer_info.printer_kind.push_back(kSecondPrinterKind);

  tracker_.RecordIppPrinterInstalled(
      test_printer, PrinterEventTracker::kAutomatic, ipp_printer_info);

  auto events = GetEvents();
  ASSERT_FALSE(events.empty());

  metrics::PrinterEventProto recorded_event = events.front();
  EXPECT_EQ(metrics::PrinterEventProto::SETUP_AUTOMATIC,
            recorded_event.event_type());
  EXPECT_EQ(kMakeAndModel, recorded_event.ipp_make_and_model());

  EXPECT_EQ(kDocumentFormatDefault, recorded_event.document_format_default());
  EXPECT_EQ(kDocumentFormatPreferred,
            recorded_event.document_format_preferred());
  EXPECT_THAT(
      recorded_event.document_format_supported(),
      testing::UnorderedElementsAreArray(
          {kFirstDocumentFormatSupported, kSecondDocumentFormatSupported,
           kDocumentFormatDefault, kDocumentFormatPreferred}));
  EXPECT_THAT(recorded_event.urf_supported(),
              testing::UnorderedElementsAreArray(
                  {kFirstUrfSupported, kSecondUrfSupported}));
  EXPECT_THAT(recorded_event.pdf_versions_supported(),
              testing::UnorderedElementsAreArray(
                  {kFirstPdfVersionSupported, kSecondPdfVersionSupported}));
  EXPECT_THAT(recorded_event.ipp_features_supported(),
              testing::UnorderedElementsAreArray(
                  {kFirstIppFeatureSupported, kSecondIppFeatureSupported}));
  EXPECT_EQ(recorded_event.mopria_certified(), kMopriaCertified);
  EXPECT_THAT(recorded_event.printer_kind(),
              testing::UnorderedElementsAreArray(
                  {kFirstPrinterKind, kSecondPrinterKind}));

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

  tracker_.RecordIppPrinterInstalled(test_printer, PrinterEventTracker::kUser,
                                     chromeos::IppPrinterInfo{});

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
  usb_printer.ppd_search_data.usb_manufacturer = kUsbManufacturer;
  usb_printer.ppd_search_data.usb_model = kUsbModel;
  usb_printer.printer.mutable_ppd_reference()->effective_make_and_model =
      kEffectiveMakeAndModel;

  tracker_.RecordUsbPrinterInstalled(usb_printer,
                                     PrinterEventTracker::SetupMode::kUser);

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
  usb_printer.ppd_search_data.usb_manufacturer = kUsbManufacturer;
  usb_printer.ppd_search_data.usb_model = kUsbModel;

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

}  // namespace
}  // namespace ash
