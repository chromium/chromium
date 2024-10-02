// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file.h"

#include <utility>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_tracing.h"
#include "base/metrics/histogram.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include <errno.h>
#endif

namespace base {

File::Info::Info() = default;

File::Info::~Info() = default;

File::File() = default;

#if !BUILDFLAG(IS_NACL)
File::File(const FilePath& path, uint32_t flags) : error_details_(FILE_OK) {
  Initialize(path, flags);
}
#endif

File::File(ScopedPlatformFile platform_file)
    : File(std::move(platform_file), false) {}

File::File(PlatformFile platform_file) : File(platform_file, false) {}

File::File(ScopedPlatformFile platform_file, bool async)
    : file_(std::move(platform_file)), error_details_(FILE_OK), async_(async) {
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  DCHECK_GE(file_.get(), -1);
#endif
}

File::File(PlatformFile platform_file, bool async)
    : file_(platform_file),
      error_details_(FILE_OK),
      async_(async) {
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  DCHECK_GE(platform_file, -1);
#endif
}

File::File(Error error_details) : error_details_(error_details) {}

File::File(File&& other)
    : file_(other.TakePlatformFile()),
      path_(other.path_),
      error_details_(other.error_details()),
      created_(other.created()),
      async_(other.async_) {}

File::~File() {
  // Go through the AssertIOAllowed logic.
  Close();
}

File& File::operator=(File&& other) {
  Close();
  SetPlatformFile(other.TakePlatformFile());
  path_ = other.path_;
  error_details_ = other.error_details();
  created_ = other.created();
  async_ = other.async_;
  return *this;
}

#if !BUILDFLAG(IS_NACL)
void File::Initialize(const FilePath& path, uint32_t flags) {
  if (path.ReferencesParent()) {
#if BUILDFLAG(IS_WIN)
    ::SetLastError(ERROR_ACCESS_DENIED);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
    errno = EACCES;
#else
#error Unsupported platform
#endif
    error_details_ = FILE_ERROR_ACCESS_DENIED;
    return;
  }
  if (FileTracing::IsCategoryEnabled()
#if BUILDFLAG(IS_ANDROID)
      || path.IsContentUri()
#endif
  ) {
    path_ = path;
  }
  SCOPED_FILE_TRACE("Initialize");
  DoInitialize(path, flags);
}
#endif

std::optional<size_t> File::Read(int64_t offset, span<uint8_t> data) {
  span<char> chars = base::as_writable_chars(data);
  int size = checked_cast<int>(chars.size());
  // SAFETY: `chars.size()` describes valid portion of `chars.data()`.
  int result = UNSAFE_BUFFERS(Read(offset, chars.data(), size));
  if (result < 0) {
    return std::nullopt;
  }
  return checked_cast<size_t>(result);
}

bool File::ReadAndCheck(int64_t offset, span<uint8_t> data) {
  // Size checked in span form of Read() above.
  return Read(offset, data) == static_cast<int>(data.size());
}

std::optional<size_t> File::ReadAtCurrentPos(span<uint8_t> data) {
  span<char> chars = base::as_writable_chars(data);
  int size = checked_cast<int>(chars.size());
  // SAFETY: `chars.size()` describes valid portion of `chars.data()`.
  int result = UNSAFE_BUFFERS(ReadAtCurrentPos(chars.data(), size));
  if (result < 0) {
    return std::nullopt;
  }
  return checked_cast<size_t>(result);
}

bool File::ReadAtCurrentPosAndCheck(span<uint8_t> data) {
  // Size checked in span form of ReadAtCurrentPos() above.
  return ReadAtCurrentPos(data) == static_cast<int>(data.size());
}

std::optional<size_t> File::Write(int64_t offset, span<const uint8_t> data) {
  span<const char> chars = base::as_chars(data);
  int size = checked_cast<int>(chars.size());
  // SAFETY: `chars.size()` describes valid portion of `chars.data()`.
  int result = UNSAFE_BUFFERS(Write(offset, chars.data(), size));
  if (result < 0) {
    return std::nullopt;
  }
  return checked_cast<size_t>(result);
}

bool File::WriteAndCheck(int64_t offset, span<const uint8_t> data) {
  // Size checked in span form of Write() above.
  return Write(offset, data) == static_cast<int>(data.size());
}

std::optional<size_t> File::WriteAtCurrentPos(span<const uint8_t> data) {
  span<const char> chars = base::as_chars(data);
  int size = checked_cast<int>(chars.size());
  // SAFETY: `chars.size()` describes valid portion of `chars.data()`.
  int result = UNSAFE_BUFFERS(WriteAtCurrentPos(chars.data(), size));
  if (result < 0) {
    return std::nullopt;
  }
  return checked_cast<size_t>(result);
}

bool File::WriteAtCurrentPosAndCheck(span<const uint8_t> data) {
  // Size checked in span form of WriteAtCurrentPos() above.
  return WriteAtCurrentPos(data) == static_cast<int>(data.size());
}

std::optional<size_t> File::ReadNoBestEffort(int64_t offset,
                                             base::span<uint8_t> data) {
  span<char> chars = base::as_writable_chars(data);
  int size = checked_cast<int>(chars.size());
  // SAFETY: `chars.size()` describes valid portion of `chars.data()`.
  int result = UNSAFE_BUFFERS(ReadNoBestEffort(offset, chars.data(), size));
  if (result < 0) {
    return std::nullopt;
  }
  return checked_cast<size_t>(result);
}

std::optional<size_t> File::ReadAtCurrentPosNoBestEffort(
    base::span<uint8_t> data) {
  span<char> chars = base::as_writable_chars(data);
  int size = checked_cast<int>(chars.size());
  // SAFETY: `chars.size()` describes valid portion of `chars.data()`.
  int result = UNSAFE_BUFFERS(ReadAtCurrentPosNoBestEffort(chars.data(), size));
  if (result < 0) {
    return std::nullopt;
  }
  return checked_cast<size_t>(result);
}

std::optional<size_t> File::WriteAtCurrentPosNoBestEffort(
    base::span<const uint8_t> data) {
  span<const char> chars = base::as_chars(data);
  int size = checked_cast<int>(chars.size());
  // SAFETY: `chars.size()` describes valid portion of `chars.data()`.
  int result =
      UNSAFE_BUFFERS(WriteAtCurrentPosNoBestEffort(chars.data(), size));
  if (result < 0) {
    return std::nullopt;
  }
  return checked_cast<size_t>(result);
}

// static
std::string File::ErrorToString(Error error) {
  switch (error) {
    case FILE_OK:
      return "FILE_OK";
    case FILE_ERROR_FAILED:
      return "FILE_ERROR_FAILED";
    case FILE_ERROR_IN_USE:
      return "FILE_ERROR_IN_USE";
    case FILE_ERROR_EXISTS:
      return "FILE_ERROR_EXISTS";
    case FILE_ERROR_NOT_FOUND:
      return "FILE_ERROR_NOT_FOUND";
    case FILE_ERROR_ACCESS_DENIED:
      return "FILE_ERROR_ACCESS_DENIED";
    case FILE_ERROR_TOO_MANY_OPENED:
      return "FILE_ERROR_TOO_MANY_OPENED";
    case FILE_ERROR_NO_MEMORY:
      return "FILE_ERROR_NO_MEMORY";
    case FILE_ERROR_NO_SPACE:
      return "FILE_ERROR_NO_SPACE";
    case FILE_ERROR_NOT_A_DIRECTORY:
      return "FILE_ERROR_NOT_A_DIRECTORY";
    case FILE_ERROR_INVALID_OPERATION:
      return "FILE_ERROR_INVALID_OPERATION";
    case FILE_ERROR_SECURITY:
      return "FILE_ERROR_SECURITY";
    case FILE_ERROR_ABORT:
      return "FILE_ERROR_ABORT";
    case FILE_ERROR_NOT_A_FILE:
      return "FILE_ERROR_NOT_A_FILE";
    case FILE_ERROR_NOT_EMPTY:
      return "FILE_ERROR_NOT_EMPTY";
    case FILE_ERROR_INVALID_URL:
      return "FILE_ERROR_INVALID_URL";
    case FILE_ERROR_IO:
      return "FILE_ERROR_IO";
    case FILE_ERROR_MAX:
      break;
  }

  NOTREACHED();
}

void File::WriteIntoTrace(perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("is_valid", IsValid());
  dict.Add("created", created_);
  dict.Add("async", async_);
  dict.Add("error_details", ErrorToString(error_details_));
}

}  // namespace base
