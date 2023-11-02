// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_CREATION_REACTIONS_INTERNAL_REACTION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CONTENT_CREATION_REACTIONS_INTERNAL_REACTION_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace content_creation {

class ReactionService;

// Factory to create and retrieve a ReactionService per profile.
class ReactionServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ReactionServiceFactory* GetInstance();
  static content_creation::ReactionService* GetForProfile(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<ReactionServiceFactory>;

  ReactionServiceFactory();
  ~ReactionServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace content_creation

#endif  // CHROME_BROWSER_CONTENT_CREATION_REACTIONS_INTERNAL_REACTION_SERVICE_FACTORY_H_
