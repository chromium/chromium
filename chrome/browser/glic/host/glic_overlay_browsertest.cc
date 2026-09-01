// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "chrome/browser/glic/host/glic_overlay.mojom.h"
#include "chrome/browser/glic/host/glic_overlay_ui.h"
#include "chrome/browser/glic/host/glic_ui.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace glic {

namespace {

constexpr char kWaitForOverlayLoaded[] = R"(
  customElements.whenDefined('glic-overlay').then(() => true)
)";

class MockOverlayPageHandler : public mojom::GlicOverlayPageHandler {
 public:
  MOCK_METHOD(void, OnRetryClicked, (), (override));
  MOCK_METHOD(void, OnSignInClicked, (), (override));
  MOCK_METHOD(void, OnProfilePickerClicked, (), (override));
  MOCK_METHOD(void, OnIneligibleAccountHelpClicked, (), (override));
  MOCK_METHOD(void, OnLocationMismatchHelpClicked, (), (override));
  MOCK_METHOD(void, OnDisabledByAdminCloseClicked, (), (override));
  MOCK_METHOD(void, OnDisabledByAdminLinkClicked, (), (override));
  MOCK_METHOD(void, OnClosePanelClicked, (), (override));
};

}  // namespace

class GlicOverlayBrowserTest : public glic::GlicBrowserTest {
 public:
  GlicOverlayBrowserTest() = default;
};

IN_PROC_BROWSER_TEST_F(GlicOverlayBrowserTest,
                       OverlaySubpathInstantiatesGlicOverlayUI) {
  auto* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab);
  NavigateTab(*tab, GURL("chrome://glic/overlay"));
  content::WebContents* contents = tab->GetContents();
  ASSERT_TRUE(contents);
  ASSERT_TRUE(contents->GetWebUI());
  EXPECT_TRUE(contents->GetWebUI()->GetController()->GetAs<GlicOverlayUI>());
  EXPECT_FALSE(contents->GetWebUI()->GetController()->GetAs<GlicUI>());

  EXPECT_EQ(true, content::EvalJs(contents, kWaitForOverlayLoaded));
}

IN_PROC_BROWSER_TEST_F(GlicOverlayBrowserTest, SetsOverlayState) {
  auto* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab);
  NavigateTab(*tab, GURL("chrome://glic/overlay"));
  content::WebContents* contents = tab->GetContents();
  ASSERT_TRUE(contents);
  ASSERT_TRUE(contents->GetWebUI());
  auto* overlay_ui =
      contents->GetWebUI()->GetController()->GetAs<GlicOverlayUI>();
  ASSERT_TRUE(overlay_ui);
  EXPECT_EQ(true, content::EvalJs(contents, kWaitForOverlayLoaded));

  MockOverlayPageHandler page_handler;
  overlay_ui->SetPageHandler(&page_handler);

  overlay_ui->SetOverlayState(
      mojom::OverlayState::NewLoading(mojom::LoadingStyle::kFloating));

  EXPECT_EQ(true, content::EvalJs(contents, R"(
    new Promise((resolve, reject) => {
      const start = Date.now();
      const check = () => {
        const loadingPanel = document.getElementById('loadingPanel');
        if (loadingPanel && !loadingPanel.hidden) {
          resolve(true);
        } else if (Date.now() - start > 5000) {
          reject(new Error("Timeout waiting for loadingPanel to be unhidden."));
        } else {
          setTimeout(check, 50);
        }
      };
      check();
    })
  )"));
}

}  // namespace glic
