// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_HOTL_SODA_HOTL_INSTALLER_H_
#define CHROME_BROWSER_TTC_CORE_HOTL_SODA_HOTL_INSTALLER_H_

#include <string>

#include "chrome/services/speech/buildflags/buildflags.h"

#if !BUILDFLAG(ENABLE_SPEECH_SERVICE)
#error \
    "soda_hotl_installer.h should only be included when ENABLE_SPEECH_SERVICE is true"
#endif

class Profile;

namespace ttc::hotl {

// Resolves the language code to be used for Human-on-the-Loop (HOTL) speech
// recognition and SODA pre-caching. Uses the profile's explicit Live Caption
// language if set; otherwise falls back to the default Live Caption language
// for the application UI locale without mutating the profile preference.
std::string GetHotlRecognitionLanguage(Profile& profile);

// Pre-caches the SODA binary and the language pack for HOTL speech
// recognition if Human-on-the-Loop click confirmation is enabled and
// on-device speech recognition is supported. If the target language pack is
// already installed, refreshes its 30-day uninstallation timer.
void PrecacheSodaForHotl(Profile& profile);

}  // namespace ttc::hotl

#endif  // CHROME_BROWSER_TTC_CORE_HOTL_SODA_HOTL_INSTALLER_H_
