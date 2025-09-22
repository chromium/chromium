// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_PUBLISHER_HOST_FACTORY_IMPL_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_PUBLISHER_HOST_FACTORY_IMPL_H_

#include "chrome/browser/apps/app_service/publisher_host_factory.h"

namespace apps {

class PublisherHostFactoryImpl : public PublisherHostFactory {
 public:
  PublisherHostFactoryImpl();
  PublisherHostFactoryImpl(const PublisherHostFactoryImpl&) = delete;
  PublisherHostFactoryImpl& operator=(const PublisherHostFactoryImpl&) = delete;
  ~PublisherHostFactoryImpl() override;

  // PublisherHostFactory override:
  std::unique_ptr<PublisherHost> CreatePublisherHost(
      AppServiceProxy* proxy) override;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_PUBLISHER_HOST_FACTORY_IMPL_H_
