// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/login_state.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/login_state.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"

using LoginStateLacrosBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(LoginStateLacrosBrowserTest, GetSessionState) {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service->IsAvailable<crosapi::mojom::LoginState>()) {
    LOG(WARNING) << "Unsupported ash version.";
    return;
  }

  crosapi::mojom::GetSessionStateResultPtr result;
  crosapi::mojom::LoginStateAsyncWaiter async_waiter(
      lacros_service->GetRemote<crosapi::mojom::LoginState>().get());
  async_waiter.GetSessionState(&result);

  ASSERT_FALSE(result->is_error_message());
  EXPECT_EQ(result->get_session_state(),
            crosapi::mojom::SessionState::kInSession);
}
