// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PREFERENCE_PREFERENCE_API_H__
#define CHROME_BROWSER_EXTENSIONS_API_PREFERENCE_PREFERENCE_API_H__

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/api/content_settings/content_settings_store.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/api/types.h"

class PrefService;

namespace base {
class Value;
}

namespace extensions {

class PreferenceEventRouter : public ProfileObserver {
 public:
  explicit PreferenceEventRouter(Profile* profile);

  PreferenceEventRouter(const PreferenceEventRouter&) = delete;
  PreferenceEventRouter& operator=(const PreferenceEventRouter&) = delete;

  ~PreferenceEventRouter() override;

 private:
  void OnPrefChanged(PrefService* pref_service,
                     const std::string& pref_key);

  // ProfileObserver:
  void OnOffTheRecordProfileCreated(Profile* off_the_record) override;
  void OnProfileWillBeDestroyed(Profile* profile) override;

  void ObserveOffTheRecordPrefs(PrefService* prefs);

  PrefChangeRegistrar registrar_;
  std::unique_ptr<PrefChangeRegistrar> incognito_registrar_;

  // Weak, owns us (transitively via ExtensionService).
  raw_ptr<Profile> profile_;

  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      observed_profiles_{this};
};

class PreferenceAPI : public BrowserContextKeyedAPI,
                      public EventRouter::Observer,
                      public ContentSettingsStore::Observer {
 public:
  using ChromeSettingScope = extensions::api::types::ChromeSettingScope;
  explicit PreferenceAPI(content::BrowserContext* context);

  PreferenceAPI(const PreferenceAPI&) = delete;
  PreferenceAPI& operator=(const PreferenceAPI&) = delete;

  ~PreferenceAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<PreferenceAPI>* GetFactoryInstance();

  // Convenience method to get the PreferenceAPI for a profile.
  static PreferenceAPI* Get(content::BrowserContext* context);

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

  // Ensures that a PreferenceEventRouter is created only once.
  void EnsurePreferenceEventRouterCreated();

 private:
  friend class BrowserContextKeyedAPIFactory<PreferenceAPI>;

  // ContentSettingsStore::Observer implementation.
  void OnContentSettingChanged(const ExtensionId& extension_id,
                               bool incognito) override;

  // Clears incognito session-only content settings for all extensions.
  void ClearIncognitoSessionOnlyContentSettings();

  scoped_refptr<ContentSettingsStore> content_settings_store();

  raw_ptr<Profile> profile_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "PreferenceAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<PreferenceEventRouter> preference_event_router_;
};

// A base class to provide functionality common to the other *PreferenceFunction
// classes.
class PreferenceFunction : public ExtensionFunction {
 protected:
  enum PermissionType { PERMISSION_TYPE_READ, PERMISSION_TYPE_WRITE };

  ~PreferenceFunction() override;
};

class GetPreferenceFunction : public PreferenceFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("types.ChromeSetting.get", TYPES_CHROMESETTING_GET)

 protected:
  ~GetPreferenceFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void ProduceGetResult(base::Value::Dict* result,
                        const base::Value* pref_value,
                        const std::string& level_of_control,
                        const std::string& browser_pref,
                        bool incognito);
};

class SetPreferenceFunction : public PreferenceFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("types.ChromeSetting.set", TYPES_CHROMESETTING_SET)

 protected:
  ~SetPreferenceFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class ClearPreferenceFunction : public PreferenceFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("types.ChromeSetting.clear",
                             TYPES_CHROMESETTING_CLEAR)

 protected:
  ~ClearPreferenceFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PREFERENCE_PREFERENCE_API_H__
