// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_keyed_service_factory.h"

#include "base/logging.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace media_history {

// static
MediaHistoryKeyedService* MediaHistoryKeyedServiceFactory::GetForProfile(
    Profile* profile) {
  if (profile->IsOffTheRecord())
    return nullptr;

  return static_cast<MediaHistoryKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
MediaHistoryKeyedServiceFactory*
MediaHistoryKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<MediaHistoryKeyedServiceFactory> factory;
  return factory.get();
}

MediaHistoryKeyedServiceFactory::MediaHistoryKeyedServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "MediaHistoryKeyedService",
          BrowserContextDependencyManager::GetInstance()) {}

MediaHistoryKeyedServiceFactory::~MediaHistoryKeyedServiceFactory() = default;

bool MediaHistoryKeyedServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

KeyedService* MediaHistoryKeyedServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK(!context->IsOffTheRecord());

  return new MediaHistoryKeyedService(context);
}

}  // namespace media_history
