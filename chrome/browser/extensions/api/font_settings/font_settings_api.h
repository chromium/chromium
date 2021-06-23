// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the classes to realize the Font Settings Extension API as specified
// in the extension API JSON.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FONT_SETTINGS_FONT_SETTINGS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_FONT_SETTINGS_FONT_SETTINGS_API_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/font_pref_change_notifier.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_function.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace extensions {

// This class observes pref changed events on a profile and dispatches the
// corresponding extension API events to extensions.
class FontSettingsEventRouter {
 public:
  // Constructor for observing pref changed events on |profile|. Stores a
  // pointer to |profile| but does not take ownership. |profile| must be
  // non-NULL and remain alive for the lifetime of the instance.
  explicit FontSettingsEventRouter(Profile* profile);
  virtual ~FontSettingsEventRouter();

 private:
  // Observes browser pref |pref_name|. When a change is observed, dispatches
  // event |event_name| to extensions. A JavaScript object is passed to the
  // extension event function with the new value of the pref in property |key|.
  void AddPrefToObserve(const char* pref_name,
                        events::HistogramValue histogram_value,
                        const char* event_name,
                        const char* key);

  // Decodes a preference change for a font family map and invokes
  // OnFontNamePrefChange with the right parameters.
  void OnFontFamilyMapPrefChanged(const std::string& pref_name);

  // Dispatches a changed event for the font setting for |generic_family| and
  // |script| to extensions. The new value of the setting is the value of
  // browser pref |pref_name|.
  void OnFontNamePrefChanged(const std::string& pref_name,
                             const std::string& generic_family,
                             const std::string& script);

  // Dispatches the setting changed event |event_name| to extensions. The new
  // value of the setting is the value of browser pref |pref_name|. This value
  // is passed in the JavaScript object argument to the extension event function
  // under the key |key|.
  void OnFontPrefChanged(events::HistogramValue histogram_value,
                         const std::string& event_name,
                         const std::string& key,
                         const std::string& pref_name);

  // Manages pref observation registration.
  PrefChangeRegistrar registrar_;
  FontPrefChangeNotifier::Registrar font_change_registrar_;

  // Weak, owns us (transitively via ExtensionService).
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(FontSettingsEventRouter);
};

// The profile-keyed service that manages the font_settings extension API.
// This is not an EventRouter::Observer (and does not lazily initialize) because
// doing so caused a regression in perf tests. See crbug.com/163466.
class FontSettingsAPI : public BrowserContextKeyedAPI {
 public:
  explicit FontSettingsAPI(content::BrowserContext* context);
  ~FontSettingsAPI() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<FontSettingsAPI>* GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<FontSettingsAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "FontSettingsAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  std::unique_ptr<FontSettingsEventRouter> font_settings_event_router_;
};

// fontSettings.clearFont API function.
class FontSettingsClearFontFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fontSettings.clearFont", FONTSETTINGS_CLEARFONT)

 protected:
  // RefCounted types have non-public destructors, as with all extension
  // functions in this file.
  ~FontSettingsClearFontFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

// fontSettings.getFont API function.
class FontSettingsGetFontFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fontSettings.getFont", FONTSETTINGS_GETFONT)

 protected:
  ~FontSettingsGetFontFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

// fontSettings.setFont API function.
class FontSettingsSetFontFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fontSettings.setFont", FONTSETTINGS_SETFONT)

 protected:
  ~FontSettingsSetFontFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

// fontSettings.getFontList API function.
class FontSettingsGetFontListFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fontSettings.getFontList",
                             FONTSETTINGS_GETFONTLIST)

 protected:
  ~FontSettingsGetFontListFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void FontListHasLoaded(std::unique_ptr<base::ListValue> list);
  ResponseValue CopyFontsToResult(base::ListValue* fonts);
};

// Base class for extension API functions that clear a browser font pref.
class ClearFontPrefExtensionFunction : public ExtensionFunction {
 protected:
  ~ClearFontPrefExtensionFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;

  // Implementations should return the name of the preference to clear, like
  // "webkit.webprefs.default_font_size".
  virtual const char* GetPrefName() = 0;
};

// Base class for extension API functions that get a browser font pref.
class GetFontPrefExtensionFunction : public ExtensionFunction {
 protected:
  ~GetFontPrefExtensionFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;

  // Implementations should return the name of the preference to get, like
  // "webkit.webprefs.default_font_size".
  virtual const char* GetPrefName() = 0;

  // Implementations should return the key for the value in the extension API,
  // like "pixelSize".
  virtual const char* GetKey() = 0;
};

// Base class for extension API functions that set a browser font pref.
class SetFontPrefExtensionFunction : public ExtensionFunction {
 protected:
  ~SetFontPrefExtensionFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;

  // Implementations should return the name of the preference to set, like
  // "webkit.webprefs.default_font_size".
  virtual const char* GetPrefName() = 0;

  // Implementations should return the key for the value in the extension API,
  // like "pixelSize".
  virtual const char* GetKey() = 0;
};

// The following are get/set/clear API functions that act on a browser font
// pref.

class FontSettingsClearDefaultFontSizeFunction
    : public ClearFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fontSettings.clearDefaultFontSize",
                             FONTSETTINGS_CLEARDEFAULTFONTSIZE)

 protected:
  ~FontSettingsClearDefaultFontSizeFunction() override {}

  // ClearFontPrefExtensionFunction:
  const char* GetPrefName() override;
};

class FontSettingsGetDefaultFontSizeFunction
    : public GetFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fontSettings.getDefaultFontSize",
                             FONTSETTINGS_GETDEFAULTFONTSIZE)

 protected:
  ~FontSettingsGetDefaultFontSizeFunction() override {}

  // GetFontPrefExtensionFunction:
  const char* GetPrefName() override;
  const char* GetKey() override;
};

class FontSettingsSetDefaultFontSizeFunction
    : public SetFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fontSettings.setDefaultFontSize",
                             FONTSETTINGS_SETDEFAULTFONTSIZE)

 protected:
  ~FontSettingsSetDefaultFontSizeFunction() override {}

  // SetFontPrefExtensionFunction:
  const char* GetPrefName() override;
  const char* GetKey() override;
};

class FontSettingsClearDefaultFixedFontSizeFunction
    : public ClearFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fontSettings.clearDefaultFixedFontSize",
                             FONTSETTINGS_CLEARDEFAULTFIXEDFONTSIZE)

 protected:
  ~FontSettingsClearDefaultFixedFontSizeFunction() override {}

  // ClearFontPrefExtensionFunction:
  const char* GetPrefName() override;
};

class FontSettingsGetDefaultFixedFontSizeFunction
    : public GetFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fontSettings.getDefaultFixedFontSize",
                             FONTSETTINGS_GETDEFAULTFIXEDFONTSIZE)

 protected:
  ~FontSettingsGetDefaultFixedFontSizeFunction() override {}

  // GetFontPrefExtensionFunction:
  const char* GetPrefName() override;
  const char* GetKey() override;
};

class FontSettingsSetDefaultFixedFontSizeFunction
    : public SetFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fontSettings.setDefaultFixedFontSize",
                             FONTSETTINGS_SETDEFAULTFIXEDFONTSIZE)

 protected:
  ~FontSettingsSetDefaultFixedFontSizeFunction() override {}

  // SetFontPrefExtensionFunction:
  const char* GetPrefName() override;
  const char* GetKey() override;
};

class FontSettingsClearMinimumFontSizeFunction
    : public ClearFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fontSettings.clearMinimumFontSize",
                             FONTSETTINGS_CLEARMINIMUMFONTSIZE)

 protected:
  ~FontSettingsClearMinimumFontSizeFunction() override {}

  // ClearFontPrefExtensionFunction:
  const char* GetPrefName() override;
};

class FontSettingsGetMinimumFontSizeFunction
    : public GetFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fontSettings.getMinimumFontSize",
                             FONTSETTINGS_GETMINIMUMFONTSIZE)

 protected:
  ~FontSettingsGetMinimumFontSizeFunction() override {}

  // GetFontPrefExtensionFunction:
  const char* GetPrefName() override;
  const char* GetKey() override;
};

class FontSettingsSetMinimumFontSizeFunction
    : public SetFontPrefExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fontSettings.setMinimumFontSize",
                             FONTSETTINGS_SETMINIMUMFONTSIZE)

 protected:
  ~FontSettingsSetMinimumFontSizeFunction() override {}

  // SetFontPrefExtensionFunction:
  const char* GetPrefName() override;
  const char* GetKey() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FONT_SETTINGS_FONT_SETTINGS_API_H_
