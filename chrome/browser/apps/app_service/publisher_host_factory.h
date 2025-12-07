// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHER_HOST_FACTORY_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHER_HOST_FACTORY_H_

#include <memory>

#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"

namespace apps {

class PublisherHost;

class PublisherHostFactory {
 public:
  virtual ~PublisherHostFactory() = default;

  virtual std::unique_ptr<PublisherHost> CreatePublisherHost(
      AppServiceProxy* proxy) = 0;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHER_HOST_FACTORY_H_
