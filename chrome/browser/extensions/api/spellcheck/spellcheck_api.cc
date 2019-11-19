// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/spellcheck/spellcheck_api.h"

#include "base/lazy_instance.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/common/extensions/api/spellcheck/spellcheck_handler.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace {

SpellcheckDictionaryInfo* GetSpellcheckDictionaryInfo(
    const Extension* extension) {
  SpellcheckDictionaryInfo *spellcheck_info =
      static_cast<SpellcheckDictionaryInfo*>(
          extension->GetManifestData(manifest_keys::kSpellcheck));
  return spellcheck_info;
}

SpellcheckService::DictionaryFormat GetDictionaryFormat(
    const std::string& format) {
  if (format == "hunspell") {
    return SpellcheckService::DICT_HUNSPELL;
  } else if (format == "text") {
    return SpellcheckService::DICT_TEXT;
  } else {
    return SpellcheckService::DICT_UNKNOWN;
  }
}

}  // namespace

SpellcheckAPI::SpellcheckAPI(content::BrowserContext* context) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(context));
}

SpellcheckAPI::~SpellcheckAPI() = default;

static base::LazyInstance<BrowserContextKeyedAPIFactory<SpellcheckAPI>>::
    DestructorAtExit g_spellcheck_api_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<SpellcheckAPI>*
SpellcheckAPI::GetFactoryInstance() {
  return g_spellcheck_api_factory.Pointer();
}

void SpellcheckAPI::OnExtensionLoaded(content::BrowserContext* browser_context,
                                      const Extension* extension) {
  SpellcheckDictionaryInfo* spellcheck_info =
      GetSpellcheckDictionaryInfo(extension);
  if (spellcheck_info) {
    // TODO(rlp): Handle load failure. =
    SpellcheckService* spellcheck =
        SpellcheckServiceFactory::GetForContext(browser_context);
    spellcheck->LoadExternalDictionary(
        spellcheck_info->language,
        spellcheck_info->locale,
        spellcheck_info->path,
        GetDictionaryFormat(spellcheck_info->format));
  }
}
void SpellcheckAPI::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  SpellcheckDictionaryInfo* spellcheck_info =
      GetSpellcheckDictionaryInfo(extension);
  if (spellcheck_info) {
    // TODO(rlp): Handle unload failure.
    SpellcheckService* spellcheck =
        SpellcheckServiceFactory::GetForContext(browser_context);
    spellcheck->UnloadExternalDictionary(spellcheck_info->path);
  }
}

template <>
void
BrowserContextKeyedAPIFactory<SpellcheckAPI>::DeclareFactoryDependencies() {
  DependsOn(SpellcheckServiceFactory::GetInstance());
}

}  // namespace extensions
