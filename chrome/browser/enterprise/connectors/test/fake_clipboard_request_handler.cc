// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/test/fake_clipboard_request_handler.h"

#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_delegate.h"

namespace enterprise_connectors::test {

// static
std::unique_ptr<ClipboardRequestHandler> FakeClipboardRequestHandler::Create(
    FakeContentAnalysisDelegate* delegate,
    ContentAnalysisInfo* content_analysis_info,
    safe_browsing::BinaryUploadService* upload_service,
    Profile* profile,
    GURL url,
    Type type,
    DeepScanAccessPoint access_point,
    ContentMetaData::CopiedTextSource clipboard_source,
    std::string source_content_area_email,
    std::string content_transfer_method,
    std::string data,
    CompletionCallback callback) {
  auto handler = base::WrapUnique(new FakeClipboardRequestHandler(
      content_analysis_info, upload_service, profile, std::move(url), type,
      access_point, std::move(clipboard_source),
      std::move(source_content_area_email), std::move(content_transfer_method),
      std::move(data), std::move(callback)));
  handler->delegate_ = delegate;
  return handler;
}

void FakeClipboardRequestHandler::UploadForDeepScanning(
    std::unique_ptr<enterprise_connectors::ClipboardAnalysisRequest> request) {
  delegate_->FakeUploadClipboardDataForDeepScanning(type_, std::move(request));
}

}  // namespace enterprise_connectors::test
