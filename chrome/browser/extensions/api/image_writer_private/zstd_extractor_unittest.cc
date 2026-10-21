// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/zstd_extractor.h"

#include <limits>
#include <memory>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/extensions/api/image_writer_private/error_constants.h"
#include "chrome/browser/extensions/api/image_writer_private/extraction_properties.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions::image_writer {

class ZstdExtractorTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  std::unique_ptr<ZstdExtractor> CreateExtractor(
      ExtractionProperties properties) {
    return ZstdExtractor::CreateForTesting(std::move(properties));
  }

  void RunOnProgress(ZstdExtractor* extractor,
                     uint64_t total,
                     uint64_t progress) {
    static_cast<chrome::mojom::SingleFileExtractorListener*>(extractor)
        ->OnProgress(total, progress);
  }

  const base::FilePath& temp_dir() const { return temp_dir_.GetPath(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

// Tests that ZstdExtractor fails gracefully if it receives a progress update
// with 0 total bytes. This guards against a potential divide-by-zero crash in
// the progress calculation logic.
TEST_F(ZstdExtractorTest, ZeroTotalBytes) {
  ExtractionProperties properties;
  properties.temp_dir_path = temp_dir();

  base::MockCallback<ExtractionProperties::FailureCallback> failure_callback;
  properties.failure_callback = failure_callback.Get();

  base::MockCallback<ExtractionProperties::ProgressCallback> progress_callback;
  properties.progress_callback = progress_callback.Get();

  EXPECT_CALL(failure_callback, Run(::testing::_)).Times(0);
  EXPECT_CALL(progress_callback, Run(::testing::_, ::testing::_)).Times(0);

  std::unique_ptr<ZstdExtractor> extractor =
      CreateExtractor(std::move(properties));
  RunOnProgress(extractor.get(), /*total=*/0, /*progress=*/0);
}

// Tests that ZstdExtractor fails gracefully and reports a bad message if it
// receives a progress update with total bytes exceeding INT64_MAX.
TEST_F(ZstdExtractorTest, ExceedsInt64MaxTotalBytes) {
  ExtractionProperties properties;
  properties.temp_dir_path = temp_dir();

  base::MockCallback<ExtractionProperties::FailureCallback> failure_callback;
  properties.failure_callback = failure_callback.Get();

  base::MockCallback<ExtractionProperties::ProgressCallback> progress_callback;
  properties.progress_callback = progress_callback.Get();

  EXPECT_CALL(failure_callback, Run(::testing::_)).Times(0);
  EXPECT_CALL(progress_callback, Run(::testing::_, ::testing::_)).Times(0);

  std::unique_ptr<ZstdExtractor> extractor =
      CreateExtractor(std::move(properties));

  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  RunOnProgress(
      extractor.get(),
      /*total=*/static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1,
      /*progress=*/0);

  EXPECT_EQ("invalid extraction progress values",
            bad_message_observer.WaitForBadMessage());
}

// Tests that ZstdExtractor fails gracefully and reports a bad message if it
// receives a progress update with progress_bytes exceeding total_bytes.
TEST_F(ZstdExtractorTest, InvalidProgressBytes) {
  ExtractionProperties properties;
  properties.temp_dir_path = temp_dir();

  base::MockCallback<ExtractionProperties::FailureCallback> failure_callback;
  properties.failure_callback = failure_callback.Get();

  base::MockCallback<ExtractionProperties::ProgressCallback> progress_callback;
  properties.progress_callback = progress_callback.Get();

  EXPECT_CALL(failure_callback, Run(::testing::_)).Times(0);
  EXPECT_CALL(progress_callback, Run(::testing::_, ::testing::_)).Times(0);

  std::unique_ptr<ZstdExtractor> extractor =
      CreateExtractor(std::move(properties));

  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  RunOnProgress(extractor.get(), /*total=*/100, /*progress=*/101);

  EXPECT_EQ("invalid extraction progress values",
            bad_message_observer.WaitForBadMessage());
}

}  // namespace extensions::image_writer
