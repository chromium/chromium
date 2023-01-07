// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/login_state.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/login_state.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"

namespace {

bool IsLoginStateCrosapiAvailable() {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service->IsAvailable<crosapi::mojom::LoginState>()) {
    LOG(WARNING) << "Unsupported ash version.";
    return false;
  }
  return true;
}

}  // namespace

using LoginStateLacrosBrowserTest = InProcessBrowserTest;

// Test that the Login State Crosapi returns the session state.
IN_PROC_BROWSER_TEST_F(LoginStateLacrosBrowserTest, GetSessionState) {
  if (!IsLoginStateCrosapiAvailable())
    return;

  auto* lacros_service = chromeos::LacrosService::Get();

  crosapi::mojom::GetSessionStateResultPtr result;
  crosapi::mojom::LoginStateAsyncWaiter async_waiter(
      lacros_service->GetRemote<crosapi::mojom::LoginState>().get());
  async_waiter.GetSessionState(&result);

  ASSERT_FALSE(result->is_error_message());
  EXPECT_EQ(result->get_session_state(),
            crosapi::mojom::SessionState::kInSession);
}

using LoginStateLacrosExtensionApiTest = extensions::ExtensionApiTest;

// Test that the loginState extension API works for an extension running in
// Lacros.
IN_PROC_BROWSER_TEST_F(LoginStateLacrosExtensionApiTest, GetSessionState) {
  if (!IsLoginStateCrosapiAvailable())
    return;

  EXPECT_TRUE(
      RunExtensionTest("login_screen_apis/login_state/get_session_state",
                       {.custom_arg = "IN_SESSION"}));
}
