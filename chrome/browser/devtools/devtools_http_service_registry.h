// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_HTTP_SERVICE_REGISTRY_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_HTTP_SERVICE_REGISTRY_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/devtools/devtools_dispatch_http_request_params.h"
#include "chrome/browser/devtools/devtools_http_service_handler.h"

class Profile;

// Maps DevTools HTTP services to their handler and allowed endpoints.
//
// This is owned by SECURITY_OWNER to review new services and endpoints.
class DevToolsHttpServiceRegistry {
 public:
  DevToolsHttpServiceRegistry();
  ~DevToolsHttpServiceRegistry();

  struct Service {
    std::string service;
    struct Endpoint {
      std::string path;
      std::string method;
    };
    std::vector<Endpoint> endpoints;
    std::unique_ptr<DevToolsHttpServiceHandler> handler;

    Service(std::string service,
            std::vector<Endpoint> endpoints,
            std::unique_ptr<DevToolsHttpServiceHandler> handler);
    Service(Service&&);
    ~Service();
  };

  void Request(Profile* profile,
               const DevToolsDispatchHttpRequestParams& params,
               DevToolsHttpServiceHandler::Callback callback);

  void AddForTesting(Service service) {
    services_.push_back(std::move(service));
  }

 private:
  std::vector<Service> services_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_HTTP_SERVICE_REGISTRY_H_
