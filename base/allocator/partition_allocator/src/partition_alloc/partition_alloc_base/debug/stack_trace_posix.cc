// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc_base/debug/stack_trace.h"

#include <fcntl.h>
#include <unistd.h>

#include <cstring>

#include "partition_alloc/partition_alloc_base/logging.h"
#include "partition_alloc/partition_alloc_base/posix/eintr_wrapper.h"
#include "partition_alloc/partition_alloc_base/strings/safe_sprintf.h"

#if !PA_BUILDFLAG(IS_ANDROID) && !PA_BUILDFLAG(IS_APPLE)
#include <link.h>  // For ElfW() macro.
#endif

#if PA_BUILDFLAG(IS_APPLE)
#include <dlfcn.h>
#endif

namespace partition_alloc::internal::base::debug {

namespace {

#if !PA_BUILDFLAG(IS_APPLE)

// On Android the 'open' function has two versions:
// int open(const char *pathname, int flags);
// int open(const char *pathname, int flags, mode_t mode);
//
// This doesn't play well with WrapEINTR template. This alias helps the compiler
// to make a decision.
int OpenFile(const char* pathname, int flags) {
  return open(pathname, flags);
}

constexpr size_t kBufferSize = 4096u;

enum {
  kMapReadable = 1u,
  kMapWritable = 2u,
  kMapExecutable = 4u,
  kMapPrivate = 8u,
};

bool ParseAddress(const char** ptr,
                  const char* end,
                  uintptr_t* address_return) {
  const char* start = *ptr;

  // 0xNN = 2 characters
  const char* max_address = start + sizeof(void*) * 2;
  uintptr_t value = 0;

  const char* p = start;
  for (; p < end && p < max_address; ++p) {
    if ('0' <= *p && *p <= '9') {
      value = (value << 4) | (unsigned char)(*p - '0');
    } else if ('a' <= *p && *p <= 'f') {
      value = (value << 4) | (unsigned char)(*p - 'a' + 10);
    } else {
      break;
    }
  }
  if (p == start) {
    return false;
  }
  *ptr = p;
  if (address_return) {
    *address_return = value;
  }
  return true;
}

bool ParseInteger(const char** ptr, const char* end) {
  const char* start = *ptr;

  const char* p = start;
  for (; p < end && '0' <= *p && *p <= '9'; ++p)
    ;
  *ptr = p;
  return p > start;
}

bool ParsePermissions(const char** ptr,
                      const char* end,
                      unsigned* permission_return) {
  unsigned permission = 0u;
  const char* p = *ptr;
  if (p < end && (*p == 'r' || *p == '-')) {
    permission |= (*p == 'r') ? kMapReadable : 0u;
    ++p;
  } else {
    return false;
  }
  if (p < end && (*p == 'w' || *p == '-')) {
    permission |= (*p == 'w') ? kMapWritable : 0u;
    ++p;
  } else {
    return false;
  }
  if (p < end && (*p == 'x' || *p == '-')) {
    permission |= (*p == 'w') ? kMapExecutable : 0u;
    ++p;
  } else {
    return false;
  }
  if (p < end && (*p == 'p' || *p == '-' || *p == 's')) {
    permission |= (*p == 'w') ? kMapPrivate : 0u;
    ++p;
  } else {
    return false;
  }
  *ptr = p;
  if (permission_return) {
    *permission_return = permission;
  }
  return true;
}

bool ParseMapsLine(const char* line_start,
                   const char* line_end,
                   uintptr_t* start_address_return,
                   uintptr_t* end_address_return,
                   unsigned* permission_return,
                   uintptr_t* offset_return,
                   const char** module_name) {
  const char* ptr = line_start;
  if (!ParseAddress(&ptr, line_end, start_address_return)) {
    return false;
  }
  // Delimiter
  if (ptr >= line_end || *ptr != '-') {
    return false;
  }
  ++ptr;
  if (!ParseAddress(&ptr, line_end, end_address_return)) {
    return false;
  }

  // Delimiter
  if (ptr >= line_end || *ptr != ' ') {
    return false;
  }
  ++ptr;

  // skip permissions.
  if (!ParsePermissions(&ptr, line_end, permission_return)) {
    return false;
  }

  // Delimiter
  if (ptr >= line_end || *ptr != ' ') {
    return false;
  }
  ++ptr;

  // skip offset
  if (ParseAddress(&ptr, line_end, offset_return)) {
    if (ptr >= line_end || *ptr != ' ') {
      return false;
    }
    ++ptr;

    // skip dev
    if (!ParseAddress(&ptr, line_end, nullptr)) {
      return false;
    }
    if (ptr >= line_end || *ptr != ':') {
      return false;
    }
    ++ptr;
    if (!ParseAddress(&ptr, line_end, nullptr)) {
      return false;
    }

    // Delimiter
    if (ptr >= line_end || *ptr != ' ') {
      return false;
    }
    ++ptr;

    // skip inode
    if (!ParseInteger(&ptr, line_end)) {
      return false;
    }
  } else {
    if (offset_return) {
      *offset_return = 0u;
    }
  }
  if (ptr >= line_end || *ptr != ' ') {
    return false;
  }
  for (; ptr < line_end && *ptr == ' '; ++ptr)
    ;
  if (ptr <= line_end && module_name) {
    *module_name = ptr;
  }
  return true;
}

#if !PA_BUILDFLAG(IS_ANDROID)

ssize_t ReadFromOffset(const int fd,
                       void* buf,
                       const size_t count,
                       const size_t offset) {
  char* buf0 = reinterpret_cast<char*>(buf);
  size_t num_bytes = 0;
  while (num_bytes < count) {
    ssize_t len;
    len = WrapEINTR(pread)(fd, buf0 + num_bytes, count - num_bytes,
                           static_cast<off_t>(offset + num_bytes));
    if (len < 0) {  // There was an error other than EINTR.
      return -1;
    }
    if (len == 0) {  // Reached EOF.
      break;
    }
    num_bytes += static_cast<size_t>(len);
  }
  return static_cast<ssize_t>(num_bytes);
}

void UpdateBaseAddress(unsigned permissions,
                       uintptr_t start_address,
                       uintptr_t* base_address) {
  // Determine the base address by reading ELF headers in process memory.
  // Skip non-readable maps.
  if (!(permissions & kMapReadable)) {
    return;
  }

  int mem_fd = WrapEINTR(OpenFile)("/proc/self/mem", O_RDONLY);
  if (mem_fd == -1) {
    PA_RAW_LOG(ERROR, "Failed to open /proc/self/mem\n");
    return;
  }

  ElfW(Ehdr) ehdr;
  ssize_t len =
      ReadFromOffset(mem_fd, &ehdr, sizeof(ElfW(Ehdr)), start_address);
  if (len == sizeof(ElfW(Ehdr))) {
    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) == 0) {
      switch (ehdr.e_type) {
        case ET_EXEC:
          *base_address = 0;
          break;
        case ET_DYN:
          // Find the segment containing file offset 0. This will correspond
          // to the ELF header that we just read. Normally this will have
          // virtual address 0, but this is not guaranteed. We must subtract
          // the virtual address from the address where the ELF header was
          // mapped to get the base address.
          //
          // If we fail to find a segment for file offset 0, use the address
          // of the ELF header as the base address.
          *base_address = start_address;
          for (unsigned i = 0; i != ehdr.e_phnum; ++i) {
            ElfW(Phdr) phdr;
            len =
                ReadFromOffset(mem_fd, &phdr, sizeof(ElfW(Phdr)),
                               start_address + ehdr.e_phoff + i * sizeof(phdr));
            if (len == sizeof(ElfW(Phdr)) && phdr.p_type == PT_LOAD &&
                phdr.p_offset == 0) {
              *base_address = start_address - phdr.p_vaddr;
              break;
            }
          }
          break;
        default:
          // ET_REL or ET_CORE. These aren't directly executable, so they don't
          // affect the base address.
          break;
      }
    }
  }
  close(mem_fd);
}

#endif  // !PA_BUILDFLAG(IS_ANDROID)

void PrintStackTraceInternal(const void** trace, size_t count) {
  int fd = WrapEINTR(OpenFile)("/proc/self/maps", O_RDONLY);
  if (fd == -1) {
    PA_RAW_LOG(ERROR, "Failed to open /proc/self/maps\n");
    return;
  }

  char buffer[kBufferSize];
  char* dest = buffer;
  char* buffer_end = buffer + kBufferSize;
#if !PA_BUILDFLAG(IS_ANDROID) && !PA_BUILDFLAG(IS_APPLE)
  uintptr_t base_address = 0u;
#endif

  while (dest < buffer_end) {
    ssize_t bytes_read = WrapEINTR(read)(fd, dest, buffer_end - dest);
    if (bytes_read == 0) {
      break;
    }
    if (bytes_read < 0) {
      PA_RAW_LOG(ERROR, "Failed to read /proc/self/maps\n");
      break;
    }

    char* read_end = dest + bytes_read;
    char* parsed = buffer;
    char* line_start = buffer;
    // It is difficult to remember entire memory regions and to use them
    // to process stack traces. Instead, try to parse each line of
    // /proc/self/maps and to process matched stack traces. This will
    // make the order of the output stack traces different from the input.
    for (char* line_end = buffer; line_end < read_end; ++line_end) {
      if (*line_end == '\n') {
        parsed = line_end + 1;
        *line_end = '\0';
        uintptr_t start_address = 0u;
        uintptr_t end_address = 0u;
        uintptr_t offset = 0u;
        unsigned permissions = 0u;
        const char* module_name = nullptr;
        bool ok =
            ParseMapsLine(line_start, line_end, &start_address, &end_address,
                          &permissions, &offset, &module_name);
        if (ok) {
#if !PA_BUILDFLAG(IS_ANDROID)
          UpdateBaseAddress(permissions, start_address, &base_address);
#endif
          if (module_name && *module_name != '\0') {
            for (size_t i = 0; i < count; i++) {
#if PA_BUILDFLAG(IS_ANDROID)
              // Subtract one as return address of function may be in the next
              // function when a function is annotated as noreturn.
              uintptr_t address = reinterpret_cast<uintptr_t>(trace[i]) - 1;
              uintptr_t base_address = start_address;
#else
              uintptr_t address = reinterpret_cast<uintptr_t>(trace[i]);
#endif
              if (start_address <= address && address < end_address) {
                OutputStackTrace(i, address, base_address, module_name, offset);
              }
            }
          }
        } else {
          PA_RAW_LOG(ERROR, "Parse failed.\n");
        }
        line_start = parsed;
      }
    }
    if (parsed == buffer) {
      // /proc/self/maps contains too long line (> kBufferSize).
      PA_RAW_LOG(ERROR, "/proc/self/maps has too long line.\n");
      break;
    }
    if (parsed < read_end) {
      size_t left_chars = read_end - parsed;
      memmove(buffer, parsed, left_chars);
      dest = buffer + left_chars;
    } else {
      dest = buffer;
    }
  }
  close(fd);
}
#endif  // !PA_BUILDFLAG(IS_APPLE)

#if PA_BUILDFLAG(IS_APPLE)
// Since /proc/self/maps is not available, use dladdr() to obtain module
// names and offsets inside the modules from the given addresses.
void PrintStackTraceInternal(const void* const* trace, size_t size) {
  // NOTE: This code MUST be async-signal safe (it's used by in-process
  // stack dumping signal handler). NO malloc or stdio is allowed here.

  Dl_info dl_info;
  for (size_t i = 0; i < size; ++i) {
    const bool dl_info_found = dladdr(trace[i], &dl_info) != 0;
    if (dl_info_found) {
      const char* last_sep = strrchr(dl_info.dli_fname, '/');
      const char* basename = last_sep ? last_sep + 1 : dl_info.dli_fname;

      // Use atos with --offset to obtain symbols from the printed addresses,
      // e.g.
      //  #01 0x0000000106225d6c  (base_unittests+0x0000000001999d6c)
      //  bash-3.2$ atos -o out/default/base_unittests --offset
      //   0x0000000001999d6c
      //  partition_alloc::internal::PartitionAllocTest_Basic_Test::TestBody()
      //  (in base_unittests) + 156
      OutputStackTrace(i, reinterpret_cast<uintptr_t>(trace[i]),
                       reinterpret_cast<uintptr_t>(dl_info.dli_fbase), basename,
                       0u);
    } else {
      OutputStackTrace(i, reinterpret_cast<uintptr_t>(trace[i]), 0u, "???", 0u);
    }
  }
}
#endif  // PA_BUILDFLAG(IS_APPLE)

}  // namespace

void PrintStackTrace(const void** trace, size_t count) {
  PrintStackTraceInternal(trace, count);
}

// stack_trace_android.cc defines its own OutputStackTrace.
#if !PA_BUILDFLAG(IS_ANDROID)
void OutputStackTrace(unsigned index,
                      uintptr_t address,
                      uintptr_t base_address,
                      const char* module_name,
                      uintptr_t) {
  char buffer[256];
  strings::SafeSPrintf(buffer, "#%02d 0x%0x  (%s+0x%0x)\n", index, address,
                       module_name, address - base_address);
  PA_RAW_LOG(INFO, buffer);
}
#endif  // !PA_BUILDFLAG(IS_ANDROID)

}  // namespace partition_alloc::internal::base::debug
