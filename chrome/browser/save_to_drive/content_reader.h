// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAVE_TO_DRIVE_CONTENT_READER_H_
#define CHROME_BROWSER_SAVE_TO_DRIVE_CONTENT_READER_H_

#include <cstdint>

#include "base/functional/callback.h"

namespace mojo_base {
class BigBuffer;
}  // namespace mojo_base

namespace save_to_drive {

// Callback for reading the content of a file. An empty `buffer` indicates that
// there was an error reading the content.
using ContentReadCallback =
    base::OnceCallback<void(mojo_base::BigBuffer buffer)>;

// Callback for opening a file for reading.
using OpenCallback = base::OnceCallback<void(bool success)>;

// Interface for reading the content of a file. This is used by the Save to
// Drive flow to read the file content. The implementation of this interface
// should be able to read the content of the file in chunks.
class ContentReader {
 public:
  ContentReader() = default;
  virtual ~ContentReader() = default;

  // Opens a file for reading. Calls the `callback` with true if the file was
  // opened successfully, false otherwise.
  virtual void Open(OpenCallback callback) = 0;

  // Returns the size of the file. This is only valid after the file is opened
  // successfully.
  virtual size_t GetSize() = 0;

  // Reads the content of the file with the given `offset` and `size`.
  virtual void Read(uint32_t offset,
                    uint32_t size,
                    ContentReadCallback callback) = 0;

  // Closes the file.
  virtual void Close() = 0;
};

}  // namespace save_to_drive

#endif  // CHROME_BROWSER_SAVE_TO_DRIVE_CONTENT_READER_H_
