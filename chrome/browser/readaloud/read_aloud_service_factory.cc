// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/read_aloud_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/readaloud/read_aloud_service.h"
#include "chrome/browser/reader_mode/reader_mode_service_factory.h"
#include "ui/accessibility/accessibility_features.h"

namespace readaloud {

namespace {

// On Android the service is the native Read Aloud implementation, gated on
// kReadAloudNative. On desktop it is created for Reading Mode's experimental
// AI Playback UI, which submits feedback through it; playback itself stays
// Android-only until the utility-side controller is registered on desktop
// (b/584339399). kReadAloudNative is kept in the desktop condition so shared
// test fixtures that enable it keep working.
bool IsReadAloudServiceEnabled() {
  if (features::IsReadAloudNativeEnabled()) {
    return true;
  }
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  return features::IsReadAnythingReadAloudExperimentalPlaybackUiEnabled();
#endif
}

}  // namespace

// static
ReadAloudService* ReadAloudServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<ReadAloudService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
ReadAloudService* ReadAloudServiceFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<ReadAloudService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/false));
}

// static
ReadAloudServiceFactory* ReadAloudServiceFactory::GetInstance() {
  static base::NoDestructor<ReadAloudServiceFactory> instance;
  return instance.get();
}

ReadAloudServiceFactory::ReadAloudServiceFactory()
    : ProfileKeyedServiceFactory(
          "ReadAloudService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(reader_mode::ReaderModeServiceFactory::GetInstance());
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

ReadAloudServiceFactory::~ReadAloudServiceFactory() = default;

std::unique_ptr<KeyedService>
ReadAloudServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!IsReadAloudServiceEnabled()) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile || profile->IsOffTheRecord()) {
    return nullptr;
  }

  return std::make_unique<ReadAloudService>(profile);
}

}  // namespace readaloud
