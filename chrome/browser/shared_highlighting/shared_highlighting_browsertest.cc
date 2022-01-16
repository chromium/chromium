// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/link_to_text_menu_observer.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/base/clipboard/clipboard.h"

namespace shared_highlighting {
namespace {

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

}  // namespace

// Wait until text fragment matches are received.
class GetMatchesWaiter {
 public:
  GetMatchesWaiter() = default;
  ~GetMatchesWaiter() = default;

  void Wait() { run_loop_.Run(); }

  void OnMatchesRecieved(const std::vector<std::string>& matches) {
    matches_ = matches;
    run_loop_.Quit();
  }

  std::vector<std::string> GetMatches() { return matches_; }

 private:
  std::vector<std::string> matches_;
  base::RunLoop run_loop_;
};

class SharedHighlightingBrowserTest : public InProcessBrowserTest {
 public:
  SharedHighlightingBrowserTest(const SharedHighlightingBrowserTest&) = delete;
  SharedHighlightingBrowserTest& operator=(
      const SharedHighlightingBrowserTest&) = delete;

 protected:
  SharedHighlightingBrowserTest() = default;
  ~SharedHighlightingBrowserTest() override = default;

  // InProcessBrowserTest
  void SetUpCommandLine(base::CommandLine* command_line) override;

  // BrowserTestBase
  void SetUpOnMainThread() override;

  bool SelectTextInCurrentTab();
  int GetSelectionMidX();
  int GetSelectionMidY();
  std::u16string GetClipboardText();
  void OpenInNewTab(GURL url);
  std::string GetFirstHighlightedText();

  content::WebContents* GetCurrentTab();

 private:
  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request);

  std::string html_content_ = R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 2200px;
      }
      #first {
        position: absolute;
        top: 1000px;
      }
      #second {
        position: absolute;
        top: 2000px;
      }
    </style>
    <p id="selected">This is a test page</p>
    <p id="second">With some more text</p>
  )HTML";
};

void SharedHighlightingBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {}

void SharedHighlightingBrowserTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();

  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &SharedHighlightingBrowserTest::HandleRequest, base::Unretained(this)));
  ASSERT_TRUE(embedded_test_server()->Start());
}

bool SharedHighlightingBrowserTest::SelectTextInCurrentTab() {
  return content::ExecuteScript(
      GetCurrentTab(),
      "var node = document.getElementById('selected');"
      "if (document.body.createTextRange) {"
      "  const range = document.body.createTextRange();"
      "  range.moveToElementText(node);"
      "  range.select();"
      "} else if (window.getSelection) {"
      "  const selection = window.getSelection();"
      "  const range = document.createRange();"
      "  range.selectNodeContents(node);"
      "  selection.removeAllRanges();"
      "  selection.addRange(range);"
      "}");
}

int SharedHighlightingBrowserTest::GetSelectionMidX() {
  int x;
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
      GetCurrentTab(),
      "var bounds = document.getElementById('selected')"
      ".getBoundingClientRect();"
      "domAutomationController.send("
      "    Math.floor(bounds.left + bounds.width / 2));",
      &x));
  return x;
}

int SharedHighlightingBrowserTest::GetSelectionMidY() {
  int y;
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
      GetCurrentTab(),
      "var bounds = document.getElementById('selected')"
      ".getBoundingClientRect();"
      "domAutomationController.send("
      "    Math.floor(bounds.top + bounds.height / 2));",
      &y));
  return y;
}

std::u16string SharedHighlightingBrowserTest::GetClipboardText() {
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  std::u16string result;
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &result);
  return result;
}

void SharedHighlightingBrowserTest::OpenInNewTab(GURL url) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
}

std::string SharedHighlightingBrowserTest::GetFirstHighlightedText() {
  GetMatchesWaiter get_matches_waiter;
  mojo::Remote<blink::mojom::TextFragmentReceiver> remote;
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->GetMainFrame()
      ->GetRemoteInterfaces()
      ->GetInterface(remote.BindNewPipeAndPassReceiver());
  remote->ExtractTextFragmentsMatches(
      base::BindOnce(&GetMatchesWaiter::OnMatchesRecieved,
                     base::Unretained(&get_matches_waiter)));
  get_matches_waiter.Wait();

  return get_matches_waiter.GetMatches()[0];
}

content::WebContents* SharedHighlightingBrowserTest::GetCurrentTab() {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

std::unique_ptr<HttpResponse> SharedHighlightingBrowserTest::HandleRequest(
    const HttpRequest& request) {
  auto response = std::make_unique<BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content(html_content_);
  response->set_content_type("text/html; charset=utf-8");
  return std::move(response);
}

// Wait for a link to text generation completion. If successful "Copy link to
// text" menu options will be enabled.
class GenerationCompleteObserver {
 public:
  GenerationCompleteObserver() {
    LinkToTextMenuObserver::RegisterGenerationCompleteCallbackForTesting(
        base::BindOnce(&GenerationCompleteObserver::OnGenerationComplete,
                       base::Unretained(this)));
  }
  ~GenerationCompleteObserver() = default;

  void Wait() { run_loop_.Run(); }

  std::string GetSelector() const { return selector_; }

 private:
  void OnGenerationComplete(const std::string& selector) {
    selector_ = selector;
    run_loop_.Quit();
  }

  std::string selector_;
  base::RunLoop run_loop_;
};

// Wait for context menu to be shown and use the menu handler to execute menu
// option commands.
class ContextMenuObserver {
 public:
  ContextMenuObserver() {
    RenderViewContextMenu::RegisterMenuShownCallbackForTesting(base::BindOnce(
        &ContextMenuObserver::MenuShown, base::Unretained(this)));
  }

  ~ContextMenuObserver() = default;

  void WaitForMenuShown() { run_loop_.Run(); }

  void ExecuteCommand(int command_to_execute) {
    context_menu_->ExecuteCommand(command_to_execute, 0);
  }

 private:
  void MenuShown(RenderViewContextMenu* context_menu) {
    context_menu_ = context_menu;
    run_loop_.Quit();
  }

  RenderViewContextMenu* context_menu_;
  base::RunLoop run_loop_;
};

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_LACROS)
// Disabled because it fails for mac specific context menu:
// TODO(crbug.com/1275253): Flakily crashes under Windows and Mac.
// TODO(crbug.com/1276463): Flakily crashes under lacros.
#define MAYBE_LinkGenerationTest DISABLED_LinkGenerationTest
#else
#define MAYBE_LinkGenerationTest LinkGenerationTest
#endif
IN_PROC_BROWSER_TEST_F(SharedHighlightingBrowserTest,
                       MAYBE_LinkGenerationTest) {
  // Load the URL.
  auto url = embedded_test_server()->GetURL("/test.html");
  ASSERT_NO_FATAL_FAILURE(
      ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url)));

  // Select the text, with element id 'selected'.
  ASSERT_TRUE(SelectTextInCurrentTab());

  // Find the coordinates to click at to show the context menu
  int x = GetSelectionMidX();
  int y = GetSelectionMidY();

  // Right-click on selection and wait for context menu to show up.
  GenerationCompleteObserver generation_waiter;
  ContextMenuObserver menu_waiter;
  content::SimulateMouseClickAt(GetCurrentTab(), /*modifiers=*/0,
                                blink::WebMouseEvent::Button::kRight,
                                gfx::Point(x, y));
  menu_waiter.WaitForMenuShown();

  // Wait until link to text generation is complete and "Copy link to text" menu
  // option is enabled to execute that option.
  generation_waiter.Wait();
  ASSERT_NE(std::string(), generation_waiter.GetSelector());
  menu_waiter.ExecuteCommand(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT);

  // Get the generated link to text from clipboard.
  std::u16string link_to_text_str = GetClipboardText();
  EXPECT_TRUE(
      base::StartsWith(link_to_text_str, base::UTF8ToUTF16(url.spec())));

  // Navigate to link to text in a new tab.
  OpenInNewTab(GURL(link_to_text_str));

  // Extract and check that highlighted text matches the selected text.
  EXPECT_EQ("This is a test page", GetFirstHighlightedText());
}

}  // namespace shared_highlighting
