// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_EXTENSION_APPS_UTILS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_EXTENSION_APPS_UTILS_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace apps {

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Returns true if hosted apps should run in Lacros.
bool ShouldHostedAppsRunInLacros();

// Enables hosted apps in Lacros for testing.
void EnableHostedAppsInLacrosForTesting();
#endif  // IS_CHROMEOS_LACROS

// The delimiter separating the profile basename from the extension id
// in the muxed app id of standalone browser extension apps.
extern const char kExtensionAppMuxedIdDelimiter[];

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_EXTENSION_APPS_UTILS_H_
