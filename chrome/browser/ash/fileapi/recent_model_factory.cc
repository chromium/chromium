// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/recent_model_factory.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>

#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root_map_factory.h"
#include "chrome/browser/ash/fileapi/recent_model.h"
#include "chrome/browser/profiles/profile.h"

namespace ash {

// static
RecentModel* RecentModelFactory::GetForProfile(Profile* profile) {
  return static_cast<RecentModel*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

RecentModelFactory::RecentModelFactory()
    : ProfileKeyedServiceFactory(
          "RecentModel",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(arc::ArcDocumentsProviderRootMapFactory::GetInstance());
}

RecentModelFactory::~RecentModelFactory() = default;

// static
RecentModelFactory* RecentModelFactory::GetInstance() {
  static base::NoDestructor<RecentModelFactory> instance;
  return instance.get();
}

KeyedService* RecentModelFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new RecentModel(profile);
}

}  // namespace ash
