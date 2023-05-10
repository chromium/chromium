// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/scanning/scanning_metrics_handler.h"

#include "ash/webui/scanning/mojom/scanning.mojom.h"
#include "ash/webui/scanning/scanning_uma.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"

namespace ash {

namespace {

namespace mojo_ipc = scanning::mojom;

// Scan job settings constants.
constexpr char kSourceType[] = "sourceType";
constexpr char kFileType[] = "fileType";
constexpr char kColorMode[] = "colorMode";
constexpr char kPageSize[] = "pageSize";
constexpr char kResolution[] = "resolution";

}  // namespace

ScanningMetricsHandler::ScanningMetricsHandler() = default;

ScanningMetricsHandler::~ScanningMetricsHandler() = default;

void ScanningMetricsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "recordNumScanSettingChanges",
      base::BindRepeating(
          &ScanningMetricsHandler::HandleRecordNumScanSettingChanges,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "recordScanCompleteAction",
      base::BindRepeating(
          &ScanningMetricsHandler::HandleRecordScanCompleteAction,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "recordScanJobSettings",
      base::BindRepeating(&ScanningMetricsHandler::HandleRecordScanJobSettings,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "recordNumCompletedScans",
      base::BindRepeating(
          &ScanningMetricsHandler::HandleRecordNumCompletedScans,
          base::Unretained(this)));
}

void ScanningMetricsHandler::HandleRecordNumScanSettingChanges(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  base::UmaHistogramCounts100("Scanning.NumScanSettingChanges",
                              args[0].GetInt());
}

void ScanningMetricsHandler::HandleRecordScanCompleteAction(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  base::UmaHistogramEnumeration(
      "Scanning.ScanCompleteAction",
      static_cast<scanning::ScanCompleteAction>(args[0].GetInt()));
}

void ScanningMetricsHandler::HandleRecordScanJobSettings(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  const base::Value::Dict& scan_job_settings = args[0].GetDict();

  base::UmaHistogramEnumeration(
      "Scanning.ScanJobSettings.Source",
      static_cast<mojo_ipc::SourceType>(
          scan_job_settings.FindInt(kSourceType).value()));
  base::UmaHistogramEnumeration(
      "Scanning.ScanJobSettings.FileType",
      static_cast<mojo_ipc::FileType>(
          scan_job_settings.FindInt(kFileType).value()));
  base::UmaHistogramEnumeration(
      "Scanning.ScanJobSettings.ColorMode",
      static_cast<mojo_ipc::ColorMode>(
          scan_job_settings.FindInt(kColorMode).value()));
  base::UmaHistogramEnumeration(
      "Scanning.ScanJobSettings.PageSize",
      static_cast<mojo_ipc::PageSize>(
          scan_job_settings.FindInt(kPageSize).value()));
  const scanning::ScanJobSettingsResolution resolution =
      scanning::GetResolutionEnumValue(
          scan_job_settings.FindInt(kResolution).value());
  if (resolution != scanning::ScanJobSettingsResolution::kUnexpectedDpi) {
    base::UmaHistogramEnumeration("Scanning.ScanJobSettings.Resolution",
                                  resolution);
  }
}

void ScanningMetricsHandler::HandleRecordNumCompletedScans(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  base::UmaHistogramCounts100("Scanning.NumCompletedScansInSession",
                              args[0].GetInt());
}

}  // namespace ash
