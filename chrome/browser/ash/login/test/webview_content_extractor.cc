// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/webview_content_extractor.h"

#include <string_view>

#include "base/strings/stringprintf.h"
#include "base/uuid.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"

namespace ash {
namespace test {
namespace {

content::RenderFrameHost* FindFrame(const std::string& element_id) {
  // Tag the webview in use with a unique name.
  std::string unique_webview_name =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  OobeJS().Evaluate(
      base::StringPrintf("(function(){"
                         "  var webView = %s;"
                         "  webView.name = '%s';"
                         "})();",
                         element_id.c_str(), unique_webview_name.c_str()));

  // Find the WebViewGuest tagged with the unique name.
  content::RenderFrameHost* frame = nullptr;
  auto* const owner_contents =
      LoginDisplayHost::default_host()->GetOobeUI()->web_ui()->GetWebContents();
  owner_contents->ForEachRenderFrameHostWithAction(
      [&frame, &unique_webview_name](content::RenderFrameHost* rfh) {
        auto* web_view = extensions::WebViewGuest::FromRenderFrameHost(rfh);
        if (web_view && web_view->name() == unique_webview_name) {
          DCHECK_EQ(web_view->GetGuestMainFrame(), rfh);
          frame = rfh;
          return content::RenderFrameHost::FrameIterationAction::kStop;
        }
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });

  return frame;
}

}  // namespace

std::string GetWebViewContents(
    std::initializer_list<std::string_view> element_ids) {
  return GetWebViewContentsById(GetOobeElementPath(element_ids));
}

std::string GetWebViewContentsById(const std::string& element_id) {
  // Wait the contents to load.
  content::WaitForLoadStop(
      content::WebContents::FromRenderFrameHost(FindFrame(element_id)));

  return content::EvalJs(FindFrame(element_id), "document.body.textContent;")
      .ExtractString();
}

}  // namespace test
}  // namespace ash
