// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_KEEPLIST_CHROMEOS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_KEEPLIST_CHROMEOS_H_

#include <string>
#include <string_view>
#include <vector>

#include "build/chromeos_buildflags.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/extension_keeplist.mojom.h"

namespace extensions {

#if BUILDFLAG(IS_CHROMEOS_ASH)
crosapi::mojom::ExtensionKeepListPtr BuildExtensionKeeplistInitParam();
crosapi::mojom::StandaloneBrowserAppServiceBlockListPtr
BuildStandaloneBrowserAppServiceBlockListInitParam();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Returns ids of the extensions that are allow to run in both Ash and Lacros.
base::span<const std::string_view> GetExtensionsRunInOSAndStandaloneBrowser();

// Returns ids of the chrome apps that are allow to run in both Ash and Lacros.
base::span<const std::string_view>
GetExtensionAppsRunInOSAndStandaloneBrowser();

// Returns ids of the extensions that are allow to run in Ash only.
base::span<const std::string_view> GetExtensionsRunInOSOnly();

// Returns ids of the chrome apps that are allow to run in Ash only.
base::span<const std::string_view> GetExtensionAppsRunInOSOnly();

// By default an extension should only be enabled in either Ash or Lacros, but
// not both. Some extensions may not work properly if enabled in both. This is
// the list of exceptions.
bool ExtensionRunsInBothOSAndStandaloneBrowser(const std::string& extension_id);

// By default most extension apps will not work properly if they run in both
// Ash and Lacros. This is the list of exceptions.
bool ExtensionAppRunsInBothOSAndStandaloneBrowser(
    const std::string& extension_id);

// Returns true if the extension is kept to run in Ash. A small list of 1st
// party extensions will continue to run in Ash either since they are used to
// support Chrome OS features such as text to speech or vox, or they are not
// compatible with Lacros yet. When this method is invoked in Lacros, it may not
// know about OS-specific extensions that are compiled into ash.
bool ExtensionRunsInOS(const std::string& extension_id);

// Some extension apps will continue to run in Ash until they are either
// deprecated or migrated. This function returns whether a given app_id is on
// that keep list. This function must only be called from the UI thread. When
// this method is invoked in Lacros, it may not know about OS-specific
// extensions that are compiled into ash.
bool ExtensionAppRunsInOS(const std::string& app_id);

// Returns true if the extension app is kept to run in Ash ONLY. A small list of
// 1st party extension apps will continue to run in Ash either since they are
// used to support Chrome OS features such as text to speech or vox, or they are
// not compatible with Lacros yet. When this method is invoked in Lacros, it may
// not know about OS-specific extension apps that are compiled into ash.
bool ExtensionAppRunsInOSOnly(std::string_view app_id);

// Returns true if the extension is kept to run in Ash ONLY. A small list of
// 1st party extensions will continue to run in Ash either since they are
// used to support Chrome OS features such as text to speech or vox, or they are
// not compatible with Lacros yet. When this method is invoked in Lacros, it may
// not know about OS-specific extensions that are compiled into ash.
bool ExtensionRunsInOSOnly(std::string_view extension_id);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool IsAppServiceBlocklistCrosapiSupported();

// Returns true if the app is on app service block list in Lacros, i.e.,
// the app can't be published in app service by Lacros.
bool ExtensionAppBlockListedForAppServiceInStandaloneBrowser(
    std::string_view app_id);

// Returns true if the extension is on app service block list in Lacros, i.e.,
// the extension can't be published in app service by Lacros.
bool ExtensionBlockListedForAppServiceInStandaloneBrowser(
    std::string_view extension_id);

// Some Lacros chrome apps related browser tests and unit tests run without Ash,
// therefore, Lacros won't get the Ash extension keeplist data from Ash via
// crosapi::mojom:::BrowserInitParams. For these tests, call
// SetEmptyAshKeeplistForTest() to allow the tests to use an empty ash keeplist
// for tests and proceed without CHECK failure due to absence of the ash
// keeplist parameter in crosapi::mojom:::BrowserInitParams.
void SetEmptyAshKeeplistForTest();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Returns true if the app is on app service block list in Ash, i.e.,
// the app can't be published in app service by Ash.
bool ExtensionAppBlockListedForAppServiceInOS(std::string_view app_id);

// Returns true if the extension is on app service block list in Ash, i.e.,
// the extension can't be published in app service by Ash.
bool ExtensionBlockListedForAppServiceInOS(std::string_view extension_id);

// Returns ids of the extensions and extension apps that are allow to run in
// both Ash and Lacros.
base::span<const std::string_view>
GetExtensionsAndAppsRunInOSAndStandaloneBrowser();

size_t ExtensionsRunInOSAndStandaloneBrowserAllowlistSizeForTest();
size_t ExtensionAppsRunInOSAndStandaloneBrowserAllowlistSizeForTest();
size_t ExtensionsRunInOSOnlyAllowlistSizeForTest();
size_t ExtensionAppsRunInOSOnlyAllowlistSizeForTest();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_KEEPLIST_CHROMEOS_H_
