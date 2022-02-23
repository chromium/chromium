// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/page_print_analysis_request.h"

#include "base/callback_helpers.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

constexpr std::pair<safe_browsing::BinaryUploadService::Result, size_t>
    kTestValues[] = {
        {safe_browsing::BinaryUploadService::Result::SUCCESS, 1024},
        {safe_browsing::BinaryUploadService::Result::FILE_TOO_LARGE,
         50 * 1024 * 1024}};

class PagePrintAnalysisRequestTest
    : public testing::TestWithParam<
          std::pair<safe_browsing::BinaryUploadService::Result, size_t>> {
 public:
  safe_browsing::BinaryUploadService::Result expected_result() const {
    return GetParam().first;
  }

  size_t page_size() const { return GetParam().second; }

  base::ReadOnlySharedMemoryRegion CreateFakePage() {
    base::MappedReadOnlyRegion page =
        base::ReadOnlySharedMemoryRegion::Create(page_size());
    memset(page.mapping.memory(), 'a', page_size());
    return std::move(page.region);
  }

 private:
  base::test::SingleThreadTaskEnvironment environment_;
};

INSTANTIATE_TEST_SUITE_P(,
                         PagePrintAnalysisRequestTest,
                         testing::ValuesIn(kTestValues));

TEST_P(PagePrintAnalysisRequestTest, Sizes) {
  PagePrintAnalysisRequest request(AnalysisSettings(), CreateFakePage(),
                                   base::DoNothing());

  base::RunLoop run_loop;
  request.GetRequestData(base::BindLambdaForTesting(
      [&run_loop, this](
          safe_browsing::BinaryUploadService::Result result,
          safe_browsing::BinaryUploadService::Request::Data data) {
        ASSERT_TRUE(data.contents.empty());
        ASSERT_TRUE(data.hash.empty());
        ASSERT_TRUE(data.mime_type.empty());
        ASSERT_TRUE(data.path.empty());

        ASSERT_EQ(result, expected_result());
        ASSERT_EQ(data.size, page_size());
        if (expected_result() ==
            safe_browsing::BinaryUploadService::Result::SUCCESS) {
          ASSERT_EQ(data.page.GetSize(), page_size());
          ASSERT_TRUE(data.page.IsValid());
        }

        run_loop.Quit();
      }));

  run_loop.Run();
}

}  // namespace enterprise_connectors
