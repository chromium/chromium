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
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "url/gurl.h"
#include "url/origin.h"

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

TEST_F(OpenTabHelperUnitTest,
       MaybeSetInitiatorOrigin_WebUrlSetsExtensionOrigin) {
  auto extension = ExtensionBuilder("test_ext").Build();
  auto function = base::MakeRefCounted<TabsCreateFunction>();
  function->set_extension(extension);

  NavigateParams params(profile(), GURL("https://example.com/"),
                        ui::PAGE_TRANSITION_LINK);
  OpenTabHelper::MaybeSetInitiatorOrigin(*function, params);

  EXPECT_EQ(params.initiator_origin, extension->origin());
}

TEST_F(OpenTabHelperUnitTest,
       MaybeSetInitiatorOrigin_AboutBlankSetsOpaqueOrigin) {
  auto extension = ExtensionBuilder("test_ext").Build();
  auto function = base::MakeRefCounted<TabsCreateFunction>();
  function->set_extension(extension);

  NavigateParams params(profile(), GURL("about:blank"),
                        ui::PAGE_TRANSITION_LINK);
  OpenTabHelper::MaybeSetInitiatorOrigin(*function, params);

  ASSERT_TRUE(params.initiator_origin.has_value());
  EXPECT_TRUE(params.initiator_origin->opaque());
}

TEST_F(OpenTabHelperUnitTest, MaybeSetInitiatorOrigin_ChromeUrlLeavesNullopt) {
  auto extension = ExtensionBuilder("test_ext").Build();
  auto function = base::MakeRefCounted<TabsCreateFunction>();
  function->set_extension(extension);

  NavigateParams params(profile(), GURL("chrome://newtab/"),
                        ui::PAGE_TRANSITION_LINK);
  OpenTabHelper::MaybeSetInitiatorOrigin(*function, params);

  EXPECT_FALSE(params.initiator_origin.has_value());
}

// Regression test for crbug.com/586047471.
TEST_F(OpenTabHelperUnitTest,
       MaybeSetInitiatorOrigin_OtherExtensionUrlLeavesNullopt) {
  auto extension = ExtensionBuilder("test_ext").Build();
  auto function = base::MakeRefCounted<TabsCreateFunction>();
  function->set_extension(extension);

  auto other_extension = ExtensionBuilder("other_ext").Build();
  NavigateParams params(profile(),
                        other_extension->GetResourceURL("viewer.html"),
                        ui::PAGE_TRANSITION_LINK);
  OpenTabHelper::MaybeSetInitiatorOrigin(*function, params);

  EXPECT_FALSE(params.initiator_origin.has_value());
}

// All `chrome-extension://` URLs, including the calling extension's own pages,
// leave `initiator_origin` unset.
TEST_F(OpenTabHelperUnitTest,
       MaybeSetInitiatorOrigin_ExtensionUrlsLeaveNullopt) {
  auto extension = ExtensionBuilder("test_ext").Build();
  auto function = base::MakeRefCounted<TabsCreateFunction>();
  function->set_extension(extension);

  for (const GURL& url :
       {GURL("chrome-extension://abcdefghijklmnopabcdefghijklmnop/page.html"),
        extension->GetResourceURL("page.html")}) {
    SCOPED_TRACE(url);
    NavigateParams params(profile(), url, ui::PAGE_TRANSITION_LINK);
    OpenTabHelper::MaybeSetInitiatorOrigin(*function, params);
    EXPECT_FALSE(params.initiator_origin.has_value());
  }
}

TEST_F(OpenTabHelperUnitTest,
       MaybeSetInitiatorOrigin_DataAndFileUrlsSetExtensionOrigin) {
  auto extension = ExtensionBuilder("test_ext").Build();
  auto function = base::MakeRefCounted<TabsCreateFunction>();
  function->set_extension(extension);

  for (const char* url : {"data:text/html,hello", "file:///tmp/test.html"}) {
    SCOPED_TRACE(url);
    NavigateParams params(profile(), GURL(url), ui::PAGE_TRANSITION_LINK);
    OpenTabHelper::MaybeSetInitiatorOrigin(*function, params);
    EXPECT_EQ(params.initiator_origin, extension->origin());
  }
}

TEST_F(OpenTabHelperUnitTest,
       MaybeSetInitiatorOrigin_BlobAndFileSystemUrlsSetExtensionOrigin) {
  auto extension = ExtensionBuilder("test_ext").Build();
  auto function = base::MakeRefCounted<TabsCreateFunction>();
  function->set_extension(extension);

  for (const char* url :
       {"blob:chrome-extension://abcdefghijklmnopabcdefghijklmnop/"
        "12345678-1234-1234-1234-123456789abc",
        "blob:https://example.com/12345678-1234-1234-1234-123456789abc",
        "filesystem:chrome-extension://abcdefghijklmnopabcdefghijklmnop/"
        "temporary/page.html",
        "filesystem:https://example.com/temporary/page.html",
        "view-source:filesystem:chrome-extension://"
        "abcdefghijklmnopabcdefghijklmnop/temporary/page.html"}) {
    SCOPED_TRACE(url);
    NavigateParams params(profile(), GURL(url), ui::PAGE_TRANSITION_LINK);
    OpenTabHelper::MaybeSetInitiatorOrigin(*function, params);
    EXPECT_EQ(params.initiator_origin, extension->origin());
  }
}

TEST_F(OpenTabHelperUnitTest,
       MaybeSetInitiatorOrigin_ExternalProtocolUrlsSetExtensionOrigin) {
  auto extension = ExtensionBuilder("test_ext").Build();
  auto function = base::MakeRefCounted<TabsCreateFunction>();
  function->set_extension(extension);

  for (const char* url : {"mailto:user@example.com", "local+app://open",
                          "custom-scheme://action"}) {
    SCOPED_TRACE(url);
    NavigateParams params(profile(), GURL(url), ui::PAGE_TRANSITION_LINK);
    OpenTabHelper::MaybeSetInitiatorOrigin(*function, params);
    EXPECT_EQ(params.initiator_origin, extension->origin());
  }
}

// `view-source:` URLs load the wrapped URL, so `initiator_origin` should be set
// when the wrapped URL would set it.
TEST_F(OpenTabHelperUnitTest,
       MaybeSetInitiatorOrigin_ViewSourceUrlSetsExtensionOrigin) {
  auto extension = ExtensionBuilder("test_ext").Build();
  auto function = base::MakeRefCounted<TabsCreateFunction>();
  function->set_extension(extension);

  for (const char* url :
       {"view-source:https://example.com/", "view-source:http://example.com/",
        "view-source:file:///tmp/test.html"}) {
    SCOPED_TRACE(url);
    NavigateParams params(profile(), GURL(url), ui::PAGE_TRANSITION_LINK);
    OpenTabHelper::MaybeSetInitiatorOrigin(*function, params);
    EXPECT_EQ(params.initiator_origin, extension->origin());
  }
}

// `view-source:` URLs wrapping `chrome-extension://` or `chrome://` URLs should
// leave `initiator_origin` unset, the same as the wrapped URL would.
TEST_F(OpenTabHelperUnitTest,
       MaybeSetInitiatorOrigin_ViewSourceExtensionAndChromeUrlsLeaveNullopt) {
  auto extension = ExtensionBuilder("test_ext").Build();
  auto function = base::MakeRefCounted<TabsCreateFunction>();
  function->set_extension(extension);

  for (const char* url :
       {"view-source:chrome-extension://abcdefghijklmnopabcdefghijklmnop/"
        "page.html",
        "view-source:chrome://version/"}) {
    SCOPED_TRACE(url);
    NavigateParams params(profile(), GURL(url), ui::PAGE_TRANSITION_LINK);
    OpenTabHelper::MaybeSetInitiatorOrigin(*function, params);
    EXPECT_FALSE(params.initiator_origin.has_value());
  }
}

TEST_F(OpenTabHelperUnitTest,
       MaybeSetInitiatorOrigin_HttpUrlSetsExtensionOrigin) {
  auto extension = ExtensionBuilder("test_ext").Build();
  auto function = base::MakeRefCounted<TabsCreateFunction>();
  function->set_extension(extension);

  NavigateParams params(profile(), GURL("http://example.com/"),
                        ui::PAGE_TRANSITION_LINK);
  OpenTabHelper::MaybeSetInitiatorOrigin(*function, params);

  EXPECT_EQ(params.initiator_origin, extension->origin());
}

TEST_F(OpenTabHelperUnitTest,
       MaybeSetInitiatorOrigin_PdfExtensionLeavesUnchanged) {
  auto pdf_extension = ExtensionBuilder("pdf_ext")
                           .SetID(extension_misc::kPdfExtensionId)
                           .Build();
  auto function = base::MakeRefCounted<TabsCreateFunction>();
  function->set_extension(pdf_extension);
  function->SetBrowserContextForTesting(profile());

  NavigateParams params(profile(), GURL("about:blank"),
                        ui::PAGE_TRANSITION_LINK);
  EXPECT_TRUE(OpenTabHelper::MaybeSetPdfNavigateParams(*function, params));
  OpenTabHelper::MaybeSetInitiatorOrigin(*function, params);

  EXPECT_EQ(params.initiator_origin, pdf_extension->origin());
}

TEST_F(OpenTabHelperUnitTest,
       MaybeSetInitiatorOrigin_NoExtensionLeavesNullopt) {
  auto function = base::MakeRefCounted<TabsCreateFunction>();
  function->set_extension(nullptr);

  NavigateParams params(profile(), GURL("https://example.com/"),
                        ui::PAGE_TRANSITION_LINK);
  OpenTabHelper::MaybeSetInitiatorOrigin(*function, params);

  EXPECT_FALSE(params.initiator_origin.has_value());
}

TEST_F(OpenTabHelperUnitTest,
       MaybeSetInitiatorOrigin_DisabledFeatureLeavesNullopt) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      extensions_features::kExtensionOpenTabSetInitiatorOrigin);

  auto extension = ExtensionBuilder("test_ext").Build();
  auto function = base::MakeRefCounted<TabsCreateFunction>();
  function->set_extension(extension);

  NavigateParams params(profile(), GURL("https://example.com/"),
                        ui::PAGE_TRANSITION_LINK);
  OpenTabHelper::MaybeSetInitiatorOrigin(*function, params);

  EXPECT_FALSE(params.initiator_origin.has_value());
}

}  // namespace extensions
