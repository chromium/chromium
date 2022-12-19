// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/metrics.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"

namespace crosapi {
namespace {

using MetricsLacrosBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(MetricsLacrosBrowserTest, Basic) {
  auto* lacros_service = chromeos::LacrosService::Get();
  ASSERT_TRUE(lacros_service);

  if (!lacros_service->IsAvailable<crosapi::mojom::Metrics>()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }

  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&](const std::string& result) { run_loop.Quit(); });

  lacros_service->GetRemote<crosapi::mojom::Metrics>()->GetFullHardwareClass(
      std::move(callback));
  run_loop.Run();
}

}  // namespace
}  // namespace crosapi
