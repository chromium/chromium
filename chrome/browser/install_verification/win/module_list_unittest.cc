// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/install_verification/win/module_list.h"

#include <Windows.h>

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/win/win_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

TEST(ModuleListTest, TestCase) {
  std::vector<HMODULE> snapshot;
  ASSERT_TRUE(
      base::win::GetLoadedModulesSnapshot(::GetCurrentProcess(), &snapshot));
  std::unique_ptr<ModuleList> module_list(
      ModuleList::FromLoadedModuleSnapshot(snapshot));

  // Lookup the number of loaded modules.
  size_t original_list_size = module_list->size();
  ASSERT_GT(original_list_size, 0u);
  snapshot.clear();

  // Load in a new module. Pick msvidc32.dll as it is present from WinXP to
  // Win8 and yet rarely used.
  ASSERT_EQ(NULL, ::GetModuleHandle(L"msvidc32.dll"));

  HMODULE new_dll = ::LoadLibrary(L"msvidc32.dll");
  ASSERT_NE(static_cast<HMODULE>(NULL), new_dll);
  absl::Cleanup release_new_dll = [new_dll] { ::FreeLibrary(new_dll); };

  // Verify that there is an increase in the snapshot size.
  ASSERT_TRUE(
      base::win::GetLoadedModulesSnapshot(::GetCurrentProcess(), &snapshot));
  module_list = ModuleList::FromLoadedModuleSnapshot(snapshot);
  ASSERT_GT(module_list->size(), original_list_size);

  // Unload the module.
  std::move(release_new_dll).Invoke();

  // Reset module_list here. That should typically be the last ref on
  // msvidc32.dll, so it will be unloaded now.
  module_list.reset();
  ASSERT_EQ(NULL, ::GetModuleHandle(L"msvidc32.dll"));

  // List the modules from the stale snapshot (including a dangling HMODULE to
  // msvidc32.dll), simulating a race condition.
  module_list = ModuleList::FromLoadedModuleSnapshot(snapshot);
  ASSERT_EQ(module_list->size(), original_list_size);
}
