// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/supervised_user/family_live_test.h"
#include "chrome/test/supervised_user/family_member.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/controls/menu/menu_item_view.h"

namespace supervised_user {
namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);

// TODO(b/301587955): Fix placement of supervised_user/e2e test files and their
// dependencies.
class IncognitoModeInSupervisedContextUiTest
    : public InteractiveFamilyLiveTest {
 public:
  // Declares Prod rpc mode, but doesn't send any rpc anyway.
  IncognitoModeInSupervisedContextUiTest()
      : InteractiveFamilyLiveTest(FamilyLiveTest::RpcMode::kProd) {}

 protected:
  auto CheckCountOfIncognitoBrowsers(size_t expected_count) {
    return Check(base::BindLambdaForTesting([expected_count]() {
                   return BrowserList::GetIncognitoBrowserCount() ==
                          expected_count;
                 }),
                 "Verify count of incognito browsers");
  }
};

// TODO(https://crbug.com/367205684): SelectMenuItem unsupported
IN_PROC_BROWSER_TEST_F(IncognitoModeInSupervisedContextUiTest,
                       IncognitoModeIsNotAvailableToSupervisedUser) {
  ASSERT_TRUE(
      IncognitoModePrefs::IsIncognitoAllowed(child().browser().profile()));
  TurnOnSyncFor(child());

  ASSERT_FALSE(
      IncognitoModePrefs::IsIncognitoAllowed(child().browser().profile()));

  RunTestSequenceInContext(
      child().browser().window()->GetElementContext(),
      InstrumentTab(kWebContentsElementId),
      CheckCountOfIncognitoBrowsers(/*expected_count=*/0),
      PressButton(kToolbarAppMenuButtonElementId),
      CheckViewProperty(AppMenuModel::kIncognitoMenuItem,
                        &views::MenuItemView::GetEnabled, false),
      SelectMenuItem(AppMenuModel::kIncognitoMenuItem),
      CheckCountOfIncognitoBrowsers(/*expected_count=*/0));
}

// TODO(https://crbug.com/367205684): SelectMenuItem unsupported
IN_PROC_BROWSER_TEST_F(IncognitoModeInSupervisedContextUiTest,
                       IncognitoModeIsAvailableToHeadOfHousehold) {
  TurnOnSyncFor(head_of_household());
  ASSERT_TRUE(IncognitoModePrefs::IsIncognitoAllowed(
      head_of_household().browser().profile()));

  RunTestSequenceInContext(
      head_of_household().browser().window()->GetElementContext(),
      CheckCountOfIncognitoBrowsers(/*expected_count=*/0),
      PressButton(kToolbarAppMenuButtonElementId),
      CheckViewProperty(AppMenuModel::kIncognitoMenuItem,
                        &views::MenuItemView::GetEnabled, true),
      SelectMenuItem(AppMenuModel::kIncognitoMenuItem),
      CheckCountOfIncognitoBrowsers(/*expected_count=*/1));
}

}  // namespace
}  // namespace supervised_user
