// Copyright 2026 The Chromium Authors
#ifndef CHROME_BROWSER_MCP_SERVER_NETWORK_TRACER_NETWORK_TRACER_H_
#define CHROME_BROWSER_MCP_SERVER_NETWORK_TRACER_NETWORK_TRACER_H_

#include <string>

namespace mcp_server {
class NetworkTracer {
 public:
  NetworkTracer();
  ~NetworkTracer();
  std::string GetRequests();
  std::string GetSummary();
};
}  // namespace mcp_server
#endif
