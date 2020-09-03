// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_factory.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

#if defined(OS_ANDROID)
#include "chrome/browser/media/android/router/chrome_media_router_client.h"
#include "chrome/browser/media/android/router/media_router_android.h"
#else
#include "chrome/browser/media/router/event_page_request_manager_factory.h"
#include "chrome/browser/media/router/mojo/media_router_desktop.h"
#endif

using content::BrowserContext;

namespace media_router {

namespace {

base::LazyInstance<MediaRouterFactory>::DestructorAtExit service_factory =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
MediaRouter* MediaRouterFactory::GetApiForBrowserContext(
    BrowserContext* context) {
  DCHECK(context);
  // GetServiceForBrowserContext returns a KeyedService hence the static_cast<>
  // to return a pointer to MediaRouter.
  return static_cast<MediaRouter*>(
      service_factory.Get().GetServiceForBrowserContext(context, true));
}

// static
MediaRouterFactory* MediaRouterFactory::GetInstance() {
  return &service_factory.Get();
}

void MediaRouterFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  if (context->IsOffTheRecord()) {
    MediaRouter* router =
        static_cast<MediaRouter*>(GetServiceForBrowserContext(context, false));
    if (router)
      router->OnIncognitoProfileShutdown();
  }
  BrowserContextKeyedServiceFactory::BrowserContextShutdown(context);
}

MediaRouterFactory::MediaRouterFactory()
    : BrowserContextKeyedServiceFactory(
          "MediaRouter",
          BrowserContextDependencyManager::GetInstance()) {
#if !defined(OS_ANDROID)
  DependsOn(EventPageRequestManagerFactory::GetInstance());
#endif
}

MediaRouterFactory::~MediaRouterFactory() {
}

content::BrowserContext* MediaRouterFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* MediaRouterFactory::BuildServiceInstanceFor(
    BrowserContext* context) const {
  MediaRouterBase* media_router = nullptr;
#if defined(OS_ANDROID)
  InitChromeMediaRouterJavaClient();
  media_router = new MediaRouterAndroid();
#else
  media_router = new MediaRouterDesktop(context);
#endif
  media_router->Initialize();
  return media_router;
}

}  // namespace media_router
