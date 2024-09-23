// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_event_log_manager_unittest_helpers.h"

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webrtc_event_logging {

// Produce a LogFileWriter::Factory object.
std::unique_ptr<LogFileWriter::Factory> CreateLogFileWriterFactory(
    WebRtcEventLogCompression compression) {
  switch (compression) {
    case WebRtcEventLogCompression::NONE:
      return std::make_unique<BaseLogFileWriterFactory>();
    case WebRtcEventLogCompression::GZIP_NULL_ESTIMATION:
      return std::make_unique<GzippedLogFileWriterFactory>(
          std::make_unique<GzipLogCompressorFactory>(
              std::make_unique<NullEstimator::Factory>()));
    case WebRtcEventLogCompression::GZIP_PERFECT_ESTIMATION:
      return std::make_unique<GzippedLogFileWriterFactory>(
          std::make_unique<GzipLogCompressorFactory>(
              std::make_unique<PerfectGzipEstimator::Factory>()));
  }
  NOTREACHED();
}

#if BUILDFLAG(IS_POSIX)
void RemoveWritePermissions(const base::FilePath& path) {
  int permissions;
  ASSERT_TRUE(base::GetPosixFilePermissions(path, &permissions));
  constexpr int write_permissions = base::FILE_PERMISSION_WRITE_BY_USER |
                                    base::FILE_PERMISSION_WRITE_BY_GROUP |
                                    base::FILE_PERMISSION_WRITE_BY_OTHERS;
  permissions &= ~write_permissions;
  ASSERT_TRUE(base::SetPosixFilePermissions(path, permissions));
}
#endif  // BUILDFLAG(IS_POSIX)

std::unique_ptr<CompressedSizeEstimator> NullEstimator::Factory::Create()
    const {
  return std::make_unique<NullEstimator>();
}

size_t NullEstimator::EstimateCompressedSize(const std::string& input) const {
  return 0;
}

std::unique_ptr<CompressedSizeEstimator> PerfectGzipEstimator::Factory::Create()
    const {
  return std::make_unique<PerfectGzipEstimator>();
}

PerfectGzipEstimator::PerfectGzipEstimator() {
  // This factory will produce an optimistic compressor that will always
  // think it can compress additional inputs, which will therefore allow
  // us to find out what the real compressed size it, since compression
  // will never be suppressed.
  GzipLogCompressorFactory factory(std::make_unique<NullEstimator::Factory>());

  compressor_ = factory.Create(std::optional<size_t>());
  DCHECK(compressor_);

  std::string ignored;
  compressor_->CreateHeader(&ignored);
}

PerfectGzipEstimator::~PerfectGzipEstimator() = default;

size_t PerfectGzipEstimator::EstimateCompressedSize(
    const std::string& input) const {
  std::string output;
  EXPECT_EQ(compressor_->Compress(input, &output), LogCompressor::Result::OK);
  return output.length();
}

size_t GzippedSize(const std::string& uncompressed) {
  PerfectGzipEstimator perfect_estimator;
  return kGzipOverheadBytes +
         perfect_estimator.EstimateCompressedSize(uncompressed);
}

size_t GzippedSize(const std::vector<std::string>& uncompressed) {
  PerfectGzipEstimator perfect_estimator;

  size_t result = kGzipOverheadBytes;
  for (const std::string& str : uncompressed) {
    result += perfect_estimator.EstimateCompressedSize(str);
  }

  return result;
}

}  // namespace webrtc_event_logging
