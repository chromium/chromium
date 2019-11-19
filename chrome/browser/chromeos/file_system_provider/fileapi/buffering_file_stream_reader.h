// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_FILEAPI_BUFFERING_FILE_STREAM_READER_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_FILEAPI_BUFFERING_FILE_STREAM_READER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "net/base/completion_once_callback.h"
#include "storage/browser/file_system/file_stream_reader.h"

namespace net {
class IOBuffer;
}  // namespace net

namespace chromeos {
namespace file_system_provider {

// Wraps the file stream reader implementation with a prefetching buffer.
// Reads data from the internal file stream reader in chunks of size at least
// |preloading_buffer_length| bytes (or less for the last chunk, because of
// EOF). Up to |max_bytes_to_read| of bytes can be requested in total.
//
// The underlying internal file stream reader *must not* return any values
// synchronously. Instead, results must be returned by a callback, including
// errors.
class BufferingFileStreamReader : public storage::FileStreamReader {
 public:
  BufferingFileStreamReader(
      std::unique_ptr<storage::FileStreamReader> file_stream_reader,
      int preloading_buffer_length,
      int64_t max_bytes_to_read);

  ~BufferingFileStreamReader() override;

  // storage::FileStreamReader overrides.
  int Read(net::IOBuffer* buf,
           int buf_len,
           net::CompletionOnceCallback callback) override;
  int64_t GetLength(net::Int64CompletionOnceCallback callback) override;

 private:
  // Copies data from the preloading buffer and updates the internal iterator.
  // Returns number of bytes successfully copied.
  int CopyFromPreloadingBuffer(scoped_refptr<net::IOBuffer> buffer,
                               int buffer_length);

  // Preloads data from the internal stream reader and calls the |callback|.
  void Preload(net::CompletionOnceCallback callback);

  void OnReadCompleted(net::CompletionOnceCallback callback, int result);

  // Called when preloading of a buffer chunk is finished. Updates state of the
  // preloading buffer and copied requested data to the |buffer|.
  void OnPreloadCompleted(scoped_refptr<net::IOBuffer> buffer,
                          int buffer_length,
                          net::CompletionOnceCallback callback,
                          int result);

  std::unique_ptr<storage::FileStreamReader> file_stream_reader_;
  int preloading_buffer_length_;
  int64_t max_bytes_to_read_;
  int64_t bytes_read_;
  scoped_refptr<net::IOBuffer> preloading_buffer_;
  int preloading_buffer_offset_;
  int preloaded_bytes_;

  base::WeakPtrFactory<BufferingFileStreamReader> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(BufferingFileStreamReader);
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_FILEAPI_BUFFERING_FILE_STREAM_READER_H_
