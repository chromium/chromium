// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/page_print_analysis_request.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

static base::ReadOnlySharedMemoryRegion CreateFakePage(size_t page_size) {
  base::MappedReadOnlyRegion page =
      base::ReadOnlySharedMemoryRegion::Create(page_size);
  memset(page.mapping.memory(), 'a', page_size);
  return std::move(page.region);
}

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

 private:
  base::test::SingleThreadTaskEnvironment environment_;
};

INSTANTIATE_TEST_SUITE_P(,
                         PagePrintAnalysisRequestTest,
                         testing::ValuesIn(kTestValues));

TEST_P(PagePrintAnalysisRequestTest, CloudSizes) {
  PagePrintAnalysisRequest request(
      AnalysisSettings(), CreateFakePage(page_size()), base::DoNothing());

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
        ASSERT_EQ(data.page.GetSize(), page_size());
        ASSERT_TRUE(data.page.IsValid());

        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_P(PagePrintAnalysisRequestTest, LocalSizes) {
  AnalysisSettings settings;
  settings.cloud_or_local_settings =
      CloudOrLocalAnalysisSettings(LocalAnalysisSettings());

  PagePrintAnalysisRequest request(
      settings, CreateFakePage(page_size()), base::DoNothing());

  base::RunLoop run_loop;
  request.GetRequestData(base::BindLambdaForTesting(
      [&run_loop, this](
          safe_browsing::BinaryUploadService::Result result,
          safe_browsing::BinaryUploadService::Request::Data data) {
        ASSERT_TRUE(data.contents.empty());
        ASSERT_TRUE(data.hash.empty());
        ASSERT_TRUE(data.mime_type.empty());
        ASSERT_TRUE(data.path.empty());

        ASSERT_EQ(result, safe_browsing::BinaryUploadService::Result::SUCCESS);
        ASSERT_EQ(data.size, page_size());
        ASSERT_EQ(data.page.GetSize(), page_size());
        ASSERT_TRUE(data.page.IsValid());

        run_loop.Quit();
      }));

  run_loop.Run();
}

// Calling GetRequestData() twice should return the same valid region.
TEST(PagePrintAnalysisRequest, GetRequestData) {
  PagePrintAnalysisRequest request(AnalysisSettings(), CreateFakePage(1024),
                                   base::DoNothing());

  safe_browsing::BinaryUploadService::Request::Data data1;
  request.GetRequestData(base::BindLambdaForTesting(
      [&data1](safe_browsing::BinaryUploadService::Result result,
               safe_browsing::BinaryUploadService::Request::Data data) {
        data1 = std::move(data);
      }));

  safe_browsing::BinaryUploadService::Request::Data data2;
  request.GetRequestData(base::BindLambdaForTesting(
      [&data2](safe_browsing::BinaryUploadService::Result result,
               safe_browsing::BinaryUploadService::Request::Data data) {
        data2 = std::move(data);
      }));

  ASSERT_EQ(data1.size, data2.size);

  ASSERT_TRUE(data1.page.IsValid());
  ASSERT_TRUE(data2.page.IsValid());
  ASSERT_EQ(data1.page.GetSize(), data2.page.GetSize());
  ASSERT_EQ(data1.page.GetGUID(), data2.page.GetGUID());
}

}  // namespace enterprise_connectors
