// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/content_settings/content_settings_service.h"

#include "base/lazy_instance.h"
#include "base/memory/scoped_refptr.h"
#include "extensions/browser/extension_prefs_scope.h"
#include "extensions/browser/pref_names.h"

namespace extensions {

ContentSettingsService::ContentSettingsService(content::BrowserContext* context)
    : content_settings_store_(base::MakeRefCounted<ContentSettingsStore>()) {}

ContentSettingsService::~ContentSettingsService() {}

// static
ContentSettingsService* ContentSettingsService::Get(
    content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<ContentSettingsService>::Get(context);
}

// BrowserContextKeyedAPI implementation.
BrowserContextKeyedAPIFactory<ContentSettingsService>*
ContentSettingsService::GetFactoryInstance() {
  static base::LazyInstance<
      BrowserContextKeyedAPIFactory<ContentSettingsService>>::DestructorAtExit
      factory = LAZY_INSTANCE_INITIALIZER;
  return factory.Pointer();
}

void ContentSettingsService::OnExtensionRegistered(
    const std::string& extension_id,
    const base::Time& install_time,
    bool is_enabled) {
  content_settings_store_->RegisterExtension(
      extension_id, install_time, is_enabled);
}

void ContentSettingsService::OnExtensionPrefsLoaded(
    const std::string& extension_id,
    const ExtensionPrefs* prefs) {
  const base::ListValue* content_settings = NULL;
  if (prefs->ReadPrefAsList(
          extension_id, pref_names::kPrefContentSettings, &content_settings)) {
    content_settings_store_->SetExtensionContentSettingFromList(
        extension_id, content_settings, kExtensionPrefsScopeRegular);
  }
  if (prefs->ReadPrefAsList(extension_id,
                            pref_names::kPrefIncognitoContentSettings,
                            &content_settings)) {
    content_settings_store_->SetExtensionContentSettingFromList(
        extension_id,
        content_settings,
        kExtensionPrefsScopeIncognitoPersistent);
  }
}

void ContentSettingsService::OnExtensionPrefsDeleted(
    const std::string& extension_id) {
  content_settings_store_->UnregisterExtension(extension_id);
}

void ContentSettingsService::OnExtensionStateChanged(
    const std::string& extension_id,
    bool state) {
  content_settings_store_->SetExtensionState(extension_id, state);
}

void ContentSettingsService::OnExtensionPrefsWillBeDestroyed(
    ExtensionPrefs* prefs) {
  scoped_observer_.Remove(prefs);
}

void ContentSettingsService::OnExtensionPrefsAvailable(ExtensionPrefs* prefs) {
  scoped_observer_.Add(prefs);
}

}  // namespace extensions
