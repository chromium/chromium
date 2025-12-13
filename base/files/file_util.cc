// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"

#include <algorithm>
#include <string_view>

#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <io.h>
#endif
#include <stdio.h>

#include <algorithm>
#include <fstream>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/bit_cast.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/functional/function_ref.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/threading/scoped_blocking_call.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace base {

namespace {

#if !BUILDFLAG(IS_WIN)

void RunAndReply(OnceCallback<bool()> action_callback,
                 OnceCallback<void(bool)> reply_callback) {
  bool result = std::move(action_callback).Run();
  if (!reply_callback.is_null()) {
    std::move(reply_callback).Run(result);
  }
}

#endif  // !BUILDFLAG(IS_WIN)

bool ReadStreamToSpanWithMaxSize(
    FILE* stream,
    size_t max_size,
    FunctionRef<span<uint8_t>(size_t)> resize_span) {
  if (!stream) {
    return false;
  }

  // Seeking to the beginning is best-effort -- it is expected to fail for
  // certain non-file stream (e.g., pipes).
  HANDLE_EINTR(fseek(stream, 0, SEEK_SET));

  // Many files have incorrect size (proc files etc). Hence, the file is read
  // sequentially as opposed to a one-shot read, using file size as a hint for
  // chunk size if available.
  constexpr size_t kDefaultChunkSize = 1 << 16;
  size_t chunk_size = kDefaultChunkSize - 1;
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
#if BUILDFLAG(IS_WIN)
  BY_HANDLE_FILE_INFORMATION file_info = {};
  if (::GetFileInformationByHandle(
          reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stream))),
          &file_info)) {
    LARGE_INTEGER size;
    size.HighPart = static_cast<LONG>(file_info.nFileSizeHigh);
    size.LowPart = file_info.nFileSizeLow;
    if (size.QuadPart > 0) {
      chunk_size = static_cast<size_t>(size.QuadPart);
    }
  }
#else   // BUILDFLAG(IS_WIN)
  // In cases where the reported file size is 0, use a smaller chunk size to
  // minimize memory allocated and cost of string::resize() in case the read
  // size is small (i.e. proc files). If the file is larger than this, the read
  // loop will reset |chunk_size| to kDefaultChunkSize.
  constexpr size_t kSmallChunkSize = 4096;
  chunk_size = kSmallChunkSize - 1;
  stat_wrapper_t file_info = {};
  if (!File::Fstat(fileno(stream), &file_info) && file_info.st_size > 0) {
    chunk_size = static_cast<size_t>(file_info.st_size);
  }
#endif  // BUILDFLAG(IS_WIN)

  // We need to attempt to read at EOF for feof flag to be set so here we use
  // |chunk_size| + 1.
  chunk_size = std::min(chunk_size, max_size) + 1;
  size_t bytes_read_this_pass;
  size_t bytes_read_so_far = 0;
  bool read_status = true;
  span<uint8_t> bytes_span = resize_span(chunk_size);
  DCHECK_EQ(bytes_span.size(), chunk_size);

  // TODO(https://crbug.com/40284755): Replace `UNSAFE_TODO` with
  // `UNSAFE_BUFFERS` after replacing pointer arithmetic with `span::subspan`
  // and adding a comment explaining why `fread` calls are safe.
  while ((bytes_read_this_pass = UNSAFE_TODO(fread(
              bytes_span.data() + bytes_read_so_far, 1, chunk_size, stream))) >
         0) {
    if ((max_size - bytes_read_so_far) < bytes_read_this_pass) {
      // Read more than max_size bytes, bail out.
      bytes_read_so_far = max_size;
      read_status = false;
      break;
    }
    // In case EOF was not reached, iterate again but revert to the default
    // chunk size.
    if (bytes_read_so_far == 0) {
      chunk_size = kDefaultChunkSize;
    }

    bytes_read_so_far += bytes_read_this_pass;
    // Last fread syscall (after EOF) can be avoided via feof, which is just a
    // flag check.
    if (feof(stream)) {
      break;
    }
    bytes_span = resize_span(bytes_read_so_far + chunk_size);
    DCHECK_EQ(bytes_span.size(), bytes_read_so_far + chunk_size);
  }
  read_status = read_status && !ferror(stream);

  // Trim the container down to the number of bytes that were actually read.
  bytes_span = resize_span(bytes_read_so_far);
  DCHECK_EQ(bytes_span.size(), bytes_read_so_far);

  return read_status;
}

}  // namespace

#if !BUILDFLAG(IS_WIN)

OnceClosure GetDeleteFileCallback(const FilePath& path,
                                  OnceCallback<void(bool)> reply_callback) {
  return BindOnce(&RunAndReply, BindOnce(&DeleteFile, path),
                  reply_callback.is_null()
                      ? std::move(reply_callback)
                      : BindPostTask(SequencedTaskRunner::GetCurrentDefault(),
                                     std::move(reply_callback)));
}

OnceClosure GetDeletePathRecursivelyCallback(
    const FilePath& path,
    OnceCallback<void(bool)> reply_callback) {
  return BindOnce(&RunAndReply, BindOnce(&DeletePathRecursively, path),
                  reply_callback.is_null()
                      ? std::move(reply_callback)
                      : BindPostTask(SequencedTaskRunner::GetCurrentDefault(),
                                     std::move(reply_callback)));
}

#endif  // !BUILDFLAG(IS_WIN)

int64_t ComputeDirectorySize(const FilePath& root_path) {
  int64_t running_size = 0;
  FileEnumerator file_iter(root_path, true, FileEnumerator::FILES);
  while (!file_iter.Next().empty()) {
    running_size += file_iter.GetInfo().GetSize();
  }
  return running_size;
}

bool Move(const FilePath& from_path, const FilePath& to_path) {
  if (from_path.ReferencesParent() || to_path.ReferencesParent()) {
    return false;
  }
  return internal::MoveUnsafe(from_path, to_path);
}

bool CopyFileContents(File& infile, File& outfile) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  bool retry_slow = false;
  bool res =
      internal::CopyFileContentsWithSendfile(infile, outfile, retry_slow);
  if (res || !retry_slow) {
    return res;
  }
  // Any failures which allow retrying using read/write will not have modified
  // either file offset or size.
#endif

  static constexpr size_t kBufferSize = 32768;
  std::vector<uint8_t> buffer(kBufferSize);

  for (;;) {
    std::optional<size_t> bytes_read = infile.ReadAtCurrentPos(buffer);
    if (!bytes_read.has_value()) {
      return false;
    }
    if (bytes_read == 0) {
      return true;
    }
    // Allow for partial writes
    span<const uint8_t> bytes_to_write =
        as_byte_span(buffer).first(*bytes_read);
    do {
      std::optional<size_t> bytes_written =
          outfile.WriteAtCurrentPos(bytes_to_write);
      if (!bytes_written.has_value()) {
        return false;
      }

      bytes_to_write = bytes_to_write.subspan(*bytes_written);
    } while (!bytes_to_write.empty());
  }

  NOTREACHED();
}

bool ContentsEqual(const FilePath& filename1, const FilePath& filename2) {
  // We open the file in binary format even if they are text files because
  // we are just comparing that bytes are exactly same in both files and not
  // doing anything smart with text formatting.
#if BUILDFLAG(IS_WIN)
  std::ifstream file1(filename1.value().c_str(),
                      std::ios::in | std::ios::binary);
  std::ifstream file2(filename2.value().c_str(),
                      std::ios::in | std::ios::binary);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  std::ifstream file1(filename1.value(), std::ios::in | std::ios::binary);
  std::ifstream file2(filename2.value(), std::ios::in | std::ios::binary);
#endif  // BUILDFLAG(IS_WIN)

  // Even if both files aren't openable (and thus, in some sense, "equal"),
  // any unusable file yields a result of "false".
  if (!file1.is_open() || !file2.is_open()) {
    return false;
  }

  const int BUFFER_SIZE = 2056;
  char buffer1[BUFFER_SIZE], buffer2[BUFFER_SIZE];
  do {
    file1.read(buffer1, BUFFER_SIZE);
    file2.read(buffer2, BUFFER_SIZE);
    if ((file1.eof() != file2.eof()) || (file1.gcount() != file2.gcount())) {
      return false;
    }

    span<const uint8_t> data1 =
        as_byte_span(buffer1).first(checked_cast<size_t>(file1.gcount()));
    span<const uint8_t> data2 =
        as_byte_span(buffer2).first(checked_cast<size_t>(file2.gcount()));
    if (data1 != data2) {
      return false;
    }
  } while (!file1.eof() || !file2.eof());

  return true;
}

bool TextContentsEqual(const FilePath& filename1, const FilePath& filename2) {
#if BUILDFLAG(IS_WIN)
  std::ifstream file1(filename1.value().c_str(), std::ios::in);
  std::ifstream file2(filename2.value().c_str(), std::ios::in);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  std::ifstream file1(filename1.value(), std::ios::in);
  std::ifstream file2(filename2.value(), std::ios::in);
#endif  // BUILDFLAG(IS_WIN)

  // Even if both files aren't openable (and thus, in some sense, "equal"),
  // any unusable file yields a result of "false".
  if (!file1.is_open() || !file2.is_open()) {
    return false;
  }

  do {
    std::string line1, line2;
    getline(file1, line1);
    getline(file2, line2);

    // Check for mismatched EOF states, or any error state.
    if ((file1.eof() != file2.eof()) || file1.bad() || file2.bad()) {
      return false;
    }

    // Trim all '\r' and '\n' characters from the end of the line.
    std::string::size_type end1 = line1.find_last_not_of("\r\n");
    if (end1 == std::string::npos) {
      line1.clear();
    } else if (end1 + 1 < line1.length()) {
      line1.erase(end1 + 1);
    }

    std::string::size_type end2 = line2.find_last_not_of("\r\n");
    if (end2 == std::string::npos) {
      line2.clear();
    } else if (end2 + 1 < line2.length()) {
      line2.erase(end2 + 1);
    }

    if (line1 != line2) {
      return false;
    }
  } while (!file1.eof() || !file2.eof());

  return true;
}

bool ReadStreamToString(FILE* stream, std::string* contents) {
  return ReadStreamToStringWithMaxSize(
      stream, std::numeric_limits<size_t>::max(), contents);
}

bool ReadStreamToStringWithMaxSize(FILE* stream,
                                   size_t max_size,
                                   std::string* contents) {
  if (contents) {
    contents->clear();
  }

  std::string content_string;
  bool read_successs = ReadStreamToSpanWithMaxSize(
      stream, max_size, [&content_string](size_t size) {
        content_string.resize(size);
        return as_writable_byte_span(content_string);
      });

  if (contents) {
    contents->swap(content_string);
  }
  return read_successs;
}

std::optional<std::vector<uint8_t>> ReadFileToBytes(const FilePath& path) {
  if (path.ReferencesParent()) {
    return std::nullopt;
  }

  ScopedFILE file_stream(OpenFile(path, "rb"));
  if (!file_stream) {
    return std::nullopt;
  }

  std::vector<uint8_t> bytes;
  if (!ReadStreamToSpanWithMaxSize(file_stream.get(),
                                   std::numeric_limits<size_t>::max(),
                                   [&bytes](size_t size) {
                                     bytes.resize(size);
                                     return span(bytes);
                                   })) {
    return std::nullopt;
  }
  return bytes;
}

bool ReadFileToString(const FilePath& path, std::string* contents) {
  return ReadFileToStringWithMaxSize(path, contents,
                                     std::numeric_limits<size_t>::max());
}

bool ReadFileToStringWithMaxSize(const FilePath& path,
                                 std::string* contents,
                                 size_t max_size) {
  if (contents) {
    contents->clear();
  }
  if (path.ReferencesParent()) {
    return false;
  }
  ScopedFILE file_stream(OpenFile(path, "rb"));
  if (!file_stream) {
    return false;
  }
  return ReadStreamToStringWithMaxSize(file_stream.get(), max_size, contents);
}

bool IsDirectoryEmpty(const FilePath& dir_path) {
  FileEnumerator files(dir_path, false,
                       FileEnumerator::FILES | FileEnumerator::DIRECTORIES);
  if (files.Next().empty()) {
    return true;
  }
  return false;
}

bool CreateTemporaryFile(FilePath* path) {
  FilePath temp_dir;
  return GetTempDir(&temp_dir) && CreateTemporaryFileInDir(temp_dir, path);
}

ScopedFILE CreateAndOpenTemporaryStream(FilePath* path) {
  FilePath directory;
  if (!GetTempDir(&directory)) {
    return nullptr;
  }

  return CreateAndOpenTemporaryStreamInDir(directory, path);
}

bool CreateDirectory(const FilePath& full_path) {
  return CreateDirectoryAndGetError(full_path, nullptr);
}

std::optional<int64_t> GetFileSize(const FilePath& file_path) {
  File::Info info;
  if (!GetFileInfo(file_path, &info)) {
    return std::nullopt;
  }
  return info.size;
}

OnceCallback<std::optional<int64_t>()> GetFileSizeCallback(
    const FilePath& path) {
  return BindOnce([](const FilePath& path) { return GetFileSize(path); }, path);
}

bool TouchFile(const FilePath& path,
               const Time& last_accessed,
               const Time& last_modified) {
  uint32_t flags = File::FLAG_OPEN | File::FLAG_WRITE_ATTRIBUTES;

#if BUILDFLAG(IS_WIN)
  // On Windows, FILE_FLAG_BACKUP_SEMANTICS is needed to open a directory.
  if (DirectoryExists(path)) {
    flags |= File::FLAG_WIN_BACKUP_SEMANTICS;
  }
#elif BUILDFLAG(IS_FUCHSIA)
  // On Fuchsia, we need O_RDONLY for directories, or O_WRONLY for files.
  // TODO(crbug.com/40620916): Find a cleaner workaround for this.
  flags |= (DirectoryExists(path) ? File::FLAG_READ : File::FLAG_WRITE);
#endif

  File file(path, flags);
  if (!file.IsValid()) {
    return false;
  }

  return file.SetTimes(last_accessed, last_modified);
}

bool CloseFile(FILE* file) {
  if (file == nullptr) {
    return true;
  }
  return fclose(file) == 0;
}

bool TruncateFile(FILE* file) {
  if (file == nullptr) {
    return false;
  }
  long current_offset = ftell(file);
  if (current_offset == -1) {
    return false;
  }
#if BUILDFLAG(IS_WIN)
  int fd = _fileno(file);
  if (_chsize(fd, current_offset) != 0) {
    return false;
  }
#else
  int fd = fileno(file);
  if (ftruncate(fd, current_offset) != 0) {
    return false;
  }
#endif
  return true;
}

std::optional<uint64_t> ReadFile(const FilePath& filename,
                                 span<uint8_t> buffer) {
  return ReadFile(filename, base::as_writable_chars(buffer));
}

int ReadFile(const FilePath& filename, char* data, int max_size) {
  if (max_size < 0) {
    return -1;
  }
  size_t unsigned_size = checked_cast<size_t>(max_size);

  // SAFETY: Depending on the caller to provide valid `data` and `max_size`.
  //
  // TODO(https://crbug.com/40284755): Mark this overload of `ReadFile` with
  // `UNSAFE_BUFFER_USAGE` and eventually remove it altogether (preferring the
  // overload that takes a `span`).
  span<char> chars_buffer = UNSAFE_TODO(span(data, unsigned_size));

  span<uint8_t> bytes_buffer = as_writable_byte_span(chars_buffer);
  std::optional<uint64_t> result = ReadFile(filename, bytes_buffer);
  if (!result) {
    return -1;
  }
  return checked_cast<int>(result.value());
}

bool WriteFile(const FilePath& filename, std::string_view data) {
  return WriteFile(filename, as_byte_span(data));
}

FilePath GetUniquePath(const FilePath& path) {
  return GetUniquePathWithSuffixFormat(path, " (%d)");
}

FilePath GetUniquePathWithSuffixFormat(const FilePath& path,
                                       base::cstring_view suffix_format) {
  DCHECK(!path.empty());
  DCHECK_EQ(std::ranges::count(suffix_format, '%'), 1);
  DCHECK(base::Contains(suffix_format, "%d"));

  if (!PathExists(path)) {
    return path;
  }
  for (int count = 1; count <= kMaxUniqueFiles; ++count) {
    std::string suffix(suffix_format);
    base::ReplaceFirstSubstringAfterOffset(&suffix, 0, "%d",
                                           base::NumberToString(count));
    FilePath candidate_path = path.InsertBeforeExtensionASCII(suffix);
    if (!PathExists(candidate_path)) {
      return candidate_path;
    }
  }
  return FilePath();
}

}  // namespace base
