// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_COMPRESSOR_IO_JAVASCRIPT_STREAM_H_
#define CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_COMPRESSOR_IO_JAVASCRIPT_STREAM_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/resources/chromeos/zip_archiver/cpp/compressor_stream.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/var_array_buffer.h"
#include "ppapi/utility/completion_callback_factory.h"

class JavaScriptCompressorRequestorInterface;

class CompressorIOJavaScriptStream : public CompressorStream {
 public:
  CompressorIOJavaScriptStream(
      JavaScriptCompressorRequestorInterface* requestor);

  ~CompressorIOJavaScriptStream() override;

  // Flushes the data in buffer_. Since minizip sends tons of write requests and
  // communication between C++ and JS is very expensive, we need to cache data
  // in buffer_ and send them in a lump.
  int64_t Flush() override;

  int64_t Write(int64_t zip_offset,
                int64_t zip_length,
                const char* zip_buffer) override;

  int64_t WriteChunkDone(int64_t write_bytes) override;

  int64_t Read(int64_t bytes_to_read, char* destination_buffer) override;

  int64_t ReadFileChunkDone(int64_t read_bytes,
                            pp::VarArrayBuffer* buffer) override;

 private:
  // A requestor that makes calls to JavaScript to read and write chunks.
  JavaScriptCompressorRequestorInterface* requestor_;

  base::Lock shared_state_lock_;
  base::ConditionVariable available_data_cond_;
  base::ConditionVariable data_written_cond_;

  // The bytelength of the data written onto the archive for the last write
  // chunk request. If this value is negative, some error occurred when writing
  // a chunk in JavaScript.
  int64_t written_bytes_;

  // The bytelength of the data read from the entry for the last read file chunk
  // request. If this value is negative, some error occurred when reading a
  // chunk in JavaScript.
  int64_t read_bytes_;

  // True if destination_buffer_ is available.
  bool available_data_;

  // Stores the data read from JavaScript.
  char* destination_buffer_;

  // The current offset from which buffer_ has data.
  int64_t buffer_offset_;

  // The size of the data in buffer_.
  int64_t buffer_data_length_;

  // The buffer that contains cached data.
  std::unique_ptr<char[]> buffer_;
};

#endif  // CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_COMPRESSOR_IO_JAVASCRIPT_STREAM_H_
