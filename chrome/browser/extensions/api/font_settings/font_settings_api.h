// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the classes to realize the Font Settings Extension API as specified
// in the extension API JSON.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FONT_SETTINGS_FONT_SETTINGS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_FONT_SETTINGS_FONT_SETTINGS_API_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class FontSettingsEventRouter;

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
  void FontListHasLoaded(base::Value::List list);
  ResponseValue CopyFontsToResult(const base::Value::List& fonts);
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
