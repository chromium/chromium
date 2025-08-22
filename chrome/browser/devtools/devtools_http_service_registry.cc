// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_http_service_registry.h"

#include <ranges>

#include "chrome/browser/devtools/aida_service_handler.h"

DevToolsHttpServiceRegistry::Service::Service(
    std::string service,
    std::vector<Endpoint> endpoints,
    std::unique_ptr<DevToolsHttpServiceHandler> handler)
    : service(std::move(service)),
      endpoints(std::move(endpoints)),
      handler(std::move(handler)) {}
DevToolsHttpServiceRegistry::Service::~Service() = default;
DevToolsHttpServiceRegistry::Service::Service(Service&&) = default;

DevToolsHttpServiceRegistry::DevToolsHttpServiceRegistry() {
  services_.push_back(Service("aidaService",
                              {
                                  {"/v1/aida:codeComplete", "POST"},
                                  {"/v1/registerClientEvent", "POST"},
                              },
                              std::make_unique<AidaServiceHandler>()));
}
DevToolsHttpServiceRegistry::~DevToolsHttpServiceRegistry() = default;

void DevToolsHttpServiceRegistry::Request(
    Profile* profile,
    const std::string& service,
    const std::string& path,
    const std::string& method,
    const std::optional<std::string>& body,
    DevToolsHttpServiceHandler::Callback callback) {
  // Service exists?
  auto service_it = std::ranges::find(services_, service, &Service::service);
  if (service_it == services_.end()) {
    auto result = std::make_unique<DevToolsHttpServiceHandler::Result>();
    result->error = DevToolsHttpServiceHandler::Result::Error::kServiceNotFound;
    std::move(callback).Run(std::move(result));
    return;
  }

  // Endpoint allowed?
  if (std::ranges::none_of(service_it->endpoints,
                           [&path, &method](const Service::Endpoint& endpoint) {
                             return endpoint.path == path &&
                                    endpoint.method == method;
                           })) {
    auto result = std::make_unique<DevToolsHttpServiceHandler::Result>();
    result->error = DevToolsHttpServiceHandler::Result::Error::kAccessDenied;
    std::move(callback).Run(std::move(result));
    return;
  }

  service_it->handler->Request(profile, path, method, body,
                               std::move(callback));
}
