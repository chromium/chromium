// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/webmcp_handler.h"

#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/devtools/protocol/web_mcp.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

WebMCPHandler::WebMCPHandler(protocol::UberDispatcher* dispatcher,
                             content::WebContents* web_contents) {
  protocol::WebMCP::Dispatcher::wire(dispatcher, this);
  frontend_ =
      std::make_unique<protocol::WebMCP::Frontend>(dispatcher->channel());
}

WebMCPHandler::~WebMCPHandler() = default;

protocol::Response WebMCPHandler::Enable() {
  if (enabled_) {
    // Fall through to the renderer to make sure it's enabled as well if
    // necessary.
    return protocol::Response::FallThrough();
  }
  enabled_ = true;
  // Fall through to the renderer to make sure it's enabled as well if
  // necessary.
  return protocol::Response::FallThrough();
}
