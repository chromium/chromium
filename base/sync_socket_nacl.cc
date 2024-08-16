// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sync_socket.h"

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/containers/span.h"
#include "base/notimplemented.h"

namespace base {

// static
bool SyncSocket::CreatePair(SyncSocket* socket_a, SyncSocket* socket_b) {
  return false;
}

void SyncSocket::Close() {
  handle_.reset();
}

size_t SyncSocket::Send(span<const uint8_t> data) {
  const ssize_t bytes_written = write(handle(), data.data(), data.size());
  return bytes_written > 0 ? static_cast<size_t>(bytes_written) : 0;
}

size_t SyncSocket::Receive(span<uint8_t> buffer) {
  const ssize_t bytes_read = read(handle(), buffer.data(), buffer.size());
  return bytes_read > 0 ? static_cast<size_t>(bytes_read) : 0;
}

size_t SyncSocket::ReceiveWithTimeout(span<uint8_t> buffer, TimeDelta timeout) {
  NOTIMPLEMENTED();
  return 0;
}

size_t SyncSocket::Peek() {
  NOTIMPLEMENTED();
  return 0;
}

bool SyncSocket::IsValid() const {
  return handle_.is_valid();
}

SyncSocket::Handle SyncSocket::handle() const {
  return handle_.get();
}

SyncSocket::Handle SyncSocket::Release() {
  return handle_.release();
}

size_t CancelableSyncSocket::Send(span<const uint8_t> data) {
  return SyncSocket::Send(data);
}

bool CancelableSyncSocket::Shutdown() {
  Close();
  return true;
}

// static
bool CancelableSyncSocket::CreatePair(CancelableSyncSocket* socket_a,
                                      CancelableSyncSocket* socket_b) {
  return SyncSocket::CreatePair(socket_a, socket_b);
}

}  // namespace base
