// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_UNITTEST_HELPERS_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_UNITTEST_HELPERS_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager_common.h"

namespace webrtc_event_logging {

// Which type of compression, if any, LogFileWriterTest should use.
enum class WebRtcEventLogCompression {
  NONE,
  GZIP_NULL_ESTIMATION,
  GZIP_PERFECT_ESTIMATION
};

// Produce a LogFileWriter::Factory object.
std::unique_ptr<LogFileWriter::Factory> CreateLogFileWriterFactory(
    WebRtcEventLogCompression compression);

#if BUILDFLAG(IS_POSIX)
void RemoveWritePermissions(const base::FilePath& path);
#endif  // BUILDFLAG(IS_POSIX)

// Always estimates strings to be compressed to zero bytes.
class NullEstimator : public CompressedSizeEstimator {
 public:
  class Factory : public CompressedSizeEstimator::Factory {
   public:
    ~Factory() override = default;

    std::unique_ptr<CompressedSizeEstimator> Create() const override;
  };

  ~NullEstimator() override = default;

  size_t EstimateCompressedSize(const std::string& input) const override;
};

// Provides a perfect estimation of the compressed size by cheating - performing
// actual compression, then reporting the resulting size.
// This class is stateful; the number, nature and order of calls to
// EstimateCompressedSize() is important.
class PerfectGzipEstimator : public CompressedSizeEstimator {
 public:
  class Factory : public CompressedSizeEstimator::Factory {
   public:
    ~Factory() override = default;

    std::unique_ptr<CompressedSizeEstimator> Create() const override;
  };

  PerfectGzipEstimator();

  ~PerfectGzipEstimator() override;

  size_t EstimateCompressedSize(const std::string& input) const override;

 private:
  // This compressor allows EstimateCompressedSize to return an exact estimate.
  // EstimateCompressedSize is normally const, but here we fake it, so we set
  // it as mutable.
  mutable std::unique_ptr<LogCompressor> compressor_;
};

// Check the gzipped size of |uncompressed|, including header and footer,
// assuming it were gzipped on its own.
size_t GzippedSize(const std::string& uncompressed);

// Same as other version, but with elements compressed in sequence.
size_t GzippedSize(const std::vector<std::string>& uncompressed);

}  // namespace webrtc_event_logging

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_UNITTEST_HELPERS_H_
