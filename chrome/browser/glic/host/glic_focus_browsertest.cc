// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/test_support/glic_api_test.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_commands.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/base_window.h"

namespace glic {
namespace {

class GlicFocusBrowserTest : public GlicApiBrowserTest {
 public:
  GlicFocusBrowserTest()
      : GlicApiBrowserTest(GlicTestJsPath("./glic_focus_browsertest.js")) {}
};

// Regression test for b/475260887. The autofocus <input> element in the client
// page does not receive focus on opening the side panel.
IN_PROC_BROWSER_TEST_F(GlicFocusBrowserTest, testFocusOnSidePanelOpen) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicFocusBrowserTest, testFocusOnInvoke) {
  glic::GlicInvokeOptions options(
      glic::Target(*GetTabListInterface()->GetActiveTab()),
      glic::mojom::InvocationSource::kAutofill);
  service()->Invoke(std::move(options));
  ExecuteJsTest();
}

// Regression test for b/504144250. The client page in the side panel does not
// lose page focus.
IN_PROC_BROWSER_TEST_F(GlicFocusBrowserTest, testBlurOnOmniboxFocus) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  chrome::FocusLocationBar(GetBrowserWindowInterface());

  ContinueJsTest();
}

class GlicReloadFocusBrowserTest
    : public GlicFocusBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  GlicReloadFocusBrowserTest() {
    const auto [no_webview, native_placeholder] = GetParam();
    scoped_feature_list_.InitWithFeatureStates(
        {{features::kGlicNoWebview, no_webview},
         {features::kGlicNativeLoadingPlaceholder, native_placeholder}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Regression test for b/583963962. Reloading an open Glic panel detaches the
// old WebContents before attaching the replacement; focus must remain in Glic
// when Glic held focus, and must not be stolen when another view held focus.
IN_PROC_BROWSER_TEST_P(GlicReloadFocusBrowserTest, testFocusPreservedOnReload) {
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitUntilGuestIsShowing(instance));
  ExecuteJsTest();

  instance->host().Reload();
  ASSERT_OK(WaitUntilGuestIsShowing(instance));
  ExecuteJsTest();

  // Moving focus to the location bar mid-reload must not be clobbered when the
  // replacement guest finishes loading.
  instance->host().Reload();
  chrome::FocusLocationBar(GetBrowser());
  ASSERT_OK(RunUntilEqual<bool>([&]() { return instance->HasFocus(); }, false,
                                "Expected Glic to lose focus mid-reload"));
  ASSERT_OK(WaitUntilGuestIsShowing(instance));
  EXPECT_FALSE(instance->HasFocus());

  // Reloading while the location bar already holds focus must not steal focus.
  instance->host().Reload();
  ASSERT_OK(WaitUntilGuestIsShowing(instance));
  EXPECT_FALSE(instance->HasFocus());
}

IN_PROC_BROWSER_TEST_P(GlicReloadFocusBrowserTest,
                       testFocusPreservedOnDetachedReload) {
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTabAndDetach());
  ASSERT_OK(WaitUntilGuestIsShowing(instance));
  ExecuteJsTest();

  instance->host().Reload();
  ASSERT_OK(WaitUntilGuestIsShowing(instance));
  ExecuteJsTest();

  // Activating the browser window mid-reload must not be clobbered when the
  // replacement guest finishes loading.
  instance->host().Reload();
  GetBrowser()->GetWindow()->Activate();
  ASSERT_OK(RunUntilEqual<bool>(
      [&]() { return instance->HasFocus(); }, false,
      "Expected detached Glic window to lose active focus mid-reload"));
  ASSERT_OK(WaitUntilGuestIsShowing(instance));
  EXPECT_FALSE(instance->HasFocus());

  // Reloading while the detached window is already inactive must not steal
  // window activation.
  instance->host().Reload();
  ASSERT_OK(WaitUntilGuestIsShowing(instance));
  EXPECT_FALSE(instance->HasFocus());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GlicReloadFocusBrowserTest,
    testing::Combine(testing::Bool(), testing::Bool()),
    [](const testing::TestParamInfo<std::tuple<bool, bool>>& info) {
      return base::StrCat(
          {std::get<0>(info.param) ? "NoWebview" : "Webview", "_",
           std::get<1>(info.param) ? "NativePlaceholder" : "WebUiLoader"});
    });

}  // namespace
}  // namespace glic
