// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

#include "base/command_line.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_test_util.h"
#include "chrome/browser/glic/interactive_glic_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);

class GlicApiTest : public test::InteractiveGlicTest {
 public:
  GlicApiTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kGlicHostLogging);
    SetGlicPagePath("/glic/test.html");
  }
  ~GlicApiTest() override = default;

  void ExecuteJsTest(std::string_view name) {
    content::WebContents* web_contents = FindGlicGuestWebContents();
    ASSERT_TRUE(web_contents);
    ASSERT_THAT(content::EvalJs(web_contents,
                                base::StrCat({"runApiTest('", name, "')"})),
                testing::Eq("pass"));
  }

  content::WebContents* FindGlicGuestWebContents() {
    GlicKeyedService* glic =
        GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());
    for (GlicPageHandler* handler : glic->GetPageHandlersForTesting()) {
      if (handler->guest_contents()) {
        return handler->guest_contents();
      }
    }
    return nullptr;
  }

  base::test::ScopedFeatureList features_{features::kGlicScrollTo};
};

class GlicApiTestWithOneTab : public GlicApiTest {
 public:
  void SetUpOnMainThread() override {
    GlicApiTest::SetUpOnMainThread();

    // Load the test page in a tab, so that there is some page context.
    GURL page_url =
        InProcessBrowserTest::embedded_test_server()->GetURL("/glic/test.html");
    RunTestSequence(InstrumentTab(kFirstTab),
                    NavigateWebContents(kFirstTab, page_url),
                    OpenGlicWindow(GlicWindowMode::kAttached,
                                   GlicInstrumentMode::kHostAndContents));
  }
};

// TODO(harringtond): Many of these tests are minimal, and could be improved
// with additional cases and additional assertions.

IN_PROC_BROWSER_TEST_F(GlicApiTest, CreateTab) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest("testCreateTab");
  // TODO(harringtond): Add assertions to verify a tab was created.
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, OpenGlicSettingsPage) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest("testOpenGlicSettingsPage");
  // TODO(harringtond): Add assertions to verify the settings page opened.
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, ClosePanel) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest("testClosePanel");
  // TODO(harringtond): Assert panel is actually closed.
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, AttachPanel) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest("testAttachPanel");
  // TODO(harringtond): Assert panel is actually attached.
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, DetachPanel) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kAttached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest("testDetachPanel");
  // TODO(harringtond): Assert panel is actually detached.
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, ShowProfilePicker) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kAttached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest("testShowProfilePicker");
  // TODO(harringtond): Assert picker is shown, and try to test changing
  // profiles.
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, PanelActive) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kAttached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest("testPanelActive");
  // TODO(harringtond): Test deactivating the panel.
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, CanAttachPanel) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest("testCanAttachPanel");
  // TODO(harringtond): Test case where the canAttachPanel returns false.
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, GetFocusedTabStateV2) {
  ExecuteJsTest("testGetFocusedTabStateV2");
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab,
                       testGetContextFromFocusedTabWithoutPermission) {
  ExecuteJsTest("testGetContextFromFocusedTabWithoutPermission");
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab,
                       testGetContextFromFocusedTabWithNoRequestedData) {
  ExecuteJsTest("testGetContextFromFocusedTabWithNoRequestedData");
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab,
                       testGetContextFromFocusedTabWithAllRequestedData) {
  ExecuteJsTest("testGetContextFromFocusedTabWithAllRequestedData");
}

// TODO(harringtond): Fix this, it hangs.
IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, DISABLED_testCaptureScreenshot) {
  ExecuteJsTest("testCaptureScreenshot");
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testPermissionAccess) {
  ExecuteJsTest("testPermissionAccess");
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testGetUserProfileInfo) {
  ExecuteJsTest("testGetUserProfileInfo");
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testRefreshSignInCookies) {
  ExecuteJsTest("testRefreshSignInCookies");
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testSetContextAccessIndicator) {
  ExecuteJsTest("testSetContextAccessIndicator");
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testSetAudioDucking) {
  ExecuteJsTest("testSetAudioDucking");
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testMetrics) {
  ExecuteJsTest("testMetrics");
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testScrollToFindsText) {
  ExecuteJsTest("testScrollToFindsText");
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testScrollToNoMatchFound) {
  ExecuteJsTest("testScrollToNoMatchFound");
}

}  // namespace
}  // namespace glic
