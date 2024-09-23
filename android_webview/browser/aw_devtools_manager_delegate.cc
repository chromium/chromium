// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_devtools_manager_delegate.h"

#include "android_webview/browser/gfx/browser_view_renderer.h"
#include "android_webview/common/aw_content_client.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"

using content::DevToolsAgentHost;

namespace android_webview {

AwDevToolsManagerDelegate::AwDevToolsManagerDelegate() = default;

AwDevToolsManagerDelegate::~AwDevToolsManagerDelegate() = default;

std::string AwDevToolsManagerDelegate::GetTargetDescription(
    content::WebContents* web_contents) {
  android_webview::BrowserViewRenderer* bvr =
      android_webview::BrowserViewRenderer::FromWebContents(web_contents);
  if (!bvr)
    return "";
  base::Value::Dict description;
  description.Set("attached", bvr->attached_to_window());
  description.Set("never_attached", !bvr->was_attached());
  description.Set("visible", bvr->IsVisible());
  gfx::Rect screen_rect = bvr->GetScreenRect();
  description.Set("screenX", screen_rect.x());
  description.Set("screenY", screen_rect.y());
  description.Set("empty", screen_rect.size().IsEmpty());
  if (!screen_rect.size().IsEmpty()) {
    description.Set("width", screen_rect.width());
    description.Set("height", screen_rect.height());
  }
  std::string json;
  base::JSONWriter::Write(description, &json);
  return json;
}

std::string AwDevToolsManagerDelegate::GetDiscoveryPageHTML() {
  const char html[] =
      "<html>"
      "<head><title>WebView remote debugging</title></head>"
      "<body>Please use <a href=\'chrome://inspect\'>chrome://inspect</a>"
      "</body>"
      "</html>";
  return html;
}

bool AwDevToolsManagerDelegate::IsBrowserTargetDiscoverable() {
  return true;
}

content::DevToolsAgentHost::List
AwDevToolsManagerDelegate::RemoteDebuggingTargets(TargetType target_type) {
  DevToolsAgentHost::List result;
  std::set<content::WebContents*> targets_web_contents;
  DevToolsAgentHost::List agents = DevToolsAgentHost::GetOrCreateAll();
  for (DevToolsAgentHost::List::iterator it = agents.begin();
       it != agents.end(); ++it) {
    if (content::WebContents* web_contents = (*it)->GetWebContents()) {
      if (targets_web_contents.find(web_contents) !=
          targets_web_contents.end()) {
        continue;
      }
      targets_web_contents.insert(web_contents);
    }
    result.push_back(*it);
  }
  return result;
}
}  // namespace android_webview
