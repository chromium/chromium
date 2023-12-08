// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OMNIBOX_GEOLOCATION_HEADER_H_
#define CHROME_BROWSER_ANDROID_OMNIBOX_GEOLOCATION_HEADER_H_

#include <optional>
#include <string>

class Profile;
class GURL;

// Whether the user has given Chrome location permission.
bool HasGeolocationPermission();

// Gives the full string of the entire Geolocation header if it can be added for
// a request to |url|. Does not prompt for permission.
std::optional<std::string> GetGeolocationHeaderIfAllowed(const GURL& url,
                                                         Profile* profile);

#endif  // CHROME_BROWSER_ANDROID_OMNIBOX_GEOLOCATION_HEADER_H_
