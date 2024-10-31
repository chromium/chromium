// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ON_DEVICE_TRANSLATION_PREF_NAMES_H_
#define CHROME_BROWSER_ON_DEVICE_TRANSLATION_PREF_NAMES_H_

class PrefRegistrySimple;

namespace prefs {

// A pref of the fully-qualified path to the installed TranslateKit binary.
// This is populated only when the TranslateKit component is fully initialized
// and ready for use.
extern const char kTranslateKitBinaryPath[];

// A pref of the boolean value which indicates whether the TranslateKit
// component has been registered.
// This pref is set regardless of whether the component is currently ready for
// use. For example, the component might be tried to install but not yet
// initialized.
extern const char kTranslateKitPreviouslyRegistered[];

// A pref of the boolean value which indicates whether the use of the Translator
// API is allowed. This pref is set per profile by the "TranslatorAPIAllowed"
// Enterprise policy.
extern const char kTranslatorAPIAllowed[];

}  // namespace prefs

namespace on_device_translation {

// Call once by the browser process to register on-device translation
// preferences.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace on_device_translation

#endif  // CHROME_BROWSER_ON_DEVICE_TRANSLATION_PREF_NAMES_H_
