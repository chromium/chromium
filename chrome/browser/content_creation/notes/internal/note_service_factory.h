// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_CREATION_NOTES_INTERNAL_NOTE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CONTENT_CREATION_NOTES_INTERNAL_NOTE_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"

namespace content_creation {
class NoteService;
}  // namespace content_creation

class SimpleFactoryKey;

// Factory to create and retrieve a NoteService per profile.
class NoteServiceFactory : public SimpleKeyedServiceFactory {
 public:
  static NoteServiceFactory* GetInstance();

  static content_creation::NoteService* GetServiceInstance(
      SimpleFactoryKey* key);

 private:
  friend struct base::DefaultSingletonTraits<NoteServiceFactory>;

  NoteServiceFactory();
  ~NoteServiceFactory() override;

  // SimpleKeyedServiceFactory implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key) const override;
  SimpleFactoryKey* GetKeyToUse(SimpleFactoryKey* key) const override;

  DISALLOW_COPY_AND_ASSIGN(NoteServiceFactory);
};

#endif  // CHROME_BROWSER_CONTENT_CREATION_NOTES_INTERNAL_NOTE_SERVICE_FACTORY_H_
