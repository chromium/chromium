// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/selection/metrics_logger.h"

#include <string_view>

#include "base/metrics/histogram_functions.h"

namespace selection {

namespace {

DocumentMimeType GetDocumentMimeType(std::string_view mime_type) {
  if (mime_type == "application/pdf") {
    return DocumentMimeType::kPdf;
  } else if (mime_type == "text/html") {
    return DocumentMimeType::kHtml;
  } else if (mime_type == "text/plain") {
    return DocumentMimeType::kPlainText;
  } else if (mime_type.starts_with("image/")) {
    return DocumentMimeType::kImage;
  } else if (mime_type.starts_with("video/")) {
    return DocumentMimeType::kVideo;
  } else if (mime_type.starts_with("audio/")) {
    return DocumentMimeType::kAudio;
  } else if (mime_type == "application/json") {
    return DocumentMimeType::kJson;
  }
  return DocumentMimeType::kUnknown;
}

}  // namespace

MetricsLogger::MetricsLogger(EntryPoint entry_point,
                             std::string_view mime_type) {
  base::UmaHistogramEnumeration("SmartSelection.Invocation.ByEntryPoint",
                                entry_point);
  base::UmaHistogramEnumeration("SmartSelection.Invocation.ByDocumentMimeType",
                                GetDocumentMimeType(mime_type));
}

MetricsLogger::~MetricsLogger() = default;

}  // namespace selection
