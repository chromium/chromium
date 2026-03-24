// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_WEBMCP_HANDLER_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_WEBMCP_HANDLER_H_

#include <memory>

#include "chrome/browser/devtools/protocol/web_mcp.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

class WebMCPHandler : public protocol::WebMCP::Backend {
 public:
  WebMCPHandler(protocol::UberDispatcher* dispatcher,
                content::WebContents* web_contents);

  WebMCPHandler(const WebMCPHandler&) = delete;
  WebMCPHandler& operator=(const WebMCPHandler&) = delete;

  ~WebMCPHandler() override;

  // protocol::WebMCP::Backend:
  protocol::Response Enable() override;

 private:
  bool enabled_ = false;
  std::unique_ptr<protocol::WebMCP::Frontend> frontend_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_WEBMCP_HANDLER_H_
