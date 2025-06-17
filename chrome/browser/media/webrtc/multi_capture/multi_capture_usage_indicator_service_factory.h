// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_USAGE_INDICATOR_SERVICE_FACTORY_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_USAGE_INDICATOR_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace multi_capture {

class MultiCaptureUsageIndicatorService;

// This factory reacts to profile creation and instantiates profile-keyed
// services that manages usage indicators for the `getAllScreensMedia` API.
class MultiCaptureUsageIndicatorServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static MultiCaptureUsageIndicatorService* GetForBrowserContext(
      content::BrowserContext* context);
  static MultiCaptureUsageIndicatorServiceFactory* GetInstance();

  MultiCaptureUsageIndicatorServiceFactory(
      const MultiCaptureUsageIndicatorServiceFactory&) = delete;
  MultiCaptureUsageIndicatorServiceFactory& operator=(
      const MultiCaptureUsageIndicatorServiceFactory&) = delete;

 private:
  friend base::NoDestructor<MultiCaptureUsageIndicatorServiceFactory>;

  MultiCaptureUsageIndicatorServiceFactory();
  ~MultiCaptureUsageIndicatorServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace multi_capture

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_USAGE_INDICATOR_SERVICE_FACTORY_H_
