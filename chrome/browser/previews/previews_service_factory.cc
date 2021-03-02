// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_service_factory.h"

#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace {

base::LazyInstance<PreviewsServiceFactory>::DestructorAtExit
    g_previews_service_factory = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
PreviewsService* PreviewsServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<PreviewsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PreviewsServiceFactory* PreviewsServiceFactory::GetInstance() {
  return g_previews_service_factory.Pointer();
}

PreviewsServiceFactory::PreviewsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PreviewsService",
          BrowserContextDependencyManager::GetInstance()) {}

PreviewsServiceFactory::~PreviewsServiceFactory() {}

KeyedService* PreviewsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new PreviewsService(context);
}
