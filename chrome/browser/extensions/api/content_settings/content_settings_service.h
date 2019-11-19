// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_CONTENT_SETTINGS_CONTENT_SETTINGS_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_API_CONTENT_SETTINGS_CONTENT_SETTINGS_SERVICE_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_store.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_observer.h"

namespace extensions {

class ContentSettingsStore;

// This service hosts a single ContentSettingsStore for the
// chrome.contentSettings API.
class ContentSettingsService : public BrowserContextKeyedAPI,
                               public ExtensionPrefsObserver,
                               public EarlyExtensionPrefsObserver {
 public:
  explicit ContentSettingsService(content::BrowserContext* context);
  ~ContentSettingsService() override;

  scoped_refptr<ContentSettingsStore> content_settings_store() const {
    return content_settings_store_;
  }

  // Convenience function to get the service for some browser context.
  static ContentSettingsService* Get(content::BrowserContext* context);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<ContentSettingsService>*
      GetFactoryInstance();

  // ExtensionPrefsObserver implementation.
  void OnExtensionRegistered(const std::string& extension_id,
                             const base::Time& install_time,
                             bool is_enabled) override;
  void OnExtensionPrefsLoaded(const std::string& extension_id,
                              const ExtensionPrefs* prefs) override;
  void OnExtensionPrefsDeleted(const std::string& extension_id) override;
  void OnExtensionStateChanged(const std::string& extension_id,
                               bool state) override;
  void OnExtensionPrefsWillBeDestroyed(ExtensionPrefs* prefs) override;

  // EarlyExtensionPrefsObserver implementation.
  void OnExtensionPrefsAvailable(ExtensionPrefs* prefs) override;

 private:
  friend class BrowserContextKeyedAPIFactory<ContentSettingsService>;

  // BrowserContextKeyedAPI implementation.
  static const bool kServiceRedirectedInIncognito = true;
  static const char* service_name() { return "ContentSettingsService"; }

  scoped_refptr<ContentSettingsStore> content_settings_store_;
  ScopedObserver<ExtensionPrefs, ExtensionPrefsObserver> scoped_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(ContentSettingsService);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_CONTENT_SETTINGS_CONTENT_SETTINGS_SERVICE_H_
