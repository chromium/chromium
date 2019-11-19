// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/history/print_job_info_proto_conversions.h"

#include "base/time/time_override.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace proto = printing::proto;

namespace {

constexpr int kWidth = 297000;
constexpr int kHeight = 420000;
constexpr char kVendorId[] = "iso_a3_297x420mm";

constexpr char kName[] = "name";
constexpr char kUri[] = "ipp://192.168.1.5";

constexpr char kTitle[] = "title";
constexpr char kId[] = "id";
constexpr char kSourceId[] = "extension:123";
constexpr int64_t kJobCreationTime = 1000;
constexpr int64_t kJobDuration = 10 * 1000;
constexpr int kPagesNumber = 3;

}  // namespace

TEST(PrintJobInfoProtoConversionsTest, PrintSettingsToProto) {
  ::printing::PrintSettings settings;
  settings.set_color(::printing::ColorModel::COLOR);
  settings.set_duplex_mode(::printing::DuplexMode::LONG_EDGE);
  ::printing::PrintSettings::RequestedMedia media;
  media.size_microns = gfx::Size(kWidth, kHeight);
  media.vendor_id = kVendorId;
  settings.set_requested_media(media);
  settings.set_copies(2);

  proto::PrintSettings settings_proto = PrintSettingsToProto(settings);
  const proto::MediaSize& media_size = settings_proto.media_size();

  EXPECT_EQ(proto::PrintSettings_ColorMode_COLOR, settings_proto.color());
  EXPECT_EQ(proto::PrintSettings_DuplexMode_TWO_SIDED_LONG_EDGE,
            settings_proto.duplex());
  EXPECT_EQ(kWidth, media_size.width());
  EXPECT_EQ(kHeight, media_size.height());
  EXPECT_EQ(kVendorId, media_size.vendor_id());
  EXPECT_EQ(2, settings_proto.copies());
}

TEST(PrintJobInfoProtoConversionsTest, CupsPrintJobToProto) {
  // Override time so that base::Time::Now() always returns 1 second after the
  // epoch in Unix-like system (Jan 1, 1970).
  base::subtle::ScopedTimeClockOverrides time_override(
      []() {
        return base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(1);
      },
      nullptr, nullptr);

  chromeos::Printer printer;
  printer.set_display_name(kName);
  printer.set_uri(kUri);
  printer.set_source(chromeos::Printer::Source::SRC_POLICY);

  proto::PrintSettings settings;
  settings.set_color(proto::PrintSettings_ColorMode_COLOR);

  // CupsPrintJob computes the start time of the print job, that's why we have
  // to override base::Time::now() value for the test.
  CupsPrintJob cups_print_job(printer, /*job_id=*/0, kTitle, kPagesNumber,
                              ::printing::PrintJob::Source::PRINT_PREVIEW,
                              kSourceId, settings);
  cups_print_job.set_state(CupsPrintJob::State::STATE_FAILED);
  base::Time completion_time =
      base::Time::Now() + base::TimeDelta::FromSeconds(10);

  proto::PrintJobInfo print_job_info_proto =
      CupsPrintJobToProto(cups_print_job, kId, completion_time);
  const proto::Printer& printer_proto = print_job_info_proto.printer();

  EXPECT_EQ(kId, print_job_info_proto.id());
  EXPECT_EQ(kTitle, print_job_info_proto.title());
  EXPECT_EQ(proto::PrintJobInfo_PrintJobSource_PRINT_PREVIEW,
            print_job_info_proto.source());
  EXPECT_EQ(kSourceId, print_job_info_proto.source_id());
  EXPECT_EQ(proto::PrintJobInfo_PrintJobStatus_FAILED,
            print_job_info_proto.status());
  EXPECT_EQ(kJobCreationTime, print_job_info_proto.creation_time());
  EXPECT_EQ(kJobCreationTime + kJobDuration,
            print_job_info_proto.completion_time());
  EXPECT_EQ(kName, printer_proto.name());
  EXPECT_EQ(kUri, printer_proto.uri());
  EXPECT_EQ(proto::Printer_PrinterSource_POLICY, printer_proto.source());
  EXPECT_EQ(proto::PrintSettings_ColorMode_COLOR,
            print_job_info_proto.settings().color());
  EXPECT_EQ(kPagesNumber, print_job_info_proto.number_of_pages());
}

}  // namespace chromeos
