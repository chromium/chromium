// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/page_capture/page_capture_api.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "url/gurl.h"

namespace extensions {

// A fake window controller so that GetTabById can find our
// WebContents. We return dummy values everywhere except
// `GetBrowserWindowInterface` and `GetWebContentsAt`.
class TestWindowController : public WindowController {
 public:
  TestWindowController(Profile* profile, content::WebContents* contents)
      : WindowController(nullptr, profile), contents_(contents) {
    WindowControllerList::GetInstance()->AddExtensionWindow(this);
  }
  ~TestWindowController() override {
    WindowControllerList::GetInstance()->RemoveExtensionWindow(this);
  }
  int GetWindowId() const override { return 1; }
  std::string GetWindowTypeText() const override { return "normal"; }
  void SetFullscreenMode(bool is_fullscreen,
                         const GURL& extension_url) const override {}
  content::WebContents* GetActiveTab() const override { return contents_; }
  int GetTabCount() const override { return 1; }
  content::WebContents* GetWebContentsAt(int i) const override {
    return contents_;
  }
  bool IsVisibleToTabsAPIForExtension(
      const Extension* extension,
      bool include_dev_tools_windows) const override {
    return true;
  }
  base::DictValue CreateWindowValueForExtension(
      const Extension* extension,
      PopulateTabBehavior populate_tab_behavior,
      mojom::ContextType context) const override {
    return base::DictValue();
  }
  base::ListValue CreateTabList(const Extension* extension,
                                mojom::ContextType context) const override {
    return base::ListValue();
  }
  bool OpenOptionsPage(const Extension* extension,
                       const GURL& url,
                       bool open_in_tab) override {
    return false;
  }
  BrowserWindowInterface* GetBrowserWindowInterface() override {
    return &browser_window_interface_;
  }

 private:
  raw_ptr<content::WebContents> contents_;
  testing::NiceMock<MockBrowserWindowInterface> browser_window_interface_;
};

class PageCaptureApiUnitTest : public ExtensionServiceTestBase {
 protected:
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
  }

  void TearDown() override { ExtensionServiceTestBase::TearDown(); }
};

// Tests that if a page navigates during a call to pageCature.saveAsMHTML(), the
// API call will result in an error.
TEST_F(PageCaptureApiUnitTest, PageNavigationDuringSaveAsMHTML) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Page Capture").AddAPIPermission("pageCapture").Build();
  auto function = base::MakeRefCounted<PageCaptureSaveAsMHTMLFunction>();
  function->set_extension(extension.get());

  // Add a visible tab.
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents.get());
  content::WebContents* raw_web_contents = web_contents.get();
  sessions::SessionTabHelper::CreateForWebContents(
      raw_web_contents,
      base::BindRepeating(
          [](content::WebContents*) -> sessions::SessionTabHelperDelegate* {
            return nullptr;
          }));
  TestWindowController window_controller(profile(), raw_web_contents);
  web_contents_tester->NavigateAndCommit(GURL("https://www.google.com"));
  const int tab_id = ExtensionTabUtil::GetTabId(raw_web_contents);

  // To capture the page as MHTML, the extension function needs to hop from the
  // UI thread to the IO thread to create the temporary file, then back to the
  // UI thread to actually save the page contents. Since the URL access is only
  // checked initially and a navigation could happen during the thread hops, the
  // extension function should result in an error if a navigation happens
  // between the initial check and the actual capture. To simulate this we start
  // the extension function running, then trigger a synchronous navigation
  // using the WebContentsTester immediately which will happen before the
  // messaging between threads finishes.
  // Regression test for crbug.com/1494490
  function->SetBrowserContextForTesting(profile());
  function->SetArgs(
      base::ListValue().Append(base::DictValue().Set("tabId", tab_id)));
  api_test_utils::SendResponseHelper response_helper(function.get());
  function->RunWithValidation().Execute();

  web_contents_tester->NavigateAndCommit(GURL("https://www.chromium.org"));
  response_helper.WaitForResponse();
  const base::ListValue* results = function->GetResultListForTest();
  ASSERT_TRUE(results);
  EXPECT_TRUE(results->empty()) << "Did not expect a result";
  CHECK(function->response_type());
  EXPECT_EQ(ExtensionFunction::ResponseType::kFailed,
            *function->response_type());
  EXPECT_EQ("Tab navigated before capture could complete.",
            function->GetError());
}

}  // namespace extensions
