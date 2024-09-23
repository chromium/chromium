// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_PHRASE_SEGMENTATION_DEPENDENCY_PARSER_MODEL_LOADER_FACTORY_H_
#define CHROME_BROWSER_ACCESSIBILITY_PHRASE_SEGMENTATION_DEPENDENCY_PARSER_MODEL_LOADER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

class DependencyParserModelLoader;
class Profile;

// LazyInstance that owns all DependencyParserModelLoader(s) and
// associates them with Profiles.
class DependencyParserModelLoaderFactory : public ProfileKeyedServiceFactory {
 public:
  // Gets the DependencyParserModelLoader for the profile.
  //
  // Returns null if the features that allow for this to provide useful
  // information are disabled. Importantly, only available when the
  // optimization guide service is.
  static DependencyParserModelLoader* GetForProfile(Profile* profile);

  // Gets the LazyInstance that owns all DependencyParserModelLoader(s).
  static DependencyParserModelLoaderFactory* GetInstance();

 private:
  friend base::NoDestructor<DependencyParserModelLoaderFactory>;

  DependencyParserModelLoaderFactory();
  ~DependencyParserModelLoaderFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_PHRASE_SEGMENTATION_DEPENDENCY_PARSER_MODEL_LOADER_FACTORY_H_
