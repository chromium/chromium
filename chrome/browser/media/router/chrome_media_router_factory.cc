// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/chrome_media_router_factory.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/media_router/browser/media_router_dialog_controller.h"
#include "content/public/browser/browser_context.h"

#if defined(OS_ANDROID)
#include "chrome/browser/media/android/router/chrome_media_router_client.h"
#include "components/media_router/browser/android/media_router_android.h"
#include "components/media_router/browser/android/media_router_dialog_controller_android.h"
#else
#include "chrome/browser/media/router/event_page_request_manager_factory.h"
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
#if defined(OS_ANDROID)
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

ChromeMediaRouterFactory::ChromeMediaRouterFactory() {
#if !defined(OS_ANDROID)
  DependsOn(EventPageRequestManagerFactory::GetInstance());
#endif
}

ChromeMediaRouterFactory::~ChromeMediaRouterFactory() = default;

content::BrowserContext* ChromeMediaRouterFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* ChromeMediaRouterFactory::BuildServiceInstanceFor(
    BrowserContext* context) const {
  MediaRouterBase* media_router = nullptr;
#if defined(OS_ANDROID)
  media_router = new MediaRouterAndroid();
#else
  media_router = new MediaRouterDesktop(context);
#endif
  media_router->Initialize();
  return media_router;
}

void ChromeMediaRouterFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  // Notify the MediaRouter (which uses the normal/base proflie) of incognito
  // profile shutdown.
  if (context->IsOffTheRecord()) {
    MediaRouter* router =
        static_cast<MediaRouter*>(GetServiceForBrowserContext(context, false));
    if (router)
      router->OnIncognitoProfileShutdown();
  }
  BrowserContextKeyedServiceFactory::BrowserContextShutdown(context);
}

}  // namespace media_router
