// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_SESSION_CONTROLLER_FACTORY_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_SESSION_CONTROLLER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/webapps/isolated_web_apps/service/isolated_web_app_browser_context_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace multi_capture {

class MultiCaptureSessionController;

// This factory reacts to profile creation and instantiates profile-keyed
// services that manages the `stop_callback` for the `getAllScreensMedia` API.
class MultiCaptureSessionControllerFactory
    : public web_app::IsolatedWebAppBrowserContextServiceFactory {
 public:
  static MultiCaptureSessionController* GetForBrowserContext(
      content::BrowserContext* context);
  static MultiCaptureSessionControllerFactory* GetInstance();

  MultiCaptureSessionControllerFactory(
      const MultiCaptureSessionControllerFactory&) = delete;
  MultiCaptureSessionControllerFactory& operator=(
      const MultiCaptureSessionControllerFactory&) = delete;

 private:
  friend base::NoDestructor<MultiCaptureSessionControllerFactory>;

  MultiCaptureSessionControllerFactory();
  ~MultiCaptureSessionControllerFactory() override;

  // web_app::ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace multi_capture

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_SESSION_CONTROLLER_FACTORY_H_
