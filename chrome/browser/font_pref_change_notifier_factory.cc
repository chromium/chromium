// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/font_pref_change_notifier_factory.h"

#include "chrome/browser/font_pref_change_notifier.h"
#include "chrome/browser/profiles/profile.h"

FontPrefChangeNotifierFactory::FontPrefChangeNotifierFactory()
    : ProfileKeyedServiceFactory(
          "FontPrefChangeNotifier",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

FontPrefChangeNotifierFactory::~FontPrefChangeNotifierFactory() = default;

// static
FontPrefChangeNotifier* FontPrefChangeNotifierFactory::GetForProfile(
    Profile* profile) {
  return static_cast<FontPrefChangeNotifier*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
FontPrefChangeNotifierFactory* FontPrefChangeNotifierFactory::GetInstance() {
  static base::NoDestructor<FontPrefChangeNotifierFactory> instance;
  return instance.get();
}

KeyedService* FontPrefChangeNotifierFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new FontPrefChangeNotifier(
      Profile::FromBrowserContext(context)->GetPrefs());
}
