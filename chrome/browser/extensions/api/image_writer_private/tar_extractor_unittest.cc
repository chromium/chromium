// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/tar_extractor.h"

#include <limits>
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

class TarExtractorTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  TarExtractor* CreateExtractor(ExtractionProperties properties) {
    return TarExtractor::CreateForTesting(std::move(properties));
  }

  void RunOnProgress(TarExtractor* extractor,
                     uint64_t total,
                     uint64_t progress) {
    static_cast<chrome::mojom::SingleFileExtractorListener*>(extractor)
        ->OnProgress(total, progress);
  }

  void DeleteExtractor(TarExtractor* extractor) {
    // TarExtractor manages its own lifetime and therefore has a private
    // destructor. We need to cast to the base class (which has a virtual
    // destructor) to ensure the object can be deleted by the test.
    delete static_cast<chrome::mojom::SingleFileExtractorListener*>(extractor);
  }

  const base::FilePath& temp_dir() const { return temp_dir_.GetPath(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

// Tests that the TarExtractor fails gracefully if it receives a progress update
// with 0 total bytes. This guards against a potential divide-by-zero crash in
// the progress calculation logic.
TEST_F(TarExtractorTest, ZeroTotalBytes) {
  ExtractionProperties properties;
  properties.temp_dir_path = temp_dir();

  base::MockCallback<ExtractionProperties::FailureCallback> failure_callback;
  properties.failure_callback = failure_callback.Get();

  base::MockCallback<ExtractionProperties::ProgressCallback> progress_callback;
  properties.progress_callback = progress_callback.Get();

  // We expect no failure callback since 0 bytes in the file isn't necessarily a
  // failure to extract.
  EXPECT_CALL(failure_callback, Run(::testing::_)).Times(0);

  // We expect no progress callback because total_bytes is 0.
  EXPECT_CALL(progress_callback, Run(::testing::_, ::testing::_)).Times(0);

  // Create extractor.
  TarExtractor* extractor = CreateExtractor(std::move(properties));

  // Call OnProgress with 0 total bytes.
  RunOnProgress(extractor, /*total=*/0, /*progress=*/0);

  // Clean up extractor to satisfy leak checks.
  DeleteExtractor(extractor);
}

// Tests that the TarExtractor fails gracefully and reports a bad message if it
// receives a progress update with total bytes exceeding INT64_MAX. This guards
// against a potential underflow when the value is implicitly cast to int64_t
// in Operation::OnExtractProgress.
TEST_F(TarExtractorTest, ExceedsInt64MaxTotalBytes) {
  ExtractionProperties properties;
  properties.temp_dir_path = temp_dir();

  base::MockCallback<ExtractionProperties::FailureCallback> failure_callback;
  properties.failure_callback = failure_callback.Get();

  base::MockCallback<ExtractionProperties::ProgressCallback> progress_callback;
  properties.progress_callback = progress_callback.Get();

  // We expect no failure callback since the bad message will terminate the
  // utility process, and the mojo disconnect handler will handle cleanup.
  EXPECT_CALL(failure_callback, Run(::testing::_)).Times(0);

  // We expect no progress callback because total_bytes exceeds INT64_MAX.
  EXPECT_CALL(progress_callback, Run(::testing::_, ::testing::_)).Times(0);

  TarExtractor* extractor = CreateExtractor(std::move(properties));

  // Simulate a mojo message dispatch context so that ReportBadMessage can hook
  // into it without crashing the test.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  // Call OnProgress with total bytes exceeding INT64_MAX.
  RunOnProgress(
      extractor,
      /*total=*/static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1,
      /*progress=*/0);

  // Verify that ReportBadMessage was called with the expected error string.
  EXPECT_EQ("invalid extraction progress values",
            bad_message_observer.WaitForBadMessage());

  // Clean up extractor to satisfy leak checks.
  DeleteExtractor(extractor);
}

// Tests that the TarExtractor fails gracefully and reports a bad message if it
// receives a progress update with progress_bytes exceeding total_bytes.
// This guards against a compromised utility process sending spoofed progress
// values.
TEST_F(TarExtractorTest, InvalidProgressBytes) {
  ExtractionProperties properties;
  properties.temp_dir_path = temp_dir();

  base::MockCallback<ExtractionProperties::FailureCallback> failure_callback;
  properties.failure_callback = failure_callback.Get();

  base::MockCallback<ExtractionProperties::ProgressCallback> progress_callback;
  properties.progress_callback = progress_callback.Get();

  // We expect no failure callback since the bad message will terminate the
  // utility process.
  EXPECT_CALL(failure_callback, Run(::testing::_)).Times(0);

  // We expect no progress callback because progress_bytes exceeds total_bytes.
  EXPECT_CALL(progress_callback, Run(::testing::_, ::testing::_)).Times(0);

  // Create extractor.
  TarExtractor* extractor = CreateExtractor(std::move(properties));

  // Simulate a mojo message dispatch context.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  // Call OnProgress with progress_bytes exceeding total_bytes.
  RunOnProgress(extractor, /*total=*/100, /*progress=*/101);

  // Verify that ReportBadMessage was called with the expected error string.
  EXPECT_EQ("invalid extraction progress values",
            bad_message_observer.WaitForBadMessage());

  // Clean up extractor to satisfy leak checks.
  DeleteExtractor(extractor);
}

}  // namespace extensions::image_writer
