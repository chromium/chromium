// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sync_socket.h"

#include <limits.h>
#include <stddef.h>

#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/rand_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/scoped_handle.h"

namespace base {

using win::ScopedHandle;

namespace {
// IMPORTANT: do not change how this name is generated because it will break
// in sandboxed scenarios as we might have by-name policies that allow pipe
// creation. Also keep the secure random number generation.
const wchar_t kPipeNameFormat[] = L"\\\\.\\pipe\\chrome.sync.%u.%u.%lu";
const size_t kPipePathMax = std::size(kPipeNameFormat) + (3 * 10) + 1;

// To avoid users sending negative message lengths to Send/Receive
// we clamp message lengths, which are size_t, to no more than INT_MAX.
const size_t kMaxMessageLength = static_cast<size_t>(INT_MAX);

const int kOutBufferSize = 4096;
const int kInBufferSize = 4096;
const int kDefaultTimeoutMilliSeconds = 1000;

bool CreatePairImpl(ScopedHandle* socket_a,
                    ScopedHandle* socket_b,
                    bool overlapped) {
  DCHECK_NE(socket_a, socket_b);
  DCHECK(!socket_a->is_valid());
  DCHECK(!socket_b->is_valid());

  wchar_t name[kPipePathMax];
  ScopedHandle handle_a;
  DWORD flags = PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE;
  if (overlapped)
    flags |= FILE_FLAG_OVERLAPPED;

  do {
    unsigned long rnd_name;
    RandBytes(byte_span_from_ref(rnd_name));

    swprintf(name, kPipePathMax,
             kPipeNameFormat,
             GetCurrentProcessId(),
             GetCurrentThreadId(),
             rnd_name);

    handle_a.Set(CreateNamedPipeW(
        name,
        flags,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
        1,
        kOutBufferSize,
        kInBufferSize,
        kDefaultTimeoutMilliSeconds,
        NULL));
  } while (!handle_a.is_valid() && (GetLastError() == ERROR_PIPE_BUSY));

  CHECK(handle_a.is_valid());

  // The SECURITY_ANONYMOUS flag means that the server side (handle_a) cannot
  // impersonate the client (handle_b). This allows us not to care which side
  // ends up in which side of a privilege boundary.
  flags = SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS;
  if (overlapped)
    flags |= FILE_FLAG_OVERLAPPED;

  ScopedHandle handle_b(CreateFileW(name,
                                    GENERIC_READ | GENERIC_WRITE,
                                    0,          // no sharing.
                                    NULL,       // default security attributes.
                                    OPEN_EXISTING,  // opens existing pipe.
                                    flags,
                                    NULL));     // no template file.
  if (!handle_b.is_valid()) {
    DPLOG(ERROR) << "CreateFileW failed";
    return false;
  }

  if (!ConnectNamedPipe(handle_a.get(), NULL)) {
    DWORD error = GetLastError();
    if (error != ERROR_PIPE_CONNECTED) {
      DPLOG(ERROR) << "ConnectNamedPipe failed";
      return false;
    }
  }

  *socket_a = std::move(handle_a);
  *socket_b = std::move(handle_b);

  return true;
}

// Inline helper to avoid having the cast everywhere.
DWORD GetNextChunkSize(size_t current_pos, size_t max_size) {
  // The following statement is for 64 bit portability.
  return static_cast<DWORD>(((max_size - current_pos) <= UINT_MAX) ?
      (max_size - current_pos) : UINT_MAX);
}

// Template function that supports calling ReadFile or WriteFile in an
// overlapped fashion and waits for IO completion.  The function also waits
// on an event that can be used to cancel the operation.  If the operation
// is cancelled, the function returns and closes the relevant socket object.
template <typename DataType, typename Function>
size_t CancelableFileOperation(Function operation,
                               HANDLE file,
                               span<DataType> buffer,
                               WaitableEvent* io_event,
                               WaitableEvent* cancel_event,
                               CancelableSyncSocket* socket,
                               DWORD timeout_in_ms) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  // The buffer must be byte size or the length check won't make much sense.
  static_assert(sizeof(DataType) == 1u, "incorrect buffer type");
  CHECK(!buffer.empty());
  CHECK_LE(buffer.size(), kMaxMessageLength);
  CHECK_NE(file, SyncSocket::kInvalidHandle);

  // Track the finish time so we can calculate the timeout as data is read.
  TimeTicks current_time, finish_time;
  if (timeout_in_ms != INFINITE) {
    current_time = TimeTicks::Now();
    finish_time = current_time + base::Milliseconds(timeout_in_ms);
  }

  size_t count = 0;
  do {
    // The OVERLAPPED structure will be modified by ReadFile or WriteFile.
    OVERLAPPED ol = { 0 };
    ol.hEvent = io_event->handle();

    const DWORD chunk_size = GetNextChunkSize(count, buffer.size());
    // This is either the ReadFile or WriteFile call depending on whether
    // we're receiving or sending data.
    DWORD len = 0;
    auto operation_buffer = buffer.subspan(count, chunk_size);
    // SAFETY: The below static_cast is in range for DWORD because
    // `operation_buffer` is constructed with a DWORD length above from
    // `chunk_size`.
    const BOOL operation_ok =
        operation(file, operation_buffer.data(),
                  static_cast<DWORD>(operation_buffer.size()), &len, &ol);
    if (!operation_ok) {
      if (::GetLastError() == ERROR_IO_PENDING) {
        HANDLE events[] = { io_event->handle(), cancel_event->handle() };
        const DWORD wait_result = WaitForMultipleObjects(
            std::size(events), events, FALSE,
            timeout_in_ms == INFINITE
                ? timeout_in_ms
                : static_cast<DWORD>(
                      (finish_time - current_time).InMilliseconds()));
        if (wait_result != WAIT_OBJECT_0 + 0) {
          // CancelIo() doesn't synchronously cancel outstanding IO, only marks
          // outstanding IO for cancellation. We must call GetOverlappedResult()
          // below to ensure in flight writes complete before returning.
          CancelIo(file);
        }

        // We set the |bWait| parameter to TRUE for GetOverlappedResult() to
        // ensure writes are complete before returning.
        if (!GetOverlappedResult(file, &ol, &len, TRUE))
          len = 0;

        if (wait_result == WAIT_OBJECT_0 + 1) {
          DVLOG(1) << "Shutdown was signaled. Closing socket.";
          socket->Close();
          return count;
        }

        // Timeouts will be handled by the while() condition below since
        // GetOverlappedResult() may complete successfully after CancelIo().
        DCHECK(wait_result == WAIT_OBJECT_0 + 0 || wait_result == WAIT_TIMEOUT);
      } else {
        break;
      }
    }

    count += len;

    // Quit the operation if we can't write/read anymore.
    if (len != chunk_size) {
      break;
    }

    // Since TimeTicks::Now() is expensive, only bother updating the time if we
    // have more work to do.
    if (timeout_in_ms != INFINITE && count < buffer.size()) {
      current_time = base::TimeTicks::Now();
    }
  } while (count < buffer.size() &&
           (timeout_in_ms == INFINITE || current_time < finish_time));

  return count;
}

}  // namespace

// static
bool SyncSocket::CreatePair(SyncSocket* socket_a, SyncSocket* socket_b) {
  return CreatePairImpl(&socket_a->handle_, &socket_b->handle_, false);
}

void SyncSocket::Close() {
  handle_.Close();
}

size_t SyncSocket::Send(span<const uint8_t> data) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  CHECK_LE(data.size(), kMaxMessageLength);
  DCHECK(IsValid());
  size_t count = 0;
  while (count < data.size()) {
    DWORD len;
    const DWORD chunk_size = GetNextChunkSize(count, data.size());
    auto data_chunk = data.subspan(count, chunk_size);
    // SAFETY: The below static_cast is in range for DWORD because `data_chunk`
    // is constructed with a DWORD length above from `chunk_size`.
    if (::WriteFile(handle(), data_chunk.data(),
                    static_cast<DWORD>(data_chunk.size()), &len,
                    NULL) == FALSE) {
      return count;
    }
    count += len;
  }
  return count;
}

size_t SyncSocket::ReceiveWithTimeout(span<uint8_t> buffer, TimeDelta timeout) {
  NOTIMPLEMENTED();
  return 0;
}

size_t SyncSocket::Receive(span<uint8_t> buffer) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  CHECK_LE(buffer.size(), kMaxMessageLength);
  DCHECK(IsValid());
  size_t count = 0;
  while (count < buffer.size()) {
    DWORD len;
    const DWORD chunk_size = GetNextChunkSize(count, buffer.size());
    auto data_chunk = buffer.subspan(count, chunk_size);
    // SAFETY: The below static_cast is in range for DWORD because `data_chunk`
    // is constructed with a DWORD length above from `chunk_size`.
    if (::ReadFile(handle(), data_chunk.data(),
                   static_cast<DWORD>(data_chunk.size()), &len,
                   NULL) == FALSE) {
      return count;
    }
    count += len;
  }
  return count;
}

size_t SyncSocket::Peek() {
  DWORD available = 0;
  PeekNamedPipe(handle(), NULL, 0, NULL, &available, NULL);
  return available;
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

bool CancelableSyncSocket::Shutdown() {
  // This doesn't shut down the pipe immediately, but subsequent Receive or Send
  // methods will fail straight away.
  shutdown_event_.Signal();
  return true;
}

void CancelableSyncSocket::Close() {
  SyncSocket::Close();
  shutdown_event_.Reset();
}

size_t CancelableSyncSocket::Send(span<const uint8_t> data) {
  static const DWORD kWaitTimeOutInMs = 500;
  return CancelableFileOperation(&::WriteFile, handle(), data, &file_operation_,
                                 &shutdown_event_, this, kWaitTimeOutInMs);
}

size_t CancelableSyncSocket::Receive(span<uint8_t> buffer) {
  return CancelableFileOperation(&::ReadFile, handle(), buffer,
                                 &file_operation_, &shutdown_event_, this,
                                 INFINITE);
}

size_t CancelableSyncSocket::ReceiveWithTimeout(span<uint8_t> buffer,
                                                TimeDelta timeout) {
  return CancelableFileOperation(&::ReadFile, handle(), buffer,
                                 &file_operation_, &shutdown_event_, this,
                                 static_cast<DWORD>(timeout.InMilliseconds()));
}

// static
bool CancelableSyncSocket::CreatePair(CancelableSyncSocket* socket_a,
                                      CancelableSyncSocket* socket_b) {
  return CreatePairImpl(&socket_a->handle_, &socket_b->handle_, true);
}

}  // namespace base
