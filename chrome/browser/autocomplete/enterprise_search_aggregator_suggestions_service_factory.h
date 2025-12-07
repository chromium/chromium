// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_ENTERPRISE_SEARCH_AGGREGATOR_SUGGESTIONS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_AUTOCOMPLETE_ENTERPRISE_SEARCH_AGGREGATOR_SUGGESTIONS_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

class EnterpriseSearchAggregatorSuggestionsService;
class Profile;
class KeyedService;

class EnterpriseSearchAggregatorSuggestionsServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static EnterpriseSearchAggregatorSuggestionsService* GetForProfile(
      Profile* profile,
      bool create_if_necessary);
  static EnterpriseSearchAggregatorSuggestionsServiceFactory* GetInstance();

  EnterpriseSearchAggregatorSuggestionsServiceFactory(
      const EnterpriseSearchAggregatorSuggestionsServiceFactory&) = delete;
  EnterpriseSearchAggregatorSuggestionsServiceFactory& operator=(
      const EnterpriseSearchAggregatorSuggestionsServiceFactory&) = delete;

 private:
  friend base::NoDestructor<
      EnterpriseSearchAggregatorSuggestionsServiceFactory>;

  EnterpriseSearchAggregatorSuggestionsServiceFactory();
  ~EnterpriseSearchAggregatorSuggestionsServiceFactory() override;

  // Overrides from BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_ENTERPRISE_SEARCH_AGGREGATOR_SUGGESTIONS_SERVICE_FACTORY_H_
