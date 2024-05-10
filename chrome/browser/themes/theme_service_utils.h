// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_THEME_SERVICE_UTILS_H_
#define CHROME_BROWSER_THEMES_THEME_SERVICE_UTILS_H_

#include <optional>

#include "third_party/skia/include/core/SkColor.h"

class PrefService;

// Gets the pref service grayscale theme preference.
bool CurrentThemeIsGrayscale(const PrefService* pref_service);

// Gets the pref service user color preference.
std::optional<SkColor> CurrentThemeUserColor(const PrefService* pref_service);

#endif  // CHROME_BROWSER_THEMES_THEME_SERVICE_UTILS_H_
