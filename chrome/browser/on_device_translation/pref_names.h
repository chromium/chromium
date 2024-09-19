// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ON_DEVICE_TRANSLATION_PREF_NAMES_H_
#define CHROME_BROWSER_ON_DEVICE_TRANSLATION_PREF_NAMES_H_

class PrefRegistrySimple;

namespace prefs {

// The fully-qualified path to the installed TranslateKit binary.
extern const char kTranslateKitBinaryPath[];

}  // namespace prefs

namespace on_device_translation {

// Call once by the browser process to register on-device translation
// preferences.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace on_device_translation

#endif  // CHROME_BROWSER_ON_DEVICE_TRANSLATION_PREF_NAMES_H_
