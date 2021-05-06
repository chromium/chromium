// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_creation/notes/internal/note_service_factory.h"

#include "base/memory/singleton.h"
#include "components/content_creation/notes/core/note_service.h"
#include "components/content_creation/notes/core/templates/template_store.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/keyed_service/core/simple_factory_key.h"

using content_creation::NoteService;
using content_creation::TemplateStore;

// static
NoteServiceFactory* NoteServiceFactory::GetInstance() {
  return base::Singleton<NoteServiceFactory>::get();
}

// static
NoteService* NoteServiceFactory::GetServiceInstance(SimpleFactoryKey* key) {
  return static_cast<NoteService*>(GetInstance()->GetServiceForKey(key, true));
}

NoteServiceFactory::NoteServiceFactory()
    : SimpleKeyedServiceFactory("NoteService",
                                SimpleDependencyManager::GetInstance()) {}

NoteServiceFactory::~NoteServiceFactory() = default;

std::unique_ptr<KeyedService> NoteServiceFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  return std::make_unique<NoteService>(std::make_unique<TemplateStore>());
}

SimpleFactoryKey* NoteServiceFactory::GetKeyToUse(SimpleFactoryKey* key) const {
  return key;
}
