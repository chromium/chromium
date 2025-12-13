// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/multi_capture/multi_capture_usage_indicator_service_factory.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/media/webrtc/multi_capture/multi_capture_data_service_factory.h"
#include "chrome/browser/media/webrtc/multi_capture/multi_capture_usage_indicator_service.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "multi_capture_data_service_factory.h"

static_assert(BUILDFLAG(IS_CHROMEOS), "For ChromeOS only");

namespace multi_capture {

MultiCaptureUsageIndicatorService*
MultiCaptureUsageIndicatorServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<MultiCaptureUsageIndicatorService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

MultiCaptureUsageIndicatorServiceFactory*
MultiCaptureUsageIndicatorServiceFactory::GetInstance() {
  static base::NoDestructor<MultiCaptureUsageIndicatorServiceFactory> instance;
  return instance.get();
}

MultiCaptureUsageIndicatorServiceFactory::
    MultiCaptureUsageIndicatorServiceFactory()
    : web_app::IsolatedWebAppBrowserContextServiceFactory(
          "MultiCaptureUsageIndicatorServiceFactory") {
  DependsOn(MultiCaptureDataServiceFactory::GetInstance());
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
}

MultiCaptureUsageIndicatorServiceFactory::
    ~MultiCaptureUsageIndicatorServiceFactory() = default;

std::unique_ptr<KeyedService>
MultiCaptureUsageIndicatorServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  CHECK(profile);
  return MultiCaptureUsageIndicatorService::Create(
      profile, profile->GetPrefs(),
      MultiCaptureDataServiceFactory::GetForBrowserContext(profile));
}

bool MultiCaptureUsageIndicatorServiceFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace multi_capture
