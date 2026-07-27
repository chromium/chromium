// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/multi_capture/multi_capture_session_controller_factory.h"

#include "chrome/browser/media/webrtc/multi_capture/multi_capture_session_controller.h"
#include "chrome/browser/profiles/profile.h"

namespace multi_capture {

// static
MultiCaptureSessionController*
MultiCaptureSessionControllerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<MultiCaptureSessionController*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
MultiCaptureSessionControllerFactory*
MultiCaptureSessionControllerFactory::GetInstance() {
  static base::NoDestructor<MultiCaptureSessionControllerFactory> instance;
  return instance.get();
}

MultiCaptureSessionControllerFactory::MultiCaptureSessionControllerFactory()
    : web_app::IsolatedWebAppBrowserContextServiceFactory(
          "MultiCaptureSessionControllerFactory") {}

MultiCaptureSessionControllerFactory::~MultiCaptureSessionControllerFactory() =
    default;

std::unique_ptr<KeyedService>
MultiCaptureSessionControllerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<MultiCaptureSessionController>();
}

}  // namespace multi_capture
