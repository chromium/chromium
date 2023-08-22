// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_DOCUMENT_SUGGESTIONS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_AUTOCOMPLETE_DOCUMENT_SUGGESTIONS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class DocumentSuggestionsService;
class Profile;

class DocumentSuggestionsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static DocumentSuggestionsService* GetForProfile(Profile* profile,
                                                   bool create_if_necessary);
  static DocumentSuggestionsServiceFactory* GetInstance();

  DocumentSuggestionsServiceFactory(const DocumentSuggestionsServiceFactory&) =
      delete;
  DocumentSuggestionsServiceFactory& operator=(
      const DocumentSuggestionsServiceFactory&) = delete;

 private:
  friend base::NoDestructor<DocumentSuggestionsServiceFactory>;

  DocumentSuggestionsServiceFactory();
  ~DocumentSuggestionsServiceFactory() override;

  // Overrides from BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_DOCUMENT_SUGGESTIONS_SERVICE_FACTORY_H_
