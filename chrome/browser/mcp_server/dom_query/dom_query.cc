// Copyright 2026 The Chromium Authors
#include "chrome/browser/mcp_server/dom_query/dom_query.h"
#include "base/logging.h"

namespace mcp_server {
DOMQuery::DOMQuery() {}
DOMQuery::~DOMQuery() = default;
std::string DOMQuery::QuerySelector(const std::string& selector) { return "[]"; }
std::string DOMQuery::GetHTML() { return "<html></html>"; }
std::string DOMQuery::GetFrameTree() { return "[]"; }
}  // namespace mcp_server
