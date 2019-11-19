// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_CONTENT_SETTINGS_CONTENT_SETTINGS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_CONTENT_SETTINGS_CONTENT_SETTINGS_API_H_

#include "base/gtest_prod_util.h"
#include "chrome/browser/extensions/chrome_extension_function.h"

namespace content {
struct WebPluginInfo;
}

namespace extensions {

class ContentSettingsContentSettingClearFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentSettings.clear", CONTENTSETTINGS_CLEAR)

 protected:
  ~ContentSettingsContentSettingClearFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class ContentSettingsContentSettingGetFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentSettings.get", CONTENTSETTINGS_GET)

 protected:
  ~ContentSettingsContentSettingGetFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class ContentSettingsContentSettingSetFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentSettings.set", CONTENTSETTINGS_SET)

 protected:
  ~ContentSettingsContentSettingSetFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class ContentSettingsContentSettingGetResourceIdentifiersFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentSettings.getResourceIdentifiers",
                             CONTENTSETTINGS_GETRESOURCEIDENTIFIERS)

 protected:
  ~ContentSettingsContentSettingGetResourceIdentifiersFunction() override {}

  // ExtensionFunction:
  bool RunAsync() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtensionApiTest,
                           ContentSettingsGetResourceIdentifiers);

  // Callback method that gets executed when |plugins|
  // are asynchronously fetched.
  void OnGotPlugins(const std::vector<content::WebPluginInfo>& plugins);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_CONTENT_SETTINGS_CONTENT_SETTINGS_API_H_
