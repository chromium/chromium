// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_http_service_registry.h"

#include <ranges>

#include "chrome/browser/devtools/aida_service_handler.h"
#include "chrome/browser/devtools/gdp_service_handler.h"

namespace {

using Service = DevToolsHttpServiceRegistry::Service;
using Result = DevToolsHttpServiceHandler::Result;

void DispatchRequest(
    const std::vector<DevToolsHttpServiceRegistry::Service>& services,
    Profile* profile,
    const DevToolsDispatchHttpRequestParams& params,
    std::optional<DevToolsHttpServiceHandler::StreamWriter> stream_writer,
    DevToolsHttpServiceHandler::Callback callback) {
  // Service exists?
  auto service_it =
      std::ranges::find(services, params.service, &Service::service);
  if (service_it == services.end()) {
    auto result = std::make_unique<DevToolsHttpServiceHandler::Result>();
    result->error = DevToolsHttpServiceHandler::Result::Error::kServiceNotFound;
    std::move(callback).Run(std::move(result));
    return;
  }

  // Endpoint allowed?
  if (std::ranges::none_of(service_it->endpoints,
                           [&params](const Service::Endpoint& endpoint) {
                             return endpoint.path == params.path &&
                                    endpoint.method == params.method;
                           })) {
    auto result = std::make_unique<DevToolsHttpServiceHandler::Result>();
    result->error = DevToolsHttpServiceHandler::Result::Error::kAccessDenied;
    std::move(callback).Run(std::move(result));
    return;
  }

  service_it->handler->Request(profile, params, std::move(stream_writer),
                               std::move(callback));
}

}  // namespace

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
                                  {"/v1/aida:generateCode", "POST"},
                                  {"/v1/aida:doConversation", "POST"},
                              },
                              std::make_unique<AidaServiceHandler>()));
  services_.push_back(
      Service("gdpService",
              {
                  {"/v1beta1/profile:get", "GET"},
                  {"/v1beta1/eligibility:check", "GET"},
                  {"/v1beta1/profiles/me/awards", "GET"},
                  {"/v1beta1/profiles/me/awards:batchGet", "GET"},
                  {"/v1beta1/profiles", "POST"},
                  {"/v1beta1/profiles/me/awards", "POST"},
              },
              std::make_unique<GdpServiceHandler>()));
}
DevToolsHttpServiceRegistry::~DevToolsHttpServiceRegistry() = default;

void DevToolsHttpServiceRegistry::Request(
    Profile* profile,
    const DevToolsDispatchHttpRequestParams& params,
    DevToolsHttpServiceHandler::Callback callback) {
  DispatchRequest(services_, profile, params, std::nullopt,
                  std::move(callback));
}

void DevToolsHttpServiceRegistry::RequestAsStream(
    Profile* profile,
    const DevToolsDispatchHttpRequestParams& params,
    DevToolsHttpServiceHandler::StreamWriter stream_writer,
    DevToolsHttpServiceHandler::Callback callback) {
  DispatchRequest(services_, profile, params, std::move(stream_writer),
                  std::move(callback));
}
