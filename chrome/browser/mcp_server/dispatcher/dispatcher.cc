// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mcp_server/dispatcher/dispatcher.h"

#include "base/logging.h"

namespace mcp_server {

Dispatcher::Dispatcher() {
  LOG(INFO) << "Dispatcher initialized";
}

Dispatcher::~Dispatcher() = default;

void Dispatcher::RegisterRoutes() {
  // TODO: Register API routes
  // GET /mcp/tabs
  // POST /mcp/tabs
  // DELETE /mcp/tabs/:id
  // POST /mcp/tabs/:id/activate
  // GET /mcp/tabs/:id/state
  // POST /click
  // POST /type
  // POST /scroll
  // GET /dom/query
  // GET /html
  // GET /screenshot
  // GET /logs
  // GET /network
}

std::string Dispatcher::HandleRequest(const std::string& method,
                                       const std::string& path,
                                       const std::string& body) {
  LOG(INFO) << "Handling request: " << method << " " << path;

  // TODO: Route to appropriate handler
  return "{\"status\": \"not_implemented\"}";
}

}  // namespace mcp_server
