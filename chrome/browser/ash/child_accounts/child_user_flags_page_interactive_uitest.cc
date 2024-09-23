// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gtest_tags.h"
#include "chrome/browser/ash/child_accounts/child_user_interactive_base_test.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/supervised_user/supervision_mixin.h"
#include "url/gurl.h"

namespace ash {

constexpr char kFlagsUrl[] = "chrome://flags";

// The flags page is expected to be blocked for supervised users by the URL
// blocklist policy. This policy is applied in the ChildUserInteractiveBaseTest.
class ChildUserFlagsPageInteractiveTest : public ChildUserInteractiveBaseTest {
 public:
  ChildUserFlagsPageInteractiveTest() = default;
  ChildUserFlagsPageInteractiveTest(const ChildUserFlagsPageInteractiveTest&) =
      delete;
  ChildUserFlagsPageInteractiveTest& operator=(
      const ChildUserFlagsPageInteractiveTest&) = delete;
  ~ChildUserFlagsPageInteractiveTest() override = default;

  auto OpenFlagsPage() {
    return Do([&]() { CreateBrowserWindow(GURL(kFlagsUrl)); });
  }
};

IN_PROC_BROWSER_TEST_F(ChildUserFlagsPageInteractiveTest,
                       CheckFlagsPageBlocked) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-bf561e54-e674-426b-b1f5-78b20540d39c");

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFlagsTabId);
  const DeepQuery kErrorPage{"#main-frame-error"};

  RunTestSequence(Log("Navigate to flags page"),
                  InstrumentNextTab(kFlagsTabId, AnyBrowser()), OpenFlagsPage(),

                  Log("Check that flags page is blocked"),
                  WaitForElementExists(kFlagsTabId, kErrorPage));
}

}  // namespace ash
