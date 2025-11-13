// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/clipboard_analysis_request.h"

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

// Calling GetRequestData() twice should return the same valid region.
TEST(ClipboardAnalysisRequest, GetRequestData) {
  std::string contents("contents");
  ClipboardAnalysisRequest request(AnalysisSettings().cloud_or_local_settings,
                                   contents, base::DoNothing());

  safe_browsing::BinaryUploadService::Request::Data data1;
  request.GetRequestData(base::BindLambdaForTesting(
      [&data1](ScanRequestUploadResult result,
               safe_browsing::BinaryUploadService::Request::Data data) {
        data1 = std::move(data);
      }));

  safe_browsing::BinaryUploadService::Request::Data data2;
  request.GetRequestData(base::BindLambdaForTesting(
      [&data2](ScanRequestUploadResult result,
               safe_browsing::BinaryUploadService::Request::Data data) {
        data2 = std::move(data);
      }));

  ASSERT_EQ(data1.size, data2.size);
  ASSERT_EQ(data1.size, contents.size());
  ASSERT_EQ(data1.contents, data2.contents);
}

}  // namespace enterprise_connectors
