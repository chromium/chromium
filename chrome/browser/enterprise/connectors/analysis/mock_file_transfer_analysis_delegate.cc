// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/mock_file_transfer_analysis_delegate.h"

#include "base/functional/callback.h"
#include "chrome/browser/enterprise/connectors/analysis/file_transfer_analysis_delegate.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_connectors {

MockFileTransferAnalysisDelegate::MockFileTransferAnalysisDelegate(
    safe_browsing::DeepScanAccessPoint access_point,
    storage::FileSystemURL source_url,
    storage::FileSystemURL destination_url,
    Profile* profile,
    storage::FileSystemContext* file_system_context,
    AnalysisSettings settings)
    : FileTransferAnalysisDelegate(access_point,
                                   source_url,
                                   destination_url,
                                   profile,
                                   file_system_context,
                                   std::move(settings)) {}

MockFileTransferAnalysisDelegate::~MockFileTransferAnalysisDelegate() = default;

}  // namespace enterprise_connectors
