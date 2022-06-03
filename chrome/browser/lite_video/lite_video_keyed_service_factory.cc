// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lite_video/lite_video_keyed_service_factory.h"

#include "chrome/browser/lite_video/lite_video_features.h"
#include "chrome/browser/lite_video/lite_video_keyed_service.h"
#include "chrome/browser/lite_video/lite_video_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

// static
LiteVideoKeyedService* LiteVideoKeyedServiceFactory::GetForProfile(
    Profile* profile) {
  if (IsLiteVideoAllowedForUser(profile)) {
    return static_cast<LiteVideoKeyedService*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }
  return nullptr;
}

// static
LiteVideoKeyedServiceFactory* LiteVideoKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<LiteVideoKeyedServiceFactory> factory;
  return factory.get();
}

LiteVideoKeyedServiceFactory::LiteVideoKeyedServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "LiteVideoKeyedService",
          BrowserContextDependencyManager::GetInstance()) {}

LiteVideoKeyedServiceFactory::~LiteVideoKeyedServiceFactory() = default;

KeyedService* LiteVideoKeyedServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new LiteVideoKeyedService(context);
}
