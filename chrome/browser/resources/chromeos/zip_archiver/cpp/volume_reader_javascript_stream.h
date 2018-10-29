// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_VOLUME_READER_JAVASCRIPT_STREAM_H_
#define CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_VOLUME_READER_JAVASCRIPT_STREAM_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/resources/chromeos/zip_archiver/cpp/volume_reader.h"
#include "ppapi/cpp/var_array_buffer.h"

class JavaScriptRequestorInterface;

// A VolumeReader that reads the content of the volume's archive from
// JavaScript. All methods including the constructor and destructor should be
// called from the same thread with the exception of SetBufferAndSignal and
// ReadErrorSignal which MUST be called from another thread.
class VolumeReaderJavaScriptStream : public VolumeReader {
 public:
  // archive_size is used by Seek method in order to seek from volume's
  // archive end.
  // requestor is used to request more data from JavaScript.
  VolumeReaderJavaScriptStream(int64_t archive_size,
                               JavaScriptRequestorInterface* requestor);

  ~VolumeReaderJavaScriptStream() override;

  // Sets the internal array buffer used for reads and signal the blocked
  // VolumeReaderJavaScriptStream::Read to continue execution. Must be done in
  // a different thread from VolumeReaderJavaScriptStream::Read method.
  // read_offset represents the offset from which VolumeReaderJavaScriptStream
  // requested a chunk read from JavaScriptRequestorInterface. May block for a
  // few cycles in order to synchronize with VolumeReaderJavaScriptStream::Read.
  void SetBufferAndSignal(const pp::VarArrayBuffer& array_buffer,
                          int64_t read_offset);

  // Signal the blocked VolumeReaderJavaScriptStream::Read to continue execution
  // and return an error code. Must be called from a different thread than
  // VolumeReaderJavaScriptStream::Read. May block for a few cycles in order
  // to synchronize with VolumeReaderJavaScriptStream::Read.
  void ReadErrorSignal();

  // Sets the passphrase and signals the blocked Passphrase() to continue
  // execution. Must be done in a different thread from Passphrase() method.
  // Reporting an error is not supported. Returning an empty string indicates
  // an error.
  void SetPassphraseAndSignal(const std::string& passphrase);

  // Signal the blocked VolumeReaderJavaScriptStream::Passphrase to continue
  // execution and return an error code. Must be called from a different thread
  // than VolumeReaderJavaScriptStream::Passphrase. May block for a few cycles
  // in order to synchronize with VolumeReaderJavaScriptStream::Passphrase.
  void PassphraseErrorSignal();

  // See volume_reader.h for description. This method blocks on
  // available_data_cond_. SetBufferAndSignal should unblock it from another
  // thread.
  int64_t Read(int64_t bytes_to_read, const void** destination_buffer) override;

  // See volume_reader.h for description.
  int64_t Seek(int64_t offset, base::File::Whence whence) override;

  // Sets the request Id to be used by the reader.
  void SetRequestId(const std::string& request_id);

  // See volume_reader.h for description. The method blocks on
  // available_passphrase_cond_. SetPassphraseAndSignal should unblock it from
  // another thread.
  std::unique_ptr<std::string> Passphrase() override;

  int64_t offset() override;

  int64_t archive_size() override;

 private:
  // Request a chunk of length number of bytes from JavaScript starting from
  // offset_ member. Should be run within a lock.
  void RequestChunk(int64_t length);

  std::string request_id_;      // The request id for which the reader was
                                // created.
  const int64_t archive_size_;  // The archive size.

  // A requestor that makes calls to JavaScript to obtain file chunks.
  JavaScriptRequestorInterface* requestor_;

  bool available_data_;  // Indicates whether any data is available.
  bool read_error_;      // Marks an error in reading from JavaScript.

  std::string available_passphrase_;  // Stores a passphrase from JavaScript.
  bool passphrase_error_;  // Marks an error in getting the passphrase.

  // The shared_state_lock_ is used to protect members which are accessed by
  // more than one thread.
  base::Lock shared_state_lock_;
  base::ConditionVariable available_data_cond_;
  base::ConditionVariable available_passphrase_cond_;

  int64_t offset_;  // The offset from where read should be done.
  int64_t last_read_chunk_offset_;  // The offset reached after last call to
                                    // VolumeReaderJavaScriptStream::Read.

  // Two buffers used to store the actual data used by minizip and the data
  // read ahead.
  pp::VarArrayBuffer first_array_buffer_;
  pp::VarArrayBuffer second_array_buffer_;

  // A pointer to first_arrray_buffer_ or second_array_buffer_. This is used in
  // order to avoid an extra copy from the second buffer to the first buffer
  // when data is available for VolumeReaderJavaScriptStream::Read method.
  // It points to the array buffer used for reading ahead when data is received
  // from JavaScript at VolumeReaderJavaScriptStream::SetBufferAndSignal.
  pp::VarArrayBuffer* read_ahead_array_buffer_ptr_;
};

#endif  // CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_VOLUME_READER_JAVASCRIPT_STREAM_H_
