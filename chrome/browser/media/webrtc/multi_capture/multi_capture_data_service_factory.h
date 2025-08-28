// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_DATA_SERVICE_FACTORY_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_DATA_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/webapps/isolated_web_apps/service/isolated_web_app_browser_context_service_factory.h"

namespace content {
class BrowserContext;
}

namespace multi_capture {

class MultiCaptureDataService;

// This factory reacts to profile creation and instantiates a service that
// manages policy and component data for the `getAllScreensMedia` API.
class MultiCaptureDataServiceFactory
    : public web_app::IsolatedWebAppBrowserContextServiceFactory {
 public:
  static MultiCaptureDataService* GetForBrowserContext(
      content::BrowserContext* context);
  static MultiCaptureDataServiceFactory* GetInstance();

  MultiCaptureDataServiceFactory(const MultiCaptureDataServiceFactory&) =
      delete;
  MultiCaptureDataServiceFactory& operator=(
      const MultiCaptureDataServiceFactory&) = delete;

 private:
  friend base::NoDestructor<MultiCaptureDataServiceFactory>;

  MultiCaptureDataServiceFactory();
  ~MultiCaptureDataServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace multi_capture

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_DATA_SERVICE_FACTORY_H_
