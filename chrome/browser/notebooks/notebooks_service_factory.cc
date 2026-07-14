// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notebooks/notebooks_service_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "components/notebooks/internal/empty_notebooks_service.h"
#include "components/notebooks/internal/notebooks_service_impl.h"
#include "components/notebooks/public/features.h"
#include "components/notebooks/public/notebooks_service.h"

namespace notebooks {

// static
NotebooksService* NotebooksServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<NotebooksService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
NotebooksServiceFactory* NotebooksServiceFactory::GetInstance() {
  static base::NoDestructor<NotebooksServiceFactory> instance;
  return instance.get();
}

NotebooksServiceFactory::NotebooksServiceFactory()
    : ProfileKeyedServiceFactory(
          "NotebooksService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .Build()) {}

NotebooksServiceFactory::~NotebooksServiceFactory() = default;

std::unique_ptr<KeyedService>
NotebooksServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kNotebooks)) {
    return std::make_unique<EmptyNotebooksService>();
  }

  Profile* profile = Profile::FromBrowserContext(context);
  if (profile->IsOffTheRecord()) {
    return std::make_unique<EmptyNotebooksService>();
  }

  return std::make_unique<NotebooksServiceImpl>();
}

}  // namespace notebooks
