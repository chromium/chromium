// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTALL_VERIFICATION_WIN_MODULE_LIST_H_
#define CHROME_BROWSER_INSTALL_VERIFICATION_WIN_MODULE_LIST_H_

#include <Windows.h>
#include <stddef.h>

#include <memory>
#include <set>
#include <vector>

struct ModuleInfo;

// Manages a list of HMODULEs, releasing them upon destruction.
class ModuleList {
 public:
  ModuleList(const ModuleList&) = delete;
  ModuleList& operator=(const ModuleList&) = delete;

  ~ModuleList();

  // Attempts to AddRef each HMODULE in |snapshot|. If a module was unloaded
  // since |snapshot| was taken it will be silently dropped. Successfully
  // AddRef'd HMODULEs will be inserted in the returned ModuleList.
  //
  // This method _always_ returns a valid ModuleList instance.
  static std::unique_ptr<ModuleList> FromLoadedModuleSnapshot(
      const std::vector<HMODULE>& snapshot);

  // Retrieves name and address information for the module list.
  void GetModuleInfoSet(std::set<ModuleInfo>* module_info_set);

  size_t size() const {
    return modules_.size();
  }

  HMODULE operator[](size_t index) const {
    return modules_[index];
  }

 private:
  ModuleList();

  std::vector<HMODULE> modules_;
};

#endif  // CHROME_BROWSER_INSTALL_VERIFICATION_WIN_MODULE_LIST_H_
