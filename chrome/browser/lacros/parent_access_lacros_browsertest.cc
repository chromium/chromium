// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/parent_access.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/parent_access.mojom.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace {
constexpr char test_url[] = "http://example.com";
const std::u16string test_child_display_name = u"child display name";
const gfx::ImageSkia test_favicon =
    gfx::ImageSkia::CreateFrom1xBitmap(gfx::test::CreateBitmap(1, 2));
}  // namespace

using ParentAccessLacrosBrowserTest = InProcessBrowserTest;

// Tests that invoking Parent Access local web approvals UI via crosapi works.
IN_PROC_BROWSER_TEST_F(ParentAccessLacrosBrowserTest,
                       GetWebsiteParentApproval) {
  auto* lacros_service = chromeos::LacrosService::Get();

  if (!lacros_service->IsAvailable<crosapi::mojom::ParentAccess>() ||
      lacros_service->GetInterfaceVersion<crosapi::mojom::ParentAccess>() <
          int(crosapi::mojom::ParentAccess::
                  kGetWebsiteParentApprovalMinVersion)) {
    GTEST_SKIP() << "Unsupported ash version.";
  }

  base::RunLoop run_loop;
  lacros_service->GetRemote<crosapi::mojom::ParentAccess>()
      ->GetWebsiteParentApproval(
          GURL(test_url), test_child_display_name, test_favicon,
          base::BindLambdaForTesting([&](crosapi::mojom::ParentAccessResultPtr
                                             result) -> void {
            // Expect an error since the parent access UI isn't being
            // invoked in the correct environment (the user isn't a child).
            EXPECT_TRUE(result->is_error());
            EXPECT_EQ(
                result->get_error()->type,
                crosapi::mojom::ParentAccessErrorResult::Type::kNotAChildUser);
            run_loop.Quit();
          }));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ParentAccessLacrosBrowserTest,
                       GetExtensionParentApproval) {
  auto* lacros_service = chromeos::LacrosService::Get();

  if (!lacros_service->IsAvailable<crosapi::mojom::ParentAccess>() ||
      lacros_service->GetInterfaceVersion<crosapi::mojom::ParentAccess>() <
          int(crosapi::mojom::ParentAccess::
                  kGetExtensionParentApprovalMinVersion)) {
    GTEST_SKIP() << "Unsupported ash version.";
  }

  base::RunLoop run_loop;
  lacros_service->GetRemote<crosapi::mojom::ParentAccess>()
      ->GetExtensionParentApproval(
          u"extension", test_child_display_name,
          extensions::util::GetDefaultExtensionIcon(), /*permissions=*/{},
          /*requests_disabled=*/false,
          base::BindLambdaForTesting([&](crosapi::mojom::ParentAccessResultPtr
                                             result) -> void {
            // Expect an error since the parent access UI isn't being
            // invoked in the correct environment (the user isn't a child).
            EXPECT_TRUE(result->is_error());
            EXPECT_EQ(
                result->get_error()->type,
                crosapi::mojom::ParentAccessErrorResult::Type::kNotAChildUser);
            run_loop.Quit();
          }));

  run_loop.Run();
}
