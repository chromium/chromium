// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/service_discovery_client_test_helpers.h"

#include "chrome/browser/local_discovery/service_discovery_client_impl.h"

namespace local_discovery {

// MockServiceDiscoveryClient
MockServiceDiscoveryClient::MockServiceDiscoveryClient() = default;
MockServiceDiscoveryClient::~MockServiceDiscoveryClient() = default;

// MockServiceWatcher
MockServiceWatcher::MockServiceWatcher(UpdatedCallback callback)
    : updated_callback_(callback) {}
MockServiceWatcher::~MockServiceWatcher() = default;

void MockServiceWatcher::SimulateServiceUpdated(UpdateType update,
                                                std::string_view service_name) {
  updated_callback_.Run(update, std::string(service_name));
}

// MockServiceResolver
MockServiceResolver::MockServiceResolver(
    ServiceResolver::ResolveCompleteCallback callback)
    : resolve_complete_callback_(std::move(callback)) {}
MockServiceResolver::~MockServiceResolver() = default;

void MockServiceResolver::SimulateResolveComplete(
    RequestStatus status,
    const ServiceDescription& service_description) {
  if (resolve_complete_callback_) {
    std::move(resolve_complete_callback_).Run(status, service_description);
  }
}

// MockServiceDiscoveryDeviceListerDelegate
MockServiceDiscoveryDeviceListerDelegate::
    MockServiceDiscoveryDeviceListerDelegate() = default;
MockServiceDiscoveryDeviceListerDelegate::
    ~MockServiceDiscoveryDeviceListerDelegate() = default;

}  // namespace local_discovery
