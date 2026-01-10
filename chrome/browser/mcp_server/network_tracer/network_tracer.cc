// Copyright 2026 The Chromium Authors
#include "chrome/browser/mcp_server/network_tracer/network_tracer.h"

namespace mcp_server {
NetworkTracer::NetworkTracer() {}
NetworkTracer::~NetworkTracer() = default;
std::string NetworkTracer::GetRequests() { return "[]"; }
std::string NetworkTracer::GetSummary() { return "{}"; }
}  // namespace mcp_server
