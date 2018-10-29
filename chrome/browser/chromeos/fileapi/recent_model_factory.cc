// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/recent_model_factory.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>

#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_root_map_factory.h"
#include "chrome/browser/chromeos/fileapi/recent_model.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {

// static
RecentModel* RecentModelFactory::GetForProfile(Profile* profile) {
  return static_cast<RecentModel*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

RecentModelFactory::RecentModelFactory()
    : BrowserContextKeyedServiceFactory(
          "RecentModel",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(arc::ArcDocumentsProviderRootMapFactory::GetInstance());
}

RecentModelFactory::~RecentModelFactory() = default;

// static
RecentModelFactory* RecentModelFactory::GetInstance() {
  return base::Singleton<RecentModelFactory>::get();
}

content::BrowserContext* RecentModelFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* RecentModelFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new RecentModel(profile);
}

}  // namespace chromeos
