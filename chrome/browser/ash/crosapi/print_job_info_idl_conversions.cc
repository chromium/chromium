// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/print_job_info_idl_conversions.h"

#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/extensions/api/printing/printing_api.h"
#include "chrome/common/extensions/api/printing.h"

namespace proto = ::ash::printing::proto;

namespace extensions {

namespace idl = api::printing_metrics;

namespace {

idl::ColorMode ColorModeProtoToIdl(proto::PrintSettings_ColorMode color_proto) {
  switch (color_proto) {
    case proto::PrintSettings_ColorMode_BLACK_AND_WHITE:
      return idl::ColorMode::kBlackAndWhite;
    case proto::PrintSettings_ColorMode_COLOR:
      return idl::ColorMode::kColor;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return idl::ColorMode::kBlackAndWhite;
}

idl::DuplexMode DuplexModeProtoToIdl(
    proto::PrintSettings_DuplexMode duplex_proto) {
  switch (duplex_proto) {
    case proto::PrintSettings_DuplexMode_ONE_SIDED:
      return idl::DuplexMode::kOneSided;
    case proto::PrintSettings_DuplexMode_TWO_SIDED_LONG_EDGE:
      return idl::DuplexMode::kTwoSidedLongEdge;
    case proto::PrintSettings_DuplexMode_TWO_SIDED_SHORT_EDGE:
      return idl::DuplexMode::kTwoSidedShortEdge;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return idl::DuplexMode::kOneSided;
}

idl::MediaSize MediaSizeProtoToIdl(const proto::MediaSize& media_size_proto) {
  idl::MediaSize media_size;
  media_size.width = media_size_proto.width();
  media_size.height = media_size_proto.height();
  media_size.vendor_id = media_size_proto.vendor_id();
  return media_size;
}

idl::PrintSettings PrintSettingsProtoToIdl(
    const proto::PrintSettings& settings_proto) {
  idl::PrintSettings settings;
  settings.color = ColorModeProtoToIdl(settings_proto.color());
  settings.duplex = DuplexModeProtoToIdl(settings_proto.duplex());
  settings.media_size = MediaSizeProtoToIdl(settings_proto.media_size());
  settings.copies = settings_proto.copies();
  return settings;
}

idl::PrintJobSource PrintJobSourceProtoToIdl(
    proto::PrintJobInfo_PrintJobSource print_job_source_proto) {
  switch (print_job_source_proto) {
    case proto::PrintJobInfo_PrintJobSource_PRINT_PREVIEW:
    case proto::PrintJobInfo_PrintJobSource_PRINT_PREVIEW_INCOGNITO:
      return idl::PrintJobSource::kPrintPreview;
    case proto::PrintJobInfo_PrintJobSource_ARC:
      return idl::PrintJobSource::kAndroidApp;
    case proto::PrintJobInfo_PrintJobSource_EXTENSION:
      return idl::PrintJobSource::kExtension;
    case proto::PrintJobInfo_PrintJobSource_ISOLATED_WEB_APP:
      return idl::PrintJobSource::kIsolatedWebApp;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return idl::PrintJobSource::kPrintPreview;
}

idl::PrintJobStatus PrintJobStatusProtoToIdl(
    proto::PrintJobInfo_PrintJobStatus print_job_status_proto) {
  switch (print_job_status_proto) {
    case proto::PrintJobInfo_PrintJobStatus_FAILED:
      return idl::PrintJobStatus::kFailed;
    case proto::PrintJobInfo_PrintJobStatus_CANCELED:
      return idl::PrintJobStatus::kCanceled;
    case proto::PrintJobInfo_PrintJobStatus_PRINTED:
      return idl::PrintJobStatus::kPrinted;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return idl::PrintJobStatus::kFailed;
}

idl::PrinterSource PrinterSourceProtoToIdl(
    proto::Printer_PrinterSource printer_source_proto) {
  switch (printer_source_proto) {
    case proto::Printer_PrinterSource_USER:
      return idl::PrinterSource::kUser;
    case proto::Printer_PrinterSource_POLICY:
      return idl::PrinterSource::kPolicy;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return idl::PrinterSource::kUser;
}

idl::Printer PrinterProtoToIdl(const proto::Printer& printer_proto) {
  idl::Printer printer;
  printer.name = printer_proto.name();
  printer.uri = printer_proto.uri();
  printer.source = PrinterSourceProtoToIdl(printer_proto.source());
  return printer;
}

api::printing::PrinterStatus PrinterErrorCodeToIdl(
    proto::PrintJobInfo_PrinterErrorCode error_code_proto) {
  switch (error_code_proto) {
    case proto::PrintJobInfo_PrinterErrorCode_NO_ERROR:
      return api::printing::PrinterStatus::kAvailable;
    case proto::PrintJobInfo_PrinterErrorCode_PAPER_JAM:
      return api::printing::PrinterStatus::kPaperJam;
    case proto::PrintJobInfo_PrinterErrorCode_OUT_OF_PAPER:
      return api::printing::PrinterStatus::kOutOfPaper;
    case proto::PrintJobInfo_PrinterErrorCode_OUT_OF_INK:
      return api::printing::PrinterStatus::kOutOfInk;
    case proto::PrintJobInfo_PrinterErrorCode_DOOR_OPEN:
      return api::printing::PrinterStatus::kDoorOpen;
    case proto::PrintJobInfo_PrinterErrorCode_PRINTER_UNREACHABLE:
      return api::printing::PrinterStatus::kUnreachable;
    case proto::PrintJobInfo_PrinterErrorCode_TRAY_MISSING:
      return api::printing::PrinterStatus::kTrayMissing;
    case proto::PrintJobInfo_PrinterErrorCode_OUTPUT_FULL:
      return api::printing::PrinterStatus::kOutputFull;
    case proto::PrintJobInfo_PrinterErrorCode_STOPPED:
      return api::printing::PrinterStatus::kStopped;
    case proto::PrintJobInfo_PrinterErrorCode_FILTER_FAILED:
    case proto::PrintJobInfo_PrinterErrorCode_UNKNOWN_ERROR:
    case proto::PrintJobInfo_PrinterErrorCode_CLIENT_UNAUTHORIZED:
      return api::printing::PrinterStatus::kGenericIssue;
    case proto::PrintJobInfo_PrinterErrorCode_EXPIRED_CERTIFICATE:
      return api::printing::PrinterStatus::kExpiredCertificate;
    case proto::
        PrintJobInfo_PrinterErrorCode_PrintJobInfo_PrinterErrorCode_INT_MIN_SENTINEL_DO_NOT_USE_:
    case proto::
        PrintJobInfo_PrinterErrorCode_PrintJobInfo_PrinterErrorCode_INT_MAX_SENTINEL_DO_NOT_USE_:
      NOTREACHED_IN_MIGRATION();
      return api::printing::PrinterStatus::kGenericIssue;
  }
  return api::printing::PrinterStatus::kGenericIssue;
}

}  // namespace

idl::PrintJobInfo PrintJobInfoProtoToIdl(
    const proto::PrintJobInfo& print_job_info_proto) {
  idl::PrintJobInfo print_job_info;
  print_job_info.id = print_job_info_proto.id();
  print_job_info.title = print_job_info_proto.title();
  print_job_info.source =
      PrintJobSourceProtoToIdl(print_job_info_proto.source());
  print_job_info.status =
      PrintJobStatusProtoToIdl(print_job_info_proto.status());
  print_job_info.creation_time =
      base::checked_cast<double>(print_job_info_proto.creation_time());
  print_job_info.completion_time =
      base::checked_cast<double>(print_job_info_proto.completion_time());
  print_job_info.printer = PrinterProtoToIdl(print_job_info_proto.printer());
  print_job_info.printer_status =
      PrinterErrorCodeToIdl(print_job_info_proto.printer_error_code());
  print_job_info.settings =
      PrintSettingsProtoToIdl(print_job_info_proto.settings());
  print_job_info.number_of_pages = print_job_info_proto.number_of_pages();
  return print_job_info;
}

}  // namespace extensions
