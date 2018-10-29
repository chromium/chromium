// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_COMPRESSOR_STREAM_H_
#define CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_COMPRESSOR_STREAM_H_

#include <cstdint>
#include <string>

#include "ppapi/cpp/var_array_buffer.h"

// A IO class that reads and writes data from and to files through JavaScript.
// Currently, Read() and Write() methods are not called at the same time.
// To improve the performance, these should be called in parallel by preparing
// two buffers and calling them with these buffers alternatively.
class CompressorStream {
 public:
  virtual ~CompressorStream() {}

  // Flush the data in buffer_. This method is called when the buffer gets full
  // or needs to have the different range of data from the current data in it.
  // This method must not be called in the main thread.
  virtual int64_t Flush() = 0;

  // Writes the given buffer onto the archive. After sending a write chunk
  // request to JavaScript, it waits until WriteChunkDone() is called in the
  // main thread. Thus, This method must not be called in the main thread.
  virtual int64_t Write(int64_t zip_offset,
                        int64_t zip_length,
                        const char* zip_buffer) = 0;

  // Called when write chunk done response arrives from JavaScript. Sends a
  // signal to invoke Write function in another thread again.
  virtual int64_t WriteChunkDone(int64_t write_bytes) = 0;

  // Reads a file chunk from the entry that is currently being processed. After
  // sending a read file chunk request to JavaScript, it waits until
  // ReadFileChunkDone() is called in the main thread. Thus, This method must
  // not be called in the main thread.
  virtual int64_t Read(int64_t bytes_to_read, char* destination_buffer) = 0;

  // Called when read file chunk done response arrives from JavaScript. Copies
  // the binary data in the given buffer to destination_buffer_ and Sends a
  // signal to invoke Read function in another thread again. buffer must not be
  // const because buffer.Map() and buffer.Unmap() can not be called with const.
  virtual int64_t ReadFileChunkDone(int64_t read_bytes,
                                    pp::VarArrayBuffer* buffer) = 0;
};

#endif  // CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_COMPRESSOR_STREAM_H_
