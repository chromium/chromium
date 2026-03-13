// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/publisher_host_factory_impl.h"

#include <memory>

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/publishers/publisher_host_impl.h"

namespace apps {

PublisherHostFactoryImpl::PublisherHostFactoryImpl() = default;
PublisherHostFactoryImpl::~PublisherHostFactoryImpl() = default;

std::unique_ptr<PublisherHost> PublisherHostFactoryImpl::CreatePublisherHost(
    AppServiceProxyBase* proxy) {
  return std::make_unique<PublisherHostImpl>(
      static_cast<AppServiceProxy*>(proxy));
}

}  // namespace apps
