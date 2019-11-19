// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/printing_metrics/print_job_info_idl_conversions.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace proto = chromeos::printing::proto;

namespace extensions {

namespace idl = api::printing_metrics;

namespace {

constexpr int kWidth = 297000;
constexpr int kHeight = 420000;
constexpr char kVendorId[] = "iso_a3_297x420mm";
constexpr int kCopies = 2;

constexpr char kName[] = "name";
constexpr char kUri[] = "ipp://192.168.1.5";

constexpr char kTitle[] = "title";
constexpr char kId[] = "id";
constexpr int64_t kJobCreationTime = 1000;
constexpr int64_t kJobCompletionTime = 11 * 1000;
constexpr int kPagesNumber = 3;

}  // namespace

TEST(PrintJobInfoIdlConversionsTest, PrintJobInfoProtoToIdl) {
  proto::PrintJobInfo print_job_info_proto;
  print_job_info_proto.set_id(kId);
  print_job_info_proto.set_title(kTitle);
  print_job_info_proto.set_source(
      proto::PrintJobInfo_PrintJobSource_PRINT_PREVIEW);
  print_job_info_proto.set_source_id("");
  print_job_info_proto.set_status(proto::PrintJobInfo_PrintJobStatus_FAILED);
  print_job_info_proto.set_creation_time(kJobCreationTime);
  print_job_info_proto.set_completion_time(kJobCompletionTime);

  proto::Printer printer_proto;
  printer_proto.set_name(kName);
  printer_proto.set_uri(kUri);
  printer_proto.set_source(proto::Printer_PrinterSource_POLICY);
  *print_job_info_proto.mutable_printer() = printer_proto;

  proto::MediaSize media_size_proto;
  media_size_proto.set_width(kWidth);
  media_size_proto.set_height(kHeight);
  media_size_proto.set_vendor_id(kVendorId);
  proto::PrintSettings settings_proto;
  settings_proto.set_color(proto::PrintSettings_ColorMode_COLOR);
  settings_proto.set_duplex(proto::PrintSettings_DuplexMode_ONE_SIDED);
  *settings_proto.mutable_media_size() = media_size_proto;
  settings_proto.set_copies(kCopies);
  *print_job_info_proto.mutable_settings() = settings_proto;

  print_job_info_proto.set_number_of_pages(kPagesNumber);

  idl::PrintJobInfo print_job_info =
      PrintJobInfoProtoToIdl(print_job_info_proto);
  const idl::Printer& printer = print_job_info.printer;
  const idl::PrintSettings& settings = print_job_info.settings;
  const idl::MediaSize& media_size = settings.media_size;

  EXPECT_EQ(kId, print_job_info.id);
  EXPECT_EQ(kTitle, print_job_info.title);
  EXPECT_EQ(idl::PRINT_JOB_SOURCE_PRINT_PREVIEW, print_job_info.source);
  EXPECT_EQ(nullptr, print_job_info.source_id);
  EXPECT_EQ(idl::PRINT_JOB_STATUS_FAILED, print_job_info.status);
  EXPECT_DOUBLE_EQ(static_cast<double>(kJobCreationTime),
                   print_job_info.creation_time);
  EXPECT_DOUBLE_EQ(static_cast<double>(kJobCompletionTime),
                   print_job_info.completion_time);
  EXPECT_EQ(kName, printer.name);
  EXPECT_EQ(kUri, printer.uri);
  EXPECT_EQ(idl::PRINTER_SOURCE_POLICY, printer.source);
  EXPECT_EQ(idl::COLOR_MODE_COLOR, settings.color);
  EXPECT_EQ(idl::DUPLEX_MODE_ONE_SIDED, settings.duplex);
  EXPECT_EQ(kCopies, settings.copies);
  EXPECT_EQ(kWidth, media_size.width);
  EXPECT_EQ(kHeight, media_size.height);
  EXPECT_EQ(kVendorId, media_size.vendor_id);
  EXPECT_EQ(kPagesNumber, print_job_info.number_of_pages);
}

}  // namespace extensions
