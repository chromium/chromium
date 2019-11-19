// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PREFERENCE_PREFERENCE_API_H__
#define CHROME_BROWSER_EXTENSIONS_API_PREFERENCE_PREFERENCE_API_H__

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_store.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs_scope.h"

class ExtensionPrefValueMap;
class PrefService;

namespace base {
class Value;
}

namespace extensions {
class ExtensionPrefs;

class PreferenceEventRouter : public ProfileObserver {
 public:
  explicit PreferenceEventRouter(Profile* profile);
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
  Profile* profile_;

  ScopedObserver<Profile, ProfileObserver> observed_profiles_{this};

  DISALLOW_COPY_AND_ASSIGN(PreferenceEventRouter);
};

// The class containing the implementation for extension-controlled preference
// manipulation. This implementation is separate from PreferenceAPI, since
// we need to be able to use these methods in testing, where we use
// TestExtensionPrefs and don't construct a profile.
//
// See also PreferenceAPI and TestPreferenceAPI.
class PreferenceAPIBase {
 public:
  // Functions for manipulating preference values that are controlled by the
  // extension. In other words, these are not pref values *about* the extension,
  // but rather about something global the extension wants to override.

  // Set a new extension-controlled preference value.
  void SetExtensionControlledPref(const std::string& extension_id,
                                  const std::string& pref_key,
                                  ExtensionPrefsScope scope,
                                  base::Value value);

  // Remove an extension-controlled preference value.
  void RemoveExtensionControlledPref(const std::string& extension_id,
                                     const std::string& pref_key,
                                     ExtensionPrefsScope scope);

  // Returns true if currently no extension with higher precedence controls the
  // preference.
  bool CanExtensionControlPref(const std::string& extension_id,
                               const std::string& pref_key,
                               bool incognito);

  // Returns true if extension |extension_id| currently controls the
  // preference. If |from_incognito| is not NULL, looks at incognito preferences
  // first, and |from_incognito| is set to true if the effective pref value is
  // coming from the incognito preferences, false if it is coming from the
  // normal ones.
  bool DoesExtensionControlPref(const std::string& extension_id,
                                const std::string& pref_key,
                                bool* from_incognito);

 protected:
  // Virtual for testing.
  virtual ExtensionPrefs* extension_prefs() = 0;
  virtual ExtensionPrefValueMap* extension_pref_value_map() = 0;
  virtual scoped_refptr<ContentSettingsStore> content_settings_store() = 0;
};

class PreferenceAPI : public PreferenceAPIBase,
                      public BrowserContextKeyedAPI,
                      public EventRouter::Observer,
                      public ContentSettingsStore::Observer {
 public:
  explicit PreferenceAPI(content::BrowserContext* context);
  ~PreferenceAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<PreferenceAPI>* GetFactoryInstance();

  // Convenience method to get the PreferenceAPI for a profile.
  static PreferenceAPI* Get(content::BrowserContext* context);

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<PreferenceAPI>;

  // ContentSettingsStore::Observer implementation.
  void OnContentSettingChanged(const std::string& extension_id,
                               bool incognito) override;

  // Clears incognito session-only content settings for all extensions.
  void ClearIncognitoSessionOnlyContentSettings();

  // PreferenceAPIBase implementation.
  ExtensionPrefs* extension_prefs() override;
  ExtensionPrefValueMap* extension_pref_value_map() override;
  scoped_refptr<ContentSettingsStore> content_settings_store() override;

  Profile* profile_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "PreferenceAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<PreferenceEventRouter> preference_event_router_;

  DISALLOW_COPY_AND_ASSIGN(PreferenceAPI);
};

class PrefTransformerInterface {
 public:
  virtual ~PrefTransformerInterface() {}

  // Converts the representation of a preference as seen by the extension
  // into a representation that is used in the pref stores of the browser.
  // Returns the pref store representation in case of success or sets
  // |error| and returns NULL otherwise. |bad_message| is passed to simulate
  // the behavior of EXTENSION_FUNCTION_VALIDATE. It is never NULL.
  // The ownership of the returned value is passed to the caller.
  virtual std::unique_ptr<base::Value> ExtensionToBrowserPref(
      const base::Value* extension_pref,
      std::string* error,
      bool* bad_message) = 0;

  // Converts the representation of the preference as stored in the browser
  // into a representation that is used by the extension.
  // Returns the extension representation in case of success or NULL otherwise.
  // The ownership of the returned value is passed to the caller.
  virtual std::unique_ptr<base::Value> BrowserToExtensionPref(
      const base::Value* browser_pref) = 0;
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
