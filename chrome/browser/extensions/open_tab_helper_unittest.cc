// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/open_tab_helper.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/test_standalone_window_controller.h"
#include "chrome/browser/tab_list/mock_tab_list_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "url/gurl.h"

namespace extensions {

using OpenTabHelperUnitTest = ChromeRenderViewHostTestHarness;

// Verifies that OpenTabHelper::OpenTab returns kSplitViewCreationFailedError
// when split_with_tab_id specifies a tab in a standalone WindowController.
TEST_F(OpenTabHelperUnitTest, OpenTab_StandaloneSplitWithTabIdRejected) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(extensions_features::kApiTabsSplitView);

  std::unique_ptr<content::WebContents> standalone_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  sessions::SessionTabHelper::CreateForWebContents(standalone_contents.get(),
                                                   base::NullCallback());
  const int standalone_tab_id =
      ExtensionTabUtil::GetTabId(standalone_contents.get());

  const SessionID window_id = SessionID::NewUnique();
  TestStandaloneWindowController controller(
      /*base_window=*/nullptr, profile(), window_id, standalone_contents.get());

  testing::NiceMock<MockBrowserWindowInterface> mock_browser;
  ON_CALL(mock_browser, GetProfile()).WillByDefault(testing::Return(profile()));
  controller.SetBrowserWindowInterfaceForLookup(&mock_browser);
  testing::NiceMock<MockTabListInterface> mock_tab_list;
  ON_CALL(mock_tab_list, GetTabCount()).WillByDefault(testing::Return(1));
  ui::ScopedUnownedUserData<TabListInterface> tab_list_registration(
      mock_browser.GetUnownedUserDataHost(), mock_tab_list);

  auto extension = ExtensionBuilder("split_ext").Build();
  auto function = base::MakeRefCounted<TabsCreateFunction>();
  function->ignore_did_respond_for_testing();
  function->set_extension(extension);
  function->SetBrowserContextForTesting(profile());

  OpenTabHelper::Params params;
  params.split_with_tab_id = standalone_tab_id;

  auto result = OpenTabHelper::OpenTab(GURL("https://example.com"),
                                       mock_browser, *function, params);
  EXPECT_THAT(result, base::test::ErrorIs(
                          tabs_constants::kSplitViewCreationFailedError));
}

}  // namespace extensions
