// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_SUGGESTIONS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_SUGGESTIONS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/companion/visual_search/visual_search_suggestions_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

class KeyedService;
class Profile;

namespace companion::visual_search {
class VisualSearchSuggestionsService;

// Singleton that owns VisualSearchSuggestionsService objects, one for each
// active Profile.
class VisualSearchSuggestionsServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  // Creates the service if it does not already exist for the profile.
  static VisualSearchSuggestionsService* GetForProfile(Profile* profile);

  // Get the singleton instance
  static VisualSearchSuggestionsServiceFactory* GetInstance();

  VisualSearchSuggestionsServiceFactory(
      const VisualSearchSuggestionsServiceFactory&) = delete;

  VisualSearchSuggestionsServiceFactory& operator=(
      const VisualSearchSuggestionsServiceFactory&) = delete;

 private:
  friend base::NoDestructor<VisualSearchSuggestionsServiceFactory>;

  VisualSearchSuggestionsServiceFactory();

  ~VisualSearchSuggestionsServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace companion::visual_search

#endif  // CHROME_BROWSER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_SUGGESTIONS_SERVICE_FACTORY_H_
