// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/install_verification/win/module_verification_common.h"

#include "base/win/win_util.h"
#include "chrome/browser/install_verification/win/module_info.h"
#include "chrome/browser/install_verification/win/module_list.h"

bool GetLoadedModules(std::set<ModuleInfo>* loaded_modules) {
  std::vector<HMODULE> snapshot;
  if (!base::win::GetLoadedModulesSnapshot(::GetCurrentProcess(), &snapshot))
    return false;

  ModuleList::FromLoadedModuleSnapshot(snapshot)->GetModuleInfoSet(
      loaded_modules);
  return true;
}
