// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/chrome_media_router_factory.h"

#include "build/build_config.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/media_router/browser/media_router_dialog_controller.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/media/android/router/chrome_media_router_client.h"
#include "components/media_router/browser/android/media_router_android.h"
#include "components/media_router/browser/android/media_router_dialog_controller_android.h"
#else
#include "chrome/browser/media/router/mojo/media_router_desktop.h"
#endif

using content::BrowserContext;

namespace media_router {

namespace {

base::LazyInstance<ChromeMediaRouterFactory>::DestructorAtExit service_factory =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
ChromeMediaRouterFactory* ChromeMediaRouterFactory::GetInstance() {
  return &service_factory.Get();
}

// static
void ChromeMediaRouterFactory::DoPlatformInit() {
#if BUILDFLAG(IS_ANDROID)
  InitChromeMediaRouterJavaClient();

  // The desktop (Views) version of this is in ChromeBrowserMainExtraPartsViews
  // because we can't reach into Views from this directory.
  media_router::MediaRouterDialogController::SetGetOrCreate(
      base::BindRepeating([](content::WebContents* web_contents) {
        DCHECK(web_contents);
        MediaRouterDialogController* controller = nullptr;
        // This call does nothing if the controller already exists.
        MediaRouterDialogControllerAndroid::CreateForWebContents(web_contents);
        controller =
            MediaRouterDialogControllerAndroid::FromWebContents(web_contents);
        return controller;
      }));
#endif
}

ChromeMediaRouterFactory::ChromeMediaRouterFactory() = default;

ChromeMediaRouterFactory::~ChromeMediaRouterFactory() = default;

content::BrowserContext* ChromeMediaRouterFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  ProfileSelections profile_selections =
      ProfileSelections::Builder()
          .WithRegular(ProfileSelection::kOwnInstance)
          .WithGuest(ProfileSelection::kOwnInstance)
          .WithSystem(ProfileSelection::kNone)
          // TODO(crbug.com/41488885): Check if this service is needed for
          // Ash Internals.
          .WithAshInternals(ProfileSelection::kOwnInstance)
          .Build();
  return profile_selections.ApplyProfileSelection(
      Profile::FromBrowserContext(context));
}

std::unique_ptr<KeyedService>
ChromeMediaRouterFactory::BuildServiceInstanceForBrowserContext(
    BrowserContext* context) const {
  CHECK(MediaRouterEnabled(context));
  std::unique_ptr<MediaRouterBase> media_router = nullptr;
#if BUILDFLAG(IS_ANDROID)
  media_router = std::make_unique<MediaRouterAndroid>();
#else
  media_router = std::make_unique<MediaRouterDesktop>(context);
#endif  // BUILDFLAG(IS_ANDROID)
  media_router->Initialize();
  return media_router;
}

}  // namespace media_router
