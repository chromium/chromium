// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTALL_VERIFICATION_WIN_MODULE_INFO_H_
#define CHROME_BROWSER_INSTALL_VERIFICATION_WIN_MODULE_INFO_H_

#include <stdint.h>

#include <string>

// Represents and facilitates operations on the address range corresponding to a
// loaded module.
struct ModuleInfo {
  ModuleInfo() : base_address(0), size(0) {
  }

  ModuleInfo(const wchar_t* const module_name,
             uintptr_t module_base_address,
             uint32_t module_size)
      : name(module_name),
        base_address(module_base_address),
        size(module_size) {}

  // Sorts modules by their base address.
  bool operator< (const ModuleInfo& compare) const {
    return base_address < compare.base_address;
  }

  // Identifies if an address is within a module.
  bool ContainsAddress(uintptr_t address) const {
    return address >= base_address && address < base_address + size;
  }

  std::wstring name;
  uintptr_t base_address;
  uint32_t size;
};

#endif  // CHROME_BROWSER_INSTALL_VERIFICATION_WIN_MODULE_INFO_H_
