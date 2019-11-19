// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_READAHEAD_FILE_STREAM_READER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_READAHEAD_FILE_STREAM_READER_H_

#include <stdint.h>

#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "storage/browser/file_system/file_stream_reader.h"

// Wraps a source FileStreamReader with a readahead buffer.
class ReadaheadFileStreamReader : public storage::FileStreamReader {
 public:
  // Takes ownership of |source|.
  explicit ReadaheadFileStreamReader(storage::FileStreamReader* source);

  ~ReadaheadFileStreamReader() override;

  // FileStreamReader overrides.
  int Read(net::IOBuffer* buf,
           int buf_len,
           net::CompletionOnceCallback callback) override;
  int64_t GetLength(net::Int64CompletionOnceCallback callback) override;

 private:
  // Returns the number of bytes consumed from the internal cache into |sink|.
  // Returns an error code if we are out of cache, hit an error, or hit EOF.
  int FinishReadFromCacheOrStoredError(net::DrainableIOBuffer* sink);

  // Reads into a new buffer from the source reader. This calls
  // OnFinishReadFromSource when it completes (either synchronously or
  // asynchronously).
  void ReadFromSourceIfNeeded();
  void OnFinishReadFromSource(net::IOBuffer* buffer, int result);

  // This is reset to NULL upon encountering a read error or EOF.
  std::unique_ptr<storage::FileStreamReader> source_;

  // This stores the error or EOF from the source FileStreamReader. Its
  // value is undefined if |source_| is non-NULL.
  int source_error_;
  bool source_has_pending_read_;

  // This contains a queue of buffers filled from |source_|, waiting to be
  // consumed.
  base::queue<scoped_refptr<net::DrainableIOBuffer>> buffers_;

  // The read buffer waiting for the source FileStreamReader to finish
  // reading and fill the cache.
  scoped_refptr<net::DrainableIOBuffer> pending_sink_buffer_;
  net::CompletionOnceCallback pending_read_callback_;

  base::WeakPtrFactory<ReadaheadFileStreamReader> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ReadaheadFileStreamReader);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_READAHEAD_FILE_STREAM_READER_H_
