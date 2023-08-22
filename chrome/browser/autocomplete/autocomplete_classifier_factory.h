// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_CLASSIFIER_FACTORY_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_CLASSIFIER_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class AutocompleteClassifier;
class Profile;

// Singleton that owns all AutocompleteClassifiers and associates them with
// Profiles.
class AutocompleteClassifierFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the AutocompleteClassifier for |profile|.
  static AutocompleteClassifier* GetForProfile(Profile* profile);

  static AutocompleteClassifierFactory* GetInstance();

  AutocompleteClassifierFactory(const AutocompleteClassifierFactory&) = delete;
  AutocompleteClassifierFactory& operator=(
      const AutocompleteClassifierFactory&) = delete;

  static std::unique_ptr<KeyedService> BuildInstanceFor(
      content::BrowserContext* context);

 private:
  friend base::NoDestructor<AutocompleteClassifierFactory>;

  AutocompleteClassifierFactory();
  ~AutocompleteClassifierFactory() override;

  // BrowserContextKeyedServiceFactory:
  bool ServiceIsNULLWhileTesting() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_CLASSIFIER_FACTORY_H_
