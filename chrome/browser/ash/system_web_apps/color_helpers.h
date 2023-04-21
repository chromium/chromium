// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_COLOR_HELPERS_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_COLOR_HELPERS_H_

#include "content/public/browser/url_data_source.h"
#include "third_party/skia/include/core/SkColor.h"

class Profile;

namespace ash {
// Returns the theme color that system web apps should use, as reported by
// the operating system theme.
SkColor GetSystemThemeColor();

// Returns the background color that system web apps should use, as reported
// by the operating system theme.
SkColor GetSystemBackgroundColor();

// Returns a instance of ThemeSource from chrome/browser/ui/webui/theme_source.
// Implemented @ chrome/browser/ui/ash/system_web_apps/color_helpers_ui_impl.cc.
std::unique_ptr<content::URLDataSource> GetThemeSource(Profile* profile,
                                                       bool untrusted = false);
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_COLOR_HELPERS_H_
