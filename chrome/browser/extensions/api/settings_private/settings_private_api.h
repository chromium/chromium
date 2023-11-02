// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_SETTINGS_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_SETTINGS_PRIVATE_API_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

// Implements the chrome.settingsPrivate.setPref method.
class SettingsPrivateSetPrefFunction : public ExtensionFunction {
 public:
  SettingsPrivateSetPrefFunction() {}

  SettingsPrivateSetPrefFunction(const SettingsPrivateSetPrefFunction&) =
      delete;
  SettingsPrivateSetPrefFunction& operator=(
      const SettingsPrivateSetPrefFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("settingsPrivate.setPref", SETTINGSPRIVATE_SETPREF)

 protected:
  ~SettingsPrivateSetPrefFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the chrome.settingsPrivate.getAllPrefs method.
class SettingsPrivateGetAllPrefsFunction : public ExtensionFunction {
 public:
  SettingsPrivateGetAllPrefsFunction() {}

  SettingsPrivateGetAllPrefsFunction(
      const SettingsPrivateGetAllPrefsFunction&) = delete;
  SettingsPrivateGetAllPrefsFunction& operator=(
      const SettingsPrivateGetAllPrefsFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("settingsPrivate.getAllPrefs",
                             SETTINGSPRIVATE_GETALLPREFS)

 protected:
  ~SettingsPrivateGetAllPrefsFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the chrome.settingsPrivate.getPref method.
class SettingsPrivateGetPrefFunction : public ExtensionFunction {
 public:
  SettingsPrivateGetPrefFunction() {}

  SettingsPrivateGetPrefFunction(const SettingsPrivateGetPrefFunction&) =
      delete;
  SettingsPrivateGetPrefFunction& operator=(
      const SettingsPrivateGetPrefFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("settingsPrivate.getPref", SETTINGSPRIVATE_GETPREF)

 protected:
  ~SettingsPrivateGetPrefFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the chrome.settingsPrivate.getDefaultZoom method.
class SettingsPrivateGetDefaultZoomFunction : public ExtensionFunction {
 public:
  SettingsPrivateGetDefaultZoomFunction() {}

  SettingsPrivateGetDefaultZoomFunction(
      const SettingsPrivateGetDefaultZoomFunction&) = delete;
  SettingsPrivateGetDefaultZoomFunction& operator=(
      const SettingsPrivateGetDefaultZoomFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("settingsPrivate.getDefaultZoom",
                             SETTINGSPRIVATE_GETDEFAULTZOOMFUNCTION)

 protected:
  ~SettingsPrivateGetDefaultZoomFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the chrome.settingsPrivate.setDefaultZoom method.
class SettingsPrivateSetDefaultZoomFunction : public ExtensionFunction {
 public:
  SettingsPrivateSetDefaultZoomFunction() {}

  SettingsPrivateSetDefaultZoomFunction(
      const SettingsPrivateSetDefaultZoomFunction&) = delete;
  SettingsPrivateSetDefaultZoomFunction& operator=(
      const SettingsPrivateSetDefaultZoomFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("settingsPrivate.setDefaultZoom",
                             SETTINGSPRIVATE_SETDEFAULTZOOMFUNCTION)

 protected:
  ~SettingsPrivateSetDefaultZoomFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_SETTINGS_PRIVATE_API_H_
