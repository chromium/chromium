// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"

#if defined(OS_WIN)
#include <io.h>
#endif
#include <stdio.h>

#include <fstream>
#include <limits>
#include <memory>
#include <vector>

#include "base/check_op.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace base {

#if !defined(OS_NACL_NONSFI)
#if !defined(OS_WIN)
OnceCallback<void(const FilePath&)> GetDeleteFileCallback() {
  return BindOnce(IgnoreResult(&DeleteFile));
}
#endif  // !defined(OS_WIN)

OnceCallback<void(const FilePath&)> GetDeletePathRecursivelyCallback() {
  return BindOnce(IgnoreResult(&DeletePathRecursively));
}

int64_t ComputeDirectorySize(const FilePath& root_path) {
  int64_t running_size = 0;
  FileEnumerator file_iter(root_path, true, FileEnumerator::FILES);
  while (!file_iter.Next().empty())
    running_size += file_iter.GetInfo().GetSize();
  return running_size;
}

bool Move(const FilePath& from_path, const FilePath& to_path) {
  if (from_path.ReferencesParent() || to_path.ReferencesParent())
    return false;
  return internal::MoveUnsafe(from_path, to_path);
}

bool CopyFileContents(File& infile, File& outfile) {
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
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
  std::vector<char> buffer(kBufferSize);

  for (;;) {
    int bytes_read = infile.ReadAtCurrentPos(buffer.data(), buffer.size());
    if (bytes_read < 0) {
      return false;
    }
    if (bytes_read == 0) {
      return true;
    }
    // Allow for partial writes
    int bytes_written_per_read = 0;
    do {
      int bytes_written_partial = outfile.WriteAtCurrentPos(
          &buffer[bytes_written_per_read], bytes_read - bytes_written_per_read);
      if (bytes_written_partial < 0) {
        return false;
      }

      bytes_written_per_read += bytes_written_partial;
    } while (bytes_written_per_read < bytes_read);
  }

  NOTREACHED();
  return false;
}

bool ContentsEqual(const FilePath& filename1, const FilePath& filename2) {
  // We open the file in binary format even if they are text files because
  // we are just comparing that bytes are exactly same in both files and not
  // doing anything smart with text formatting.
#if defined(OS_WIN)
  std::ifstream file1(filename1.value().c_str(),
                      std::ios::in | std::ios::binary);
  std::ifstream file2(filename2.value().c_str(),
                      std::ios::in | std::ios::binary);
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  std::ifstream file1(filename1.value(), std::ios::in | std::ios::binary);
  std::ifstream file2(filename2.value(), std::ios::in | std::ios::binary);
#endif  // OS_WIN

  // Even if both files aren't openable (and thus, in some sense, "equal"),
  // any unusable file yields a result of "false".
  if (!file1.is_open() || !file2.is_open())
    return false;

  const int BUFFER_SIZE = 2056;
  char buffer1[BUFFER_SIZE], buffer2[BUFFER_SIZE];
  do {
    file1.read(buffer1, BUFFER_SIZE);
    file2.read(buffer2, BUFFER_SIZE);

    if ((file1.eof() != file2.eof()) ||
        (file1.gcount() != file2.gcount()) ||
        (memcmp(buffer1, buffer2, static_cast<size_t>(file1.gcount())))) {
      file1.close();
      file2.close();
      return false;
    }
  } while (!file1.eof() || !file2.eof());

  file1.close();
  file2.close();
  return true;
}

bool TextContentsEqual(const FilePath& filename1, const FilePath& filename2) {
#if defined(OS_WIN)
  std::ifstream file1(filename1.value().c_str(), std::ios::in);
  std::ifstream file2(filename2.value().c_str(), std::ios::in);
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  std::ifstream file1(filename1.value(), std::ios::in);
  std::ifstream file2(filename2.value(), std::ios::in);
#endif  // OS_WIN

  // Even if both files aren't openable (and thus, in some sense, "equal"),
  // any unusable file yields a result of "false".
  if (!file1.is_open() || !file2.is_open())
    return false;

  do {
    std::string line1, line2;
    getline(file1, line1);
    getline(file2, line2);

    // Check for mismatched EOF states, or any error state.
    if ((file1.eof() != file2.eof()) ||
        file1.bad() || file2.bad()) {
      return false;
    }

    // Trim all '\r' and '\n' characters from the end of the line.
    std::string::size_type end1 = line1.find_last_not_of("\r\n");
    if (end1 == std::string::npos)
      line1.clear();
    else if (end1 + 1 < line1.length())
      line1.erase(end1 + 1);

    std::string::size_type end2 = line2.find_last_not_of("\r\n");
    if (end2 == std::string::npos)
      line2.clear();
    else if (end2 + 1 < line2.length())
      line2.erase(end2 + 1);

    if (line1 != line2)
      return false;
  } while (!file1.eof() || !file2.eof());

  return true;
}
#endif  // !defined(OS_NACL_NONSFI)

bool ReadStreamToString(FILE* stream, std::string* contents) {
  return ReadStreamToStringWithMaxSize(
      stream, std::numeric_limits<size_t>::max(), contents);
}

bool ReadStreamToStringWithMaxSize(FILE* stream,
                                   size_t max_size,
                                   std::string* contents) {
  if (contents)
    contents->clear();

  // Seeking to the beginning is best-effort -- it is expected to fail for
  // certain non-file stream (e.g., pipes).
  HANDLE_EINTR(fseek(stream, 0, SEEK_SET));

  // Many files have incorrect size (proc files etc). Hence, the file is read
  // sequentially as opposed to a one-shot read, using file size as a hint for
  // chunk size if available.
  constexpr int64_t kDefaultChunkSize = 1 << 16;
  int64_t chunk_size = kDefaultChunkSize - 1;
#if !defined(OS_NACL_NONSFI)
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
#if defined(OS_WIN)
  BY_HANDLE_FILE_INFORMATION file_info = {};
  if (::GetFileInformationByHandle(
          reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stream))),
          &file_info)) {
    LARGE_INTEGER size;
    size.HighPart = file_info.nFileSizeHigh;
    size.LowPart = file_info.nFileSizeLow;
    if (size.QuadPart > 0)
      chunk_size = size.QuadPart;
  }
#else   // defined(OS_WIN)
  // In cases where the reported file size is 0, use a smaller chunk size to
  // minimize memory allocated and cost of string::resize() in case the read
  // size is small (i.e. proc files). If the file is larger than this, the read
  // loop will reset |chunk_size| to kDefaultChunkSize.
  constexpr int64_t kSmallChunkSize = 4096;
  chunk_size = kSmallChunkSize - 1;
  stat_wrapper_t file_info = {};
  if (!File::Fstat(fileno(stream), &file_info) && file_info.st_size > 0)
    chunk_size = file_info.st_size;
#endif  // defined(OS_WIN)
  // We need to attempt to read at EOF for feof flag to be set so here we
  // use |chunk_size| + 1.
  chunk_size = std::min<uint64_t>(chunk_size, max_size) + 1;
#else   // !defined(OS_NACL_NONSFI)
  chunk_size = kDefaultChunkSize;
#endif  // !defined(OS_NACL_NONSFI)
  size_t bytes_read_this_pass;
  size_t bytes_read_so_far = 0;
  bool read_status = true;
  std::string local_contents;
  local_contents.resize(chunk_size);

  while ((bytes_read_this_pass = fread(&local_contents[bytes_read_so_far], 1,
                                       chunk_size, stream)) > 0) {
    if ((max_size - bytes_read_so_far) < bytes_read_this_pass) {
      // Read more than max_size bytes, bail out.
      bytes_read_so_far = max_size;
      read_status = false;
      break;
    }
    // In case EOF was not reached, iterate again but revert to the default
    // chunk size.
    if (bytes_read_so_far == 0)
      chunk_size = kDefaultChunkSize;

    bytes_read_so_far += bytes_read_this_pass;
    // Last fread syscall (after EOF) can be avoided via feof, which is just a
    // flag check.
    if (feof(stream))
      break;
    local_contents.resize(bytes_read_so_far + chunk_size);
  }
  read_status = read_status && !ferror(stream);
  if (contents) {
    contents->swap(local_contents);
    contents->resize(bytes_read_so_far);
  }

  return read_status;
}

bool ReadFileToString(const FilePath& path, std::string* contents) {
  return ReadFileToStringWithMaxSize(path, contents,
                                     std::numeric_limits<size_t>::max());
}

bool ReadFileToStringWithMaxSize(const FilePath& path,
                                 std::string* contents,
                                 size_t max_size) {
  if (contents)
    contents->clear();
  if (path.ReferencesParent())
    return false;
  ScopedFILE file_stream(OpenFile(path, "rb"));
  if (!file_stream)
    return false;
  return ReadStreamToStringWithMaxSize(file_stream.get(), max_size, contents);
}

#if !defined(OS_NACL_NONSFI)
bool IsDirectoryEmpty(const FilePath& dir_path) {
  FileEnumerator files(dir_path, false,
      FileEnumerator::FILES | FileEnumerator::DIRECTORIES);
  if (files.Next().empty())
    return true;
  return false;
}

bool CreateTemporaryFile(FilePath* path) {
  FilePath temp_dir;
  return GetTempDir(&temp_dir) && CreateTemporaryFileInDir(temp_dir, path);
}

ScopedFILE CreateAndOpenTemporaryStream(FilePath* path) {
  FilePath directory;
  if (!GetTempDir(&directory))
    return nullptr;

  return CreateAndOpenTemporaryStreamInDir(directory, path);
}

bool CreateDirectory(const FilePath& full_path) {
  return CreateDirectoryAndGetError(full_path, nullptr);
}

bool GetFileSize(const FilePath& file_path, int64_t* file_size) {
  File::Info info;
  if (!GetFileInfo(file_path, &info))
    return false;
  *file_size = info.size;
  return true;
}

bool TouchFile(const FilePath& path,
               const Time& last_accessed,
               const Time& last_modified) {
  int flags = File::FLAG_OPEN | File::FLAG_WRITE_ATTRIBUTES;

#if defined(OS_WIN)
  // On Windows, FILE_FLAG_BACKUP_SEMANTICS is needed to open a directory.
  if (DirectoryExists(path))
    flags |= File::FLAG_BACKUP_SEMANTICS;
#elif defined(OS_FUCHSIA)
  // On Fuchsia, we need O_RDONLY for directories, or O_WRONLY for files.
  // TODO(https://crbug.com/947802): Find a cleaner workaround for this.
  flags |= (DirectoryExists(path) ? File::FLAG_READ : File::FLAG_WRITE);
#endif

  File file(path, flags);
  if (!file.IsValid())
    return false;

  return file.SetTimes(last_accessed, last_modified);
}
#endif  // !defined(OS_NACL_NONSFI)

bool CloseFile(FILE* file) {
  if (file == nullptr)
    return true;
  return fclose(file) == 0;
}

#if !defined(OS_NACL_NONSFI)
bool TruncateFile(FILE* file) {
  if (file == nullptr)
    return false;
  long current_offset = ftell(file);
  if (current_offset == -1)
    return false;
#if defined(OS_WIN)
  int fd = _fileno(file);
  if (_chsize(fd, current_offset) != 0)
    return false;
#else
  int fd = fileno(file);
  if (ftruncate(fd, current_offset) != 0)
    return false;
#endif
  return true;
}

bool WriteFile(const FilePath& filename, span<const uint8_t> data) {
  int size = checked_cast<int>(data.size());
  return WriteFile(filename, reinterpret_cast<const char*>(data.data()),
                   size) == size;
}

bool WriteFile(const FilePath& filename, StringPiece data) {
  int size = checked_cast<int>(data.size());
  return WriteFile(filename, data.data(), size) == size;
}

int GetUniquePathNumber(const FilePath& path) {
  DCHECK(!path.empty());
  if (!PathExists(path))
    return 0;

  std::string number;
  for (int count = 1; count <= kMaxUniqueFiles; ++count) {
    StringAppendF(&number, " (%d)", count);
    if (!PathExists(path.InsertBeforeExtensionASCII(number)))
      return count;
    number.clear();
  }

  return -1;
}

FilePath GetUniquePath(const FilePath& path) {
  DCHECK(!path.empty());
  const int uniquifier = GetUniquePathNumber(path);
  if (uniquifier > 0)
    return path.InsertBeforeExtensionASCII(StringPrintf(" (%d)", uniquifier));
  return uniquifier == 0 ? path : base::FilePath();
}

namespace internal {

bool PreReadFileSlow(const FilePath& file_path, int64_t max_bytes) {
  DCHECK_GE(max_bytes, 0);

  File file(file_path, File::FLAG_OPEN | File::FLAG_READ |
                           File::FLAG_SEQUENTIAL_SCAN |
                           File::FLAG_SHARE_DELETE);
  if (!file.IsValid())
    return false;

  constexpr int kBufferSize = 1024 * 1024;
  // Ensures the buffer is deallocated at function exit.
  std::unique_ptr<char[]> buffer_deleter(new char[kBufferSize]);
  char* const buffer = buffer_deleter.get();

  while (max_bytes > 0) {
    // The static_cast<int> is safe because kBufferSize is int, and both values
    // are non-negative. So, the minimum is guaranteed to fit in int.
    const int read_size =
        static_cast<int>(std::min<int64_t>(max_bytes, kBufferSize));
    DCHECK_GE(read_size, 0);
    DCHECK_LE(read_size, kBufferSize);

    const int read_bytes = file.ReadAtCurrentPos(buffer, read_size);
    if (read_bytes < 0)
      return false;
    if (read_bytes == 0)
      break;

    max_bytes -= read_bytes;
  }

  return true;
}

}  // namespace internal

#endif  // !defined(OS_NACL_NONSFI)

}  // namespace base
