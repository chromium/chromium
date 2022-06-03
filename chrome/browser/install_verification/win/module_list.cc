// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/install_verification/win/module_list.h"

#include <Psapi.h>

#include <algorithm>

#include "base/check.h"
#include "chrome/browser/install_verification/win/module_info.h"

namespace {

void CheckFreeLibrary(HMODULE module) {
  BOOL result = ::FreeLibrary(module);
  DPCHECK(result);
}

}  // namespace

ModuleList::~ModuleList() {
  std::for_each(modules_.begin(), modules_.end(), &CheckFreeLibrary);
}

std::unique_ptr<ModuleList> ModuleList::FromLoadedModuleSnapshot(
    const std::vector<HMODULE>& snapshot) {
  std::unique_ptr<ModuleList> instance(new ModuleList);

  for (size_t i = 0; i < snapshot.size(); ++i) {
    HMODULE module = NULL;
    // ::GetModuleHandleEx add-ref's the module if successful.
    if (::GetModuleHandleEx(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCWSTR>(snapshot[i]),
            &module)) {
        instance->modules_.push_back(module);
    }
  }

  return instance;
}

void ModuleList::GetModuleInfoSet(std::set<ModuleInfo>* module_info_set) {
  HANDLE current_process = ::GetCurrentProcess();
  for (size_t i = 0; i < modules_.size(); ++i) {
    wchar_t filename[MAX_PATH];
    // Simply ignore modules where GetModuleFileName or GetModuleInformation
    // failed, they might have been unloaded.
    if (::GetModuleFileName(modules_[i], filename, MAX_PATH) &&
        filename[0]) {
      MODULEINFO sys_module_info = {};
      if (::GetModuleInformation(
              current_process, modules_[i],
              &sys_module_info, sizeof(sys_module_info))) {
        module_info_set->insert(ModuleInfo(
            filename,
            reinterpret_cast<uintptr_t>(sys_module_info.lpBaseOfDll),
            sys_module_info.SizeOfImage));
      }
    }
  }
}

ModuleList::ModuleList() {}
