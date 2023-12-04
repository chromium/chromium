// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/sharesheet_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "components/services/app_service/public/cpp/intent_util.h"

namespace {
const char kMimeTypeSeparator[] = "/";
}  // namespace

namespace sharesheet {

const char kSharesheetUserActionResultHistogram[] =
    "ChromeOS.Sharesheet.UserAction";
const char kSharesheetAppCountAllResultHistogram[] =
    "ChromeOS.Sharesheet.AppCount2.All";
const char kSharesheetAppCountArcResultHistogram[] =
    "ChromeOS.Sharesheet.AppCount2.Arc";
const char kSharesheetAppCountWebResultHistogram[] =
    "ChromeOS.Sharesheet.AppCount2.Web";
const char kSharesheetLaunchSourceResultHistogram[] =
    "ChromeOS.Sharesheet.LaunchSource";
const char kSharesheetFileCountResultHistogram[] =
    "ChromeOS.Sharesheet.FileCount";
const char kSharesheetMimeTypeResultHistogram[] =
    "ChromeOS.Sharesheet.Invocation.MimeType";
const char kSharesheetCopyToClipboardMimeTypeResultHistogram[] =
    "ChromeOS.Sharesheet.CopyToClipboard.MimeType";

SharesheetMetrics::SharesheetMetrics() = default;

void SharesheetMetrics::RecordSharesheetActionMetrics(const UserAction action) {
  base::UmaHistogramEnumeration(kSharesheetUserActionResultHistogram, action);
}

void SharesheetMetrics::RecordSharesheetAppCount(const int app_count) {
  base::UmaHistogramCounts100(kSharesheetAppCountAllResultHistogram, app_count);
}

void SharesheetMetrics::RecordSharesheetArcAppCount(const int app_count) {
  base::UmaHistogramCounts100(kSharesheetAppCountArcResultHistogram, app_count);
}

void SharesheetMetrics::RecordSharesheetWebAppCount(const int app_count) {
  base::UmaHistogramCounts100(kSharesheetAppCountWebResultHistogram, app_count);
}

void SharesheetMetrics::RecordSharesheetLaunchSource(
    const LaunchSource source) {
  base::UmaHistogramEnumeration(kSharesheetLaunchSourceResultHistogram, source);
}

void SharesheetMetrics::RecordSharesheetFilesSharedCount(const int file_count) {
  base::UmaHistogramCounts100(kSharesheetFileCountResultHistogram, file_count);
}

void SharesheetMetrics::RecordSharesheetMimeType(const MimeType mime_type) {
  base::UmaHistogramEnumeration(kSharesheetMimeTypeResultHistogram, mime_type);
}

void SharesheetMetrics::RecordCopyToClipboardShareActionMimeType(
    const MimeType mime_type) {
  base::UmaHistogramEnumeration(
      kSharesheetCopyToClipboardMimeTypeResultHistogram, mime_type);
}

SharesheetMetrics::MimeType SharesheetMetrics::ConvertMimeTypeForMetrics(
    std::string mime_type) {
  std::vector<std::string> type =
      base::SplitString(mime_type, kMimeTypeSeparator, base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  if (type.size() == 0) {
    return MimeType::kUnknown;
  }

  if (type[0] == "text") {
    return MimeType::kTextFile;
  } else if (type[0] == "image") {
    return MimeType::kImageFile;
  } else if (type[0] == "video") {
    return MimeType::kVideoFile;
  } else if (type[0] == "audio") {
    return MimeType::kAudioFile;
  } else if (mime_type == "application/pdf") {
    return MimeType::kPdfFile;
  } else {
    return MimeType::kUnknown;
  }
}

base::flat_set<SharesheetMetrics::MimeType>
SharesheetMetrics::GetMimeTypesFromIntentForMetrics(
    const apps::IntentPtr& intent) {
  base::flat_set<MimeType> mime_types_to_record;

  if (intent->share_text.has_value()) {
    apps_util::SharedText extracted_text =
        apps_util::ExtractSharedText(intent->share_text.value());

    if (!extracted_text.text.empty()) {
      mime_types_to_record.insert(MimeType::kText);
    }
    if (!extracted_text.url.is_empty()) {
      mime_types_to_record.insert(MimeType::kUrl);
    }
  }

  if (!intent->files.empty()) {
    for (const auto& file : intent->files) {
      if (file->mime_type.has_value())
        mime_types_to_record.insert(
            ConvertMimeTypeForMetrics(file->mime_type.value()));
    }
  }
  return mime_types_to_record;
}

}  // namespace sharesheet
