// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_MOCK_FILE_TRANSFER_ANALYSIS_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_MOCK_FILE_TRANSFER_ANALYSIS_DELEGATE_H_

#include "base/functional/callback.h"
#include "chrome/browser/enterprise/connectors/analysis/file_transfer_analysis_delegate.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_connectors {

class MockFileTransferAnalysisDelegate : public FileTransferAnalysisDelegate {
 public:
  MockFileTransferAnalysisDelegate(
      safe_browsing::DeepScanAccessPoint access_point,
      storage::FileSystemURL source_url,
      storage::FileSystemURL destination_url,
      Profile* profile,
      storage::FileSystemContext* file_system_context,
      AnalysisSettings settings);

  ~MockFileTransferAnalysisDelegate() override;

  MOCK_METHOD(void, UploadData, (base::OnceClosure callback), (override));

  MOCK_METHOD(FileTransferAnalysisResult,
              GetAnalysisResultAfterScan,
              (storage::FileSystemURL url),
              (override));

  MOCK_METHOD(std::vector<storage::FileSystemURL>,
              GetWarnedFiles,
              (),
              (const override));
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_MOCK_FILE_TRANSFER_ANALYSIS_DELEGATE_H_
