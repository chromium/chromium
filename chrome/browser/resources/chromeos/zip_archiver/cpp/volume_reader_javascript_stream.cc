// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resources/chromeos/zip_archiver/cpp/volume_reader_javascript_stream.h"

#include <algorithm>
#include <limits>

#include "base/files/file.h"
#include "chrome/browser/resources/chromeos/zip_archiver/cpp/javascript_requestor_interface.h"
#include "ppapi/cpp/logging.h"
#include "third_party/minizip/src/unzip.h"

VolumeReaderJavaScriptStream::VolumeReaderJavaScriptStream(
    int64_t archive_size,
    JavaScriptRequestorInterface* requestor)
    : archive_size_(archive_size),
      requestor_(requestor),
      available_data_(false),
      read_error_(false),
      passphrase_error_(false),
      available_data_cond_(&shared_state_lock_),
      available_passphrase_cond_(&shared_state_lock_),
      offset_(0),
      // For first call -1 will force a chunk request from JavaScript as offset
      // parameter is 0.
      last_read_chunk_offset_(-1),
      read_ahead_array_buffer_ptr_(&first_array_buffer_) {
  // Dummy Map the second buffer as first buffer is used for read ahead by
  // read_ahead_array_buffer_ptr_. This operation is required in order for Unmap
  // to correctly work in the destructor and VolumeReaderJavaScriptStream::Read.
  second_array_buffer_.Map();
}

VolumeReaderJavaScriptStream::~VolumeReaderJavaScriptStream() {
  // Unmap last mapped buffer. This is the other buffer to
  // read_ahead_array_buffer_ptr_ as read_ahead_array_buffer_ptr_ must be
  // available for SetBufferAndSignal to overwrite.
  if (read_ahead_array_buffer_ptr_ != &first_array_buffer_)
    first_array_buffer_.Unmap();
  else
    second_array_buffer_.Unmap();
}

int64_t VolumeReaderJavaScriptStream::offset() {
  return offset_;
}

int64_t VolumeReaderJavaScriptStream::archive_size() {
  return archive_size_;
}

void VolumeReaderJavaScriptStream::SetBufferAndSignal(
    const pp::VarArrayBuffer& array_buffer,
    int64_t read_offset) {
  PP_DCHECK(read_offset >= 0);

  // Ignore read ahead in case offset was changed using Skip or Seek and in case
  // we already have available data. This can happen in case of 2+ RequestChunk
  // calls done in parallel as a result of calling Read, Skip and Seek one after
  // another really fast. The usage of the buffer is not guarded so in case we
  // overwrite *read_ahead_array_buffer_ptr_ we will end up with memory
  // corruption.
  // In case read_offset and offset_ are different, then the read ahead data is
  // not valid anymore, but in case they are equal and available_data_ is set to
  // true then the second read ahead data is the same as the first read ahead
  // data so we can just ignore it.

  // TODO(mtomasz): We don't need to discard everything. Sometimes part of the
  // buffer can still be used. In such case we should use it. That can greatly
  // improve traversing headers for archives with small files!

  base::AutoLock al(shared_state_lock_);
  if (read_offset == offset_ && !available_data_ && !read_error_) {
    // Signal VolumeReaderJavaScriptStream::Read to continue execution. Copies
    // buffer locally so minizip has the buffer in memory when working with
    // it. Though we acquire a lock here this call is blocking only for a few
    // moments as VolumeReaderJavaScriptStream::Read will release the lock with
    // pthread_cond_wait. So we cannot arrive at a deadlock that will block the
    // main thread.

    *read_ahead_array_buffer_ptr_ = array_buffer;  // Copy operation.
    available_data_ = true;

    available_data_cond_.Signal();
  }
}

void VolumeReaderJavaScriptStream::ReadErrorSignal() {
  base::AutoLock al(shared_state_lock_);
  read_error_ = true;  // Read error from JavaScript.
  available_data_cond_.Signal();
}

void VolumeReaderJavaScriptStream::SetPassphraseAndSignal(
    const std::string& passphrase) {
  base::AutoLock al(shared_state_lock_);
  // Signal VolumeReaderJavaScriptStream::Passphrase to continue execution.
  available_passphrase_ = passphrase;
  available_passphrase_cond_.Signal();
}

void VolumeReaderJavaScriptStream::PassphraseErrorSignal() {
  base::AutoLock al(shared_state_lock_);
  passphrase_error_ = true;  // Passphrase error from JavaScript.
  available_passphrase_cond_.Signal();
}

int64_t VolumeReaderJavaScriptStream::Read(int64_t bytes_to_read,
                                           const void** destination_buffer) {
  PP_DCHECK(bytes_to_read > 0);

  base::AutoLock al(shared_state_lock_);

  // No more data, so signal end of reading.
  if (offset_ >= archive_size_) {
    return 0;
  }

  // Call in case of first read or read after Seek and Skip.
  if (last_read_chunk_offset_ != offset_ || !available_data_)
    RequestChunk(bytes_to_read);

  if (!available_data_) {
    // Wait for data from JavaScript.
    while (!available_data_) {  // Check again available data as first call
                                // was done outside guarded zone.
      if (read_error_) {
        return -1;
      }
      available_data_cond_.Wait();
    }
  }

  if (read_error_) {  // Read ahead failed.
    return -1;
  }

  // Make data available for minizip custom read. No need to lock this part.
  // The reason is that VolumeReaderJavaScriptStream::RequestChunk is the only
  // function that can set available_data_ back to false and let
  // VolumeReaderJavaScriptStream::SetBufferAndSignal overwrite the buffer. But
  // reading ahead is done only at the end of this function after the buffers
  // are switched.
  *destination_buffer = read_ahead_array_buffer_ptr_->Map();
  int64_t bytes_read =
      std::min(static_cast<int64_t>(read_ahead_array_buffer_ptr_->ByteLength()),
               bytes_to_read);

  offset_ += bytes_read;
  last_read_chunk_offset_ = offset_;

  // Ask for more data from JavaScript in the other buffer. This is the only
  // time when we switch buffers. The reason is that minizip must
  // always work on valid data and that data must be available until next
  // VolumeReaderJavaScriptStream::Read call, and as the data can be received
  // at any time from JavaScript, we need a buffer to store it in case of
  // reading ahead.
  read_ahead_array_buffer_ptr_ =
      read_ahead_array_buffer_ptr_ != &first_array_buffer_
          ? &first_array_buffer_
          : &second_array_buffer_;

  // Unmap old buffer. Only Read and constructor can Map the buffers so Read and
  // destructor should be the one to Unmap them. This will work because it is
  // called before RequestChunk which is the only method that overwrites the
  // buffer. The constructor should also Map a default pp::VarArrayBuffer and
  // destructor Unmap the last used array buffer (which is the other buffer than
  // read_ahead_array_buffer_ptr_). Unfortunately it's not clear from the
  // API description if this call is done automatically on pp::VarArrayBuffer
  // destructor.
  read_ahead_array_buffer_ptr_->Unmap();

  // Read ahead next chunk with a length similar to current read.
  RequestChunk(bytes_to_read);

  return bytes_read;
}

int64_t VolumeReaderJavaScriptStream::Seek(int64_t offset,
                                           base::File::Whence whence) {
  base::AutoLock al(shared_state_lock_);

  int64_t new_offset = offset_;
  switch (whence) {
    case base::File::FROM_BEGIN:
      new_offset = offset;
      break;
    case base::File::FROM_CURRENT:
      new_offset += offset;
      break;
    case base::File::FROM_END:
      new_offset = archive_size_ + offset;
      break;
    default:
      PP_NOTREACHED();
      return -1;
  }

  if (new_offset < 0) {
    return -1;
  }

  offset_ = new_offset;

  return new_offset;
}

void VolumeReaderJavaScriptStream::SetRequestId(const std::string& request_id) {
  // No lock necessary, as request_id is used by one thread only.
  request_id_ = request_id;
}

std::unique_ptr<std::string> VolumeReaderJavaScriptStream::Passphrase() {
  std::unique_ptr<std::string> result;
  // The error is not recoverable. Once passphrase fails to be provided, it is
  // never asked again. Note, that still users are able to retry entering the
  // password, unless they click Cancel.
  {
    base::AutoLock al(shared_state_lock_);
    if (passphrase_error_) {
      return result;
    }
  }

  // Request the passphrase outside of the lock.
  requestor_->RequestPassphrase(request_id_);

  base::AutoLock al(shared_state_lock_);
  // Wait for the passphrase from JavaScript.
  // TODO(amistry): Handle spurious wakeups.
  available_passphrase_cond_.Wait();

  if (!passphrase_error_)
    result.reset(new std::string(available_passphrase_));

  return result;
}

void VolumeReaderJavaScriptStream::RequestChunk(int64_t length) {
  // Read next chunk only if not at the end of archive.
  if (archive_size_ <= offset_)
    return;

  int64_t bytes_to_read =
      std::min(length, archive_size_ - offset_ /* Positive check above. */);
  available_data_ = false;

  requestor_->RequestFileChunk(request_id_, offset_, bytes_to_read);
}
