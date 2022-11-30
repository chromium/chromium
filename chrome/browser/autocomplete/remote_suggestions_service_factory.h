// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_REMOTE_SUGGESTIONS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_AUTOCOMPLETE_REMOTE_SUGGESTIONS_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class RemoteSuggestionsService;
class Profile;

class RemoteSuggestionsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static RemoteSuggestionsService* GetForProfile(Profile* profile,
                                                 bool create_if_necessary);
  static RemoteSuggestionsServiceFactory* GetInstance();

  RemoteSuggestionsServiceFactory(const RemoteSuggestionsServiceFactory&) =
      delete;
  RemoteSuggestionsServiceFactory& operator=(
      const RemoteSuggestionsServiceFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<RemoteSuggestionsServiceFactory>;

  RemoteSuggestionsServiceFactory();
  ~RemoteSuggestionsServiceFactory() override;

  // Overrides from BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_REMOTE_SUGGESTIONS_SERVICE_FACTORY_H_
