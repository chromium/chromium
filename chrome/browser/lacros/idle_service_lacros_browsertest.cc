// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/lacros/system_idle_cache.h"
#include "content/public/test/browser_test.h"

namespace {

class IdleServiceLacrosBrowserTest : public InProcessBrowserTest {
 protected:
  IdleServiceLacrosBrowserTest() = default;

  IdleServiceLacrosBrowserTest(const IdleServiceLacrosBrowserTest&) = delete;
  IdleServiceLacrosBrowserTest& operator=(const IdleServiceLacrosBrowserTest&) =
      delete;
  ~IdleServiceLacrosBrowserTest() override = default;
};

// Smoke test for having ash-chrome send system idle info to lacros-chrome.
IN_PROC_BROWSER_TEST_F(IdleServiceLacrosBrowserTest, Smoke) {
  auto* lacros_service = chromeos::LacrosService::Get();
  ASSERT_TRUE(lacros_service);

  if (!lacros_service->IsAvailable<crosapi::mojom::IdleService>())
    return;

  // Check that SystemIdelCache exists.
  const chromeos::SystemIdleCache* system_idle_cache =
      chromeos::LacrosService::Get()->system_idle_cache();
  ASSERT_TRUE(system_idle_cache);
  EXPECT_FALSE(system_idle_cache->is_locked());
}

}  // namespace
