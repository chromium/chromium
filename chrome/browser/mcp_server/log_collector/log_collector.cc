// Copyright 2026 The Chromium Authors
#include "chrome/browser/mcp_server/log_collector/log_collector.h"

namespace mcp_server {
LogCollector::LogCollector() {}
LogCollector::~LogCollector() = default;
std::string LogCollector::GetLogs() { return "[]"; }
}  // namespace mcp_server
