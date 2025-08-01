// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "partition_alloc/partition_alloc_base/debug/proc_maps_linux.h"

#include <fcntl.h>
#include <stddef.h>

#include "partition_alloc/build_config.h"
#include "partition_alloc/partition_alloc_base/files/file_util.h"
#include "partition_alloc/partition_alloc_base/logging.h"
#include "partition_alloc/partition_alloc_base/posix/eintr_wrapper.h"
#include "partition_alloc/partition_alloc_check.h"

#if PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS) || \
    PA_BUILDFLAG(IS_ANDROID)
#include <inttypes.h>
#endif

namespace partition_alloc::internal::base::debug {

namespace {
// On Android the 'open' function may have two versions:
// int open(const char *pathname, int flags);
// int open(const char *pathname, int flags, mode_t mode);
//
// This doesn't play well with WrapEINTR template. This alias helps the compiler
// to make a decision.
int OpenFile(const char* pathname, int flags) {
  return open(pathname, flags);
}
}  // namespace

// Scans |proc_maps| starting from |pos| returning true if the gate VMA was
// found, otherwise returns false.
static bool ContainsGateVMA(std::string* proc_maps, size_t pos) {
#if PA_BUILDFLAG(PA_ARCH_CPU_ARM_FAMILY)
  // The gate VMA on ARM kernels is the interrupt vectors page.
  return proc_maps->find(" [vectors]\n", pos) != std::string::npos;
#elif PA_BUILDFLAG(PA_ARCH_CPU_X86_64)
  // The gate VMA on x86 64-bit kernels is the virtual system call page.
  return proc_maps->find(" [vsyscall]\n", pos) != std::string::npos;
#else
  // Otherwise assume there is no gate VMA in which case we shouldn't
  // get duplicate entries.
  return false;
#endif
}

bool ReadProcMaps(std::string* proc_maps) {
  // seq_file only writes out a page-sized amount on each call. Refer to header
  // file for details.
  const size_t read_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));

  int fd = WrapEINTR(OpenFile)("/proc/self/maps", O_RDONLY);
  if (fd == -1) {
    PA_LOG(ERROR) << "Couldn't open /proc/self/maps";
    WrapEINTR(close)(fd);
    return false;
  }
  proc_maps->clear();

  while (true) {
    // To avoid a copy, resize |proc_maps| so read() can write directly into it.
    // Compute |buffer| afterwards since resize() may reallocate.
    size_t pos = proc_maps->size();
    proc_maps->resize(pos + read_size);
    void* buffer = &(*proc_maps)[pos];

    ssize_t bytes_read = WrapEINTR(read)(fd, buffer, read_size);
    if (bytes_read < 0) {
      PA_DPLOG(ERROR) << "Couldn't read /proc/self/maps";
      proc_maps->clear();
      WrapEINTR(close)(fd);
      return false;
    }

    // ... and don't forget to trim off excess bytes.
    proc_maps->resize(pos + static_cast<size_t>(bytes_read));

    if (bytes_read == 0) {
      break;
    }

    // The gate VMA is handled as a special case after seq_file has finished
    // iterating through all entries in the virtual memory table.
    //
    // Unfortunately, if additional entries are added at this point in time
    // seq_file gets confused and the next call to read() will return duplicate
    // entries including the gate VMA again.
    //
    // Avoid this by searching for the gate VMA and breaking early.
    if (ContainsGateVMA(proc_maps, pos)) {
      break;
    }
  }

  WrapEINTR(close)(fd);
  return true;
}

bool ParseProcMaps(const std::string& input,
                   std::vector<MappedMemoryRegion>* regions_out) {
  PA_CHECK(regions_out);
  std::vector<MappedMemoryRegion> regions;

  // This isn't async safe nor terribly efficient, but it doesn't need to be at
  // this point in time.
  std::vector<std::string> lines;

  // Split the input into lines.
  int start = 0;
  for (size_t i = 0; i < input.size(); ++i) {
    if (input[i] == '\n') {
      lines.push_back(input.substr(start, i - start));
      start = i + 1;
    }
  }
  lines.push_back(input.substr(start));

  for (size_t i = 0; i < lines.size(); ++i) {
    // Due to splitting on '\n' the last line should be empty.
    if (i == lines.size() - 1) {
      if (!lines[i].empty()) {
        PA_DLOG(WARNING) << "Last line not empty";
        return false;
      }
      break;
    }

    MappedMemoryRegion region;
    const char* line = lines[i].c_str();
    char permissions[5] = {'\0'};  // Ensure NUL-terminated string.
    uint8_t dev_major = 0;
    uint8_t dev_minor = 0;
    long inode = 0;
    int path_index = 0;

    // Sample format from man 5 proc:
    //
    // address           perms offset  dev   inode   pathname
    // 08048000-08056000 r-xp 00000000 03:0c 64593   /usr/sbin/gpm
    //
    // The final %n term captures the offset in the input string, which is used
    // to determine the path name. It *does not* increment the return value.
    // Refer to man 3 sscanf for details.
    if (sscanf(line, "%" SCNxPTR "-%" SCNxPTR " %4c %llx %hhx:%hhx %ld %n",
               &region.start, &region.end, permissions, &region.offset,
               &dev_major, &dev_minor, &inode, &path_index) < 7) {
      PA_LOG(WARNING) << "sscanf failed for line: " << line;
      return false;
    }

    region.permissions = 0;

    if (permissions[0] == 'r') {
      region.permissions |= MappedMemoryRegion::READ;
    } else if (permissions[0] != '-') {
      return false;
    }

    if (permissions[1] == 'w') {
      region.permissions |= MappedMemoryRegion::WRITE;
    } else if (permissions[1] != '-') {
      return false;
    }

    if (permissions[2] == 'x') {
      region.permissions |= MappedMemoryRegion::EXECUTE;
    } else if (permissions[2] != '-') {
      return false;
    }

    if (permissions[3] == 'p') {
      region.permissions |= MappedMemoryRegion::PRIVATE;
    } else if (permissions[3] != 's' &&
               permissions[3] != 'S') {  // Shared memory.
      return false;
    }

    // Pushing then assigning saves us a string copy.
    regions.push_back(region);
    regions.back().path.assign(line + path_index);
  }

  regions_out->swap(regions);
  return true;
}

}  // namespace partition_alloc::internal::base::debug
