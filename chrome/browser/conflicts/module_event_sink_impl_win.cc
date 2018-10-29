// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/module_event_sink_impl_win.h"

#include <windows.h>

#include <psapi.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/task/post_task.h"
#include "chrome/browser/conflicts/module_database_win.h"
#include "chrome/common/conflicts/module_watcher_win.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace {

// Gets the path of the module in the provided remote process. Returns true on
// success, false otherwise.
bool GetModulePath(base::ProcessHandle process,
                   HMODULE module,
                   base::FilePath* path) {
  std::vector<wchar_t> temp_path(MAX_PATH);
  size_t length = 0;
  while (true) {
    length = ::GetModuleFileNameEx(process, module, temp_path.data(),
                                   temp_path.size());
    if (length == 0)
      return false;
    if (length < temp_path.size())
      break;
    // The entire buffer was consumed, so grow it to ensure the result wasn't
    // actually truncated.
    temp_path.resize(2 * temp_path.size());
  }

  *path = base::FilePath(base::StringPiece16(temp_path.data(), length));
  return true;
}

// Gets the size of a module in a remote process. Returns true on success, false
// otherwise.
bool GetModuleSize(base::ProcessHandle process,
                   HMODULE module,
                   uint32_t* size) {
  MODULEINFO info = {};
  if (!::GetModuleInformation(process, module, &info, sizeof(info)))
    return false;
  *size = info.SizeOfImage;
  return true;
}

// Reads the typed data from a remote process. Returns true on success, false
// otherwise.
template <typename T>
bool ReadRemoteData(base::ProcessHandle process, uint64_t address, T* data) {
  const void* typed_address =
      reinterpret_cast<const void*>(static_cast<uintptr_t>(address));
  SIZE_T bytes_read = 0;
  if (!::ReadProcessMemory(process, typed_address, data, sizeof(*data),
                           &bytes_read)) {
    return false;
  }
  if (bytes_read != sizeof(*data))
    return false;
  return true;
}

// Reads the time date stamp from the module loaded in the provided remote
// |process| at the provided remote |load_address|.
bool GetModuleTimeDateStamp(base::ProcessHandle process,
                            uint64_t load_address,
                            uint32_t* time_date_stamp) {
  uint64_t address = load_address + offsetof(IMAGE_DOS_HEADER, e_lfanew);
  LONG e_lfanew = 0;
  if (!ReadRemoteData(process, address, &e_lfanew))
    return false;

  address = load_address + e_lfanew + offsetof(IMAGE_NT_HEADERS, FileHeader) +
            offsetof(IMAGE_FILE_HEADER, TimeDateStamp);
  DWORD temp = 0;
  if (!ReadRemoteData(process, address, &temp))
    return false;

  *time_date_stamp = temp;
  return true;
}

// Handles the module event on a background task. Looks up the path, size and
// time date stamp of the remote process and forwards the event to the
// ModuleDatabase.
void HandleModuleEvent(ModuleDatabase* module_database,
                       base::Process process,
                       content::ProcessType process_type,
                       mojom::ModuleEventType event_Type,
                       uint64_t load_address) {
  // Mojo takes care of validating |event_type|, so only |load_address| needs to
  // be checked. Load addresses must be aligned with the allocation granularity
  // which is at least 64KB on any supported Windows OS.
  if (load_address == 0 || load_address % (64 * 1024) != 0)
    return;

  // The |load_address| is a unique key to a module in a remote process. If
  // there is a valid module there then the following queries should all pass.
  // If any of them fail then the load event is silently swallowed.

  // Convert the |load_address| to a module handle.
  HMODULE module =
      reinterpret_cast<HMODULE>(static_cast<uintptr_t>(load_address));

  // Look up the various pieces of module metadata in the remote process.

  base::FilePath module_path;
  if (!GetModulePath(process.Handle(), module, &module_path))
    return;

  uint32_t module_size = 0;
  if (!GetModuleSize(process.Handle(), module, &module_size))
    return;

  uint32_t module_time_date_stamp = 0;
  if (!GetModuleTimeDateStamp(process.Handle(), load_address,
                              &module_time_date_stamp))
    return;

  // Forward this to the module database.
  module_database->OnModuleLoad(process_type, module_path, module_size,
                                module_time_date_stamp);
}

}  // namespace

ModuleEventSinkImpl::ModuleEventSinkImpl(base::Process process,
                                         content::ProcessType process_type,
                                         ModuleDatabase* module_database)
    : process_(std::move(process)),
      module_database_(module_database),
      process_type_(process_type) {}

ModuleEventSinkImpl::~ModuleEventSinkImpl() = default;

// static
void ModuleEventSinkImpl::Create(GetProcessCallback get_process,
                                 content::ProcessType process_type,
                                 ModuleDatabase* module_database,
                                 mojom::ModuleEventSinkRequest request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::Process process = get_process.Run();
  auto module_event_sink_impl = std::make_unique<ModuleEventSinkImpl>(
      std::move(process), process_type, module_database);
  mojo::MakeStrongBinding(std::move(module_event_sink_impl),
                          std::move(request));
}

void ModuleEventSinkImpl::OnModuleEvent(mojom::ModuleEventType event_type,
                                        uint64_t load_address) {
  // Handle the event on a background sequence.
  base::PostTaskWithTraits(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()},
      base::BindOnce(&HandleModuleEvent, module_database_, process_.Duplicate(),
                     process_type_, event_type, load_address));
}
