// Copyright 2026 The Chromium Authors
#ifndef CHROME_BROWSER_MCP_SERVER_LOG_COLLECTOR_LOG_COLLECTOR_H_
#define CHROME_BROWSER_MCP_SERVER_LOG_COLLECTOR_LOG_COLLECTOR_H_

#include <string>

namespace mcp_server {
class LogCollector {
 public:
  LogCollector();
  ~LogCollector();
  std::string GetLogs();
};
}  // namespace mcp_server
#endif
