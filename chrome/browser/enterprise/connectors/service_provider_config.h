// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_SERVICE_PROVIDER_CONFIG_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_SERVICE_PROVIDER_CONFIG_H_

#include <array>
#include <map>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/values.h"

namespace enterprise_connectors {

struct SupportedTag {
  const char* name = nullptr;
  const char* display_name = nullptr;
  size_t max_file_size = -1;
};

struct AnalysisConfig {
  const char* url = nullptr;
  std::array<SupportedTag, 2> supported_tags;
};

struct ReportingConfig {
  const char* url = nullptr;
};

struct FileSystemConfig {
  const char* home = nullptr;
  const char* authorization_endpoint = nullptr;
  const char* token_endpoint = nullptr;
  size_t max_direct_size = -1;
  std::array<const char*, 0> scopes;
  std::array<const char*, 2> disable;
  const char* client_id = nullptr;
  const char* client_secret = nullptr;
};

struct ServiceProvider {
  const char* display_name;
  const AnalysisConfig* analysis = nullptr;
  const ReportingConfig* reporting = nullptr;
  const FileSystemConfig* file_system = nullptr;
};

using ServiceProviderConfig =
    base::fixed_flat_map<base::StringPiece, ServiceProvider, 2>;

// Returns the global service provider configuration, containing every service
// provider and each of their supported Connector configs.
const ServiceProviderConfig* GetServiceProviderConfig();

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_SERVICE_PROVIDER_CONFIG_H_
