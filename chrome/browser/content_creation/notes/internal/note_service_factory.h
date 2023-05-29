// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_CREATION_NOTES_INTERNAL_NOTE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CONTENT_CREATION_NOTES_INTERNAL_NOTE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace content_creation {

class NoteService;

// Factory to create and retrieve a NoteService per profile.
class NoteServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static NoteServiceFactory* GetInstance();
  static content_creation::NoteService* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<NoteServiceFactory>;

  NoteServiceFactory();
  ~NoteServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace content_creation

#endif  // CHROME_BROWSER_CONTENT_CREATION_NOTES_INTERNAL_NOTE_SERVICE_FACTORY_H_
