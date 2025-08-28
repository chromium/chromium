// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/multi_capture/multi_capture_data_service_factory.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/media/webrtc/multi_capture/multi_capture_data_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

static_assert(BUILDFLAG(IS_CHROMEOS), "For ChromeOS only");

namespace multi_capture {

MultiCaptureDataService* MultiCaptureDataServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<MultiCaptureDataService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

MultiCaptureDataServiceFactory* MultiCaptureDataServiceFactory::GetInstance() {
  static base::NoDestructor<MultiCaptureDataServiceFactory> instance;
  return instance.get();
}

MultiCaptureDataServiceFactory::MultiCaptureDataServiceFactory()
    : web_app::IsolatedWebAppBrowserContextServiceFactory(
          "MultiCaptureDataServiceFactory") {
  DependsOn(web_app::WebAppProviderFactory::GetInstance());
}

MultiCaptureDataServiceFactory::~MultiCaptureDataServiceFactory() = default;

std::unique_ptr<KeyedService>
MultiCaptureDataServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  CHECK(profile);
  return MultiCaptureDataService::Create(
      web_app::WebAppProvider::GetForWebApps(profile), profile->GetPrefs());
}

bool MultiCaptureDataServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace multi_capture
