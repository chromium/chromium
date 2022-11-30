// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_DICTATION_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_DICTATION_H_

#include <string>

#include "base/containers/flat_map.h"

class Profile;

namespace ash {

// Provides global dictation (type what you speak) on Chrome OS.
class Dictation {
 public:
  // Stores whether locales are supported by offline speech recognition and
  // if the corresponding language pack is installed.
  struct LocaleData {
    bool works_offline = false;
    bool installed = false;
  };

  // Gets the default locale given a user |profile|. If this is a |new_user|,
  // uses the application language. Otherwise uses previous method of
  // determining Dictation language with default IME language.
  // This is guaranteed to return a supported BCP-47 locale.
  static std::string DetermineDefaultSupportedLocale(Profile* profile,
                                                     bool new_user);

  // Gets all possible BCP-47 style locale codes supported by Dictation,
  // and whether they are available offline.
  static const base::flat_map<std::string, LocaleData> GetAllSupportedLocales();

  Dictation() = default;
  ~Dictation() = default;
  Dictation(const Dictation&) = delete;
  Dictation& operator=(const Dictation&) = delete;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_DICTATION_H_
