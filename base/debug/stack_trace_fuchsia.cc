// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/stack_trace.h"

#include <link.h>
#include <stddef.h>
#include <string.h>
#include <threads.h>
#include <unwind.h>
#include <zircon/process.h>
#include <zircon/syscalls.h>
#include <zircon/syscalls/definitions.h>
#include <zircon/syscalls/port.h>
#include <zircon/types.h>

#include <algorithm>
#include <array>
#include <iomanip>
#include <iostream>

#include "base/logging.h"

namespace base {
namespace debug {

namespace {

const char kProcessNamePrefix[] = "app:";
const size_t kProcessNamePrefixLen = arraysize(kProcessNamePrefix) - 1;

struct BacktraceData {
  void** trace_array;
  size_t* count;
  size_t max;
};

_Unwind_Reason_Code UnwindStore(struct _Unwind_Context* context,
                                void* user_data) {
  BacktraceData* data = reinterpret_cast<BacktraceData*>(user_data);
  uintptr_t pc = _Unwind_GetIP(context);
  data->trace_array[*data->count] = reinterpret_cast<void*>(pc);
  *data->count += 1;
  if (*data->count == data->max)
    return _URC_END_OF_STACK;
  return _URC_NO_REASON;
}

// Stores and queries debugging symbol map info for the current process.
class SymbolMap {
 public:
  struct Entry {
    void* addr;
    char name[ZX_MAX_NAME_LEN + kProcessNamePrefixLen];
  };

  SymbolMap();
  ~SymbolMap() = default;

  // Gets the symbol map entry for |address|. Returns null if no entry could be
  // found for the address, or if the symbol map could not be queried.
  Entry* GetForAddress(void* address);

 private:
  // Component builds of Chrome pull about 250 shared libraries (on Linux), so
  // 512 entries should be enough in most cases.
  static const size_t kMaxMapEntries = 512;

  void Populate();

  // Sorted in descending order by address, for lookup purposes.
  std::array<Entry, kMaxMapEntries> entries_;

  size_t count_ = 0;
  bool valid_ = false;

  DISALLOW_COPY_AND_ASSIGN(SymbolMap);
};

SymbolMap::SymbolMap() {
  Populate();
}

SymbolMap::Entry* SymbolMap::GetForAddress(void* address) {
  if (!valid_) {
    return nullptr;
  }

  // Working backwards in the address space, return the first map entry whose
  // address comes before |address| (thereby enclosing it.)
  for (size_t i = 0; i < count_; ++i) {
    if (address >= entries_[i].addr) {
      return &entries_[i];
    }
  }
  return nullptr;
}

void SymbolMap::Populate() {
  zx_handle_t process = zx_process_self();

  // Try to fetch the name of the process' main executable, which was set as the
  // name of the |process| kernel object.
  // TODO(wez): Object names can only have up to ZX_MAX_NAME_LEN characters, so
  // if we keep hitting problems with truncation, find a way to plumb argv[0]
  // through to here instead, e.g. using CommandLine::GetProgramName().
  char app_name[arraysize(SymbolMap::Entry::name)];
  strcpy(app_name, kProcessNamePrefix);
  zx_status_t status = zx_object_get_property(
      process, ZX_PROP_NAME, app_name + kProcessNamePrefixLen,
      sizeof(app_name) - kProcessNamePrefixLen);
  if (status != ZX_OK) {
    DPLOG(WARNING)
        << "Couldn't get name, falling back to 'app' for program name: "
        << status;
    strlcat(app_name, "app", sizeof(app_name));
  }

  // Retrieve the debug info struct.
  uintptr_t debug_addr;
  status = zx_object_get_property(process, ZX_PROP_PROCESS_DEBUG_ADDR,
                                  &debug_addr, sizeof(debug_addr));
  if (status != ZX_OK) {
    DPLOG(ERROR) << "Couldn't get symbol map for process: " << status;
    return;
  }
  r_debug* debug_info = reinterpret_cast<r_debug*>(debug_addr);

  // Get the link map from the debug info struct.
  link_map* lmap = reinterpret_cast<link_map*>(debug_info->r_map);
  if (!lmap) {
    DPLOG(ERROR) << "Null link_map for process.";
    return;
  }

  // Copy the contents of the link map linked list to |entries_|.
  while (lmap != nullptr) {
    if (count_ >= entries_.size()) {
      break;
    }
    SymbolMap::Entry* next_entry = &entries_[count_];
    count_++;

    next_entry->addr = reinterpret_cast<void*>(lmap->l_addr);
    char* name_to_use = lmap->l_name[0] ? lmap->l_name : app_name;
    strlcpy(next_entry->name, name_to_use, sizeof(next_entry->name));
    lmap = lmap->l_next;
  }

  std::sort(
      entries_.begin(), entries_.begin() + count_,
      [](const Entry& a, const Entry& b) -> bool { return a.addr > b.addr; });

  valid_ = true;
}

}  // namespace

// static
bool EnableInProcessStackDumping() {
  // StackTrace works to capture the current stack (e.g. for diagnostics added
  // to code), but for local capture and print of backtraces, we just let the
  // system crashlogger take over. It handles printing out a nicely formatted
  // backtrace with dso information, relative offsets, etc. that we can then
  // filter with addr2line in the run script to get file/line info.
  return true;
}

StackTrace::StackTrace(size_t count) : count_(0) {
  BacktraceData data = {&trace_[0], &count_,
                        std::min(count, static_cast<size_t>(kMaxTraces))};
  _Unwind_Backtrace(&UnwindStore, &data);
}

void StackTrace::PrintWithPrefix(const char* prefix_string) const {
  OutputToStreamWithPrefix(&std::cerr, prefix_string);
}

// Sample stack trace output is designed to be similar to Fuchsia's crashlogger:
// bt#00: pc 0x1527a058aa00 (app:/system/base_unittests,0x18bda00)
// bt#01: pc 0x1527a0254b5c (app:/system/base_unittests,0x1587b5c)
// bt#02: pc 0x15279f446ece (app:/system/base_unittests,0x779ece)
// ...
// bt#21: pc 0x1527a05b51b4 (app:/system/base_unittests,0x18e81b4)
// bt#22: pc 0x54fdbf3593de (libc.so,0x1c3de)
// bt#23: end
void StackTrace::OutputToStreamWithPrefix(std::ostream* os,
                                          const char* prefix_string) const {
  SymbolMap map;

  size_t i = 0;
  for (; (i < count_) && os->good(); ++i) {
    SymbolMap::Entry* entry = map.GetForAddress(trace_[i]);
    if (prefix_string)
      *os << prefix_string;
    if (entry) {
      size_t offset = reinterpret_cast<uintptr_t>(trace_[i]) -
                      reinterpret_cast<uintptr_t>(entry->addr);
      *os << "bt#" << std::setw(2) << std::setfill('0') << i << std::setw(0)
          << ": pc " << trace_[i] << " (" << entry->name << ",0x" << std::hex
          << offset << std::dec << std::setw(0) << ")\n";
    } else {
      // Fallback if the DSO map isn't available.
      // Logged PC values are absolute memory addresses, and the shared object
      // name is not emitted.
      *os << "bt#" << std::setw(2) << std::setfill('0') << i << std::setw(0)
          << ": pc " << trace_[i] << "\n";
    }
  }

  (*os) << "bt#" << std::setw(2) << i << ": end\n";
}

}  // namespace debug
}  // namespace base
