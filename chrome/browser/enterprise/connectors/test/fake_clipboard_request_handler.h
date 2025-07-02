// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_FAKE_CLIPBOARD_REQUEST_HANDLER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_FAKE_CLIPBOARD_REQUEST_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/connectors/analysis/clipboard_request_handler.h"

namespace enterprise_connectors::test {

class FakeContentAnalysisDelegate;

class FakeClipboardRequestHandler : public ClipboardRequestHandler {
 public:
  static std::unique_ptr<ClipboardRequestHandler> Create(
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
      CompletionCallback callback);

 protected:
  using ClipboardRequestHandler::ClipboardRequestHandler;

  void UploadForDeepScanning(
      std::unique_ptr<enterprise_connectors::ClipboardAnalysisRequest> request)
      override;

  raw_ptr<enterprise_connectors::test::FakeContentAnalysisDelegate> delegate_;
};

}  // namespace enterprise_connectors::test

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_FAKE_CLIPBOARD_REQUEST_HANDLER_H_
