// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/install_verification/win/module_verification_test.h"

#include <Windows.h>

#include <vector>

#include "base/win/win_util.h"
#include "chrome/browser/install_verification/win/module_list.h"

std::set<size_t> ModuleVerificationTest::reported_module_ids_;

void ModuleVerificationTest::SetUp() {
  reported_module_ids_.clear();
}

bool ModuleVerificationTest::GetLoadedModuleInfoSet(
    std::set<ModuleInfo>* loaded_module_info_set) {
  std::vector<HMODULE> snapshot;
  if (!base::win::GetLoadedModulesSnapshot(::GetCurrentProcess(), &snapshot))
    return false;
  ModuleList::FromLoadedModuleSnapshot(snapshot)->GetModuleInfoSet(
      loaded_module_info_set);
  return true;
}

// static
void ModuleVerificationTest::ReportModule(size_t module_id) {
  reported_module_ids_.insert(module_id);
}
