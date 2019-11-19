// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/guid.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/webview_content_extractor.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"

namespace chromeos {
namespace test {

namespace {

class WebContentsLoadFinishedWaiter : public content::WebContentsObserver {
 public:
  explicit WebContentsLoadFinishedWaiter(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ~WebContentsLoadFinishedWaiter() override = default;

  void Wait() {
    if (!web_contents()->IsLoading())
      return;

    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

 private:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& url) override {
    if (run_loop_)
      run_loop_->Quit();
  }

  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsLoadFinishedWaiter);
};

// Helper invoked by GuestViewManager::ForEachGuest to collect WebContents of
// Webview named as |web_view_name|.
bool AddNamedWebContentsToSet(std::set<content::WebContents*>* frame_set,
                              const std::string& web_view_name,
                              content::WebContents* web_contents) {
  auto* web_view = extensions::WebViewGuest::FromWebContents(web_contents);
  if (web_view && web_view->name() == web_view_name)
    frame_set->insert(web_contents);
  return false;
}

content::WebContents* FindContents(std::string element_id) {
  // Tag the webview in use with a unique name.
  std::string unique_webview_name = base::GenerateGUID();
  test::OobeJS().Evaluate(
      base::StringPrintf("(function(){"
                         "  var webView = %s;"
                         "  webView.name = '%s';"
                         "})();",
                         element_id.c_str(), unique_webview_name.c_str()));

  // Find the WebContents tagged with the unique name.
  std::set<content::WebContents*> frame_set;
  auto* const owner_contents =
      LoginDisplayHost::default_host()->GetOobeUI()->web_ui()->GetWebContents();
  auto* const manager = guest_view::GuestViewManager::FromBrowserContext(
      owner_contents->GetBrowserContext());
  manager->ForEachGuest(
      owner_contents, base::BindRepeating(&AddNamedWebContentsToSet, &frame_set,
                                          unique_webview_name));
  CHECK_EQ(1u, frame_set.size());
  return *frame_set.begin();
}

}  // namespace

std::string GetWebViewContents(
    std::initializer_list<base::StringPiece> element_ids) {
  std::string element_id = test::GetOobeElementPath(element_ids);
  // Wait the contents to load.
  WebContentsLoadFinishedWaiter(FindContents(element_id)).Wait();

  std::string text;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      FindContents(element_id),
      "window.domAutomationController.send(document.body.textContent);",
      &text));

  return text;
}

}  // namespace test
}  // namespace chromeos
