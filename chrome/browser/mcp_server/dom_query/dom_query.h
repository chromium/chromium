// Copyright 2026 The Chromium Authors
#ifndef CHROME_BROWSER_MCP_SERVER_DOM_QUERY_DOM_QUERY_H_
#define CHROME_BROWSER_MCP_SERVER_DOM_QUERY_DOM_QUERY_H_

#include <string>

namespace mcp_server {
class DOMQuery {
 public:
  DOMQuery();
  ~DOMQuery();
  std::string QuerySelector(const std::string& selector);
  std::string GetHTML();
  std::string GetFrameTree();
};
}  // namespace mcp_server
#endif
