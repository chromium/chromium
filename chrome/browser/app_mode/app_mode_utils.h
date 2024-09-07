// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APP_MODE_APP_MODE_UTILS_H_
#define CHROME_BROWSER_APP_MODE_APP_MODE_UTILS_H_

#include <string>

#include "url/gurl.h"

class PrefService;

// Returns true if the given browser command is allowed in app mode.
bool IsCommandAllowedInAppMode(int command_id, bool is_popup);

// Returns true if the browser process is run in kiosk or forced app mode.
bool IsRunningInAppMode();

// Returns true if the browser process is run in forced app mode. Note: On
// Chrome OS devices this is functionally equivalent to IsRunningInAppMode.
bool IsRunningInForcedAppMode();

// Returns true if browser process is run in forced app mode for Chrome app
// with the provided id.
bool IsRunningInForcedAppModeForApp(const std::string& app_id);

// Returns true when the given `origin` can access browser permissions available
// to the web kiosk app.
bool IsWebKioskOriginAllowed(const PrefService* prefs, const GURL& origin);

#endif  // CHROME_BROWSER_APP_MODE_APP_MODE_UTILS_H_
