// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/input_method_configuration.h"

#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/global_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/session_manager/core/fake_session_manager_delegate.h"
#include "components/session_manager/core/session_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/mock_input_method_manager_impl.h"

namespace ash {
namespace input_method {

TEST(InputMethodConfigurationTest, TestInitialize) {
  session_manager::SessionManager session_manager{
      std::make_unique<session_manager::FakeSessionManagerDelegate>()};
  ScopedTestingCrosSettings cros_settings;

  InputMethodManager* manager = InputMethodManager::Get();
  EXPECT_FALSE(manager);

  Initialize(TestingBrowserProcess::GetGlobal()->local_state(),
             TestingBrowserProcess::GetGlobal()
                 ->GetFeatures()
                 ->application_locale_storage());
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
