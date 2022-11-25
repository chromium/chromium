// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/input_method_configuration.h"

#include <memory>

#include "chrome/browser/ash/input_method/mock_input_method_manager_impl.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/session_manager/core/session_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/input_method_manager.h"

namespace ash {
namespace input_method {

TEST(InputMethodConfigurationTest, TestInitialize) {
  session_manager::SessionManager session_manager;

  InputMethodManager* manager = InputMethodManager::Get();
  EXPECT_FALSE(manager);

  // Need to initialize local_state with TestingBrowserProcess::GetGlobal().
  // g_browser_process will be associated with this local state in turn.
  std::unique_ptr<ScopedTestingLocalState> local_state =
      std::make_unique<ScopedTestingLocalState>(
          TestingBrowserProcess::GetGlobal());
  Initialize();
  manager = InputMethodManager::Get();
  EXPECT_TRUE(manager);
  Shutdown();
}

TEST(InputMethodConfigurationTest, TestInitializeForTesting) {
  InputMethodManager* manager = InputMethodManager::Get();
  EXPECT_FALSE(manager);

  InitializeForTesting(new MockInputMethodManagerImpl);
  manager = InputMethodManager::Get();
  EXPECT_TRUE(manager);
  Shutdown();
}

}  // namespace input_method
}  // namespace ash
