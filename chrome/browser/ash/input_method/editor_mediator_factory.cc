// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_mediator_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/ash/input_method/editor_geolocation_provider_from_finch.h"
#include "chrome/browser/ash/input_method/editor_mediator.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/manta/manta_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/browser_context.h"

namespace ash {
namespace input_method {

EditorMediator* EditorMediatorFactory::GetForProfile(Profile* profile) {
  // The create parameter indicates to the underlying KeyedServiceFactory that
  // it is allowed to create a new EditorMediator for this context if it cannot
  // find one. It does not mean it should create a new instance on every call
  // to this method.
  return static_cast<EditorMediator*>(
      GetInstance()->GetServiceForBrowserContext(/*context=*/profile,
                                                 /*create=*/true));
}

EditorMediatorFactory* EditorMediatorFactory::GetInstance() {
  static base::NoDestructor<EditorMediatorFactory> instance;
  return instance.get();
}

EditorMediatorFactory::EditorMediatorFactory()
    : ProfileKeyedServiceFactory(
          "EditorMediatorFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(manta::MantaServiceFactory::GetInstance());
}

EditorMediatorFactory::~EditorMediatorFactory() = default;

std::unique_ptr<KeyedService> EditorMediatorFactory::BuildInstanceFor(
    content::BrowserContext* context) {
  return std::make_unique<EditorMediator>(
      Profile::FromBrowserContext(context),
      std::make_unique<EditorGeolocationProviderFromFinch>());
}

std::unique_ptr<KeyedService>
EditorMediatorFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return BuildInstanceFor(context);
}

}  // namespace input_method
}  // namespace ash
