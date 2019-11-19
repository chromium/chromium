// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTALLABLE_INSTALLABLE_LOGGING_H_
#define CHROME_BROWSER_INSTALLABLE_INSTALLABLE_LOGGING_H_

#include <string>

namespace content {
class WebContents;
}

// These values are a central reference for installability errors. The
// InstallableManager will specify an InstallableStatusCode (or
// NO_ERROR_DETECTED) in its result. Clients may also add their own error codes,
// and utilise LogErrorToConsole to write a message to the devtools console.
// This enum backs an UMA histogram, so it must be treated as append-only.
enum InstallableStatusCode {
  NO_ERROR_DETECTED = 0,
  RENDERER_EXITING = 1,
  RENDERER_CANCELLED = 2,
  USER_NAVIGATED = 3,
  NOT_IN_MAIN_FRAME = 4,
  NOT_FROM_SECURE_ORIGIN = 5,
  NO_MANIFEST = 6,
  MANIFEST_EMPTY = 7,
  START_URL_NOT_VALID = 8,
  MANIFEST_MISSING_NAME_OR_SHORT_NAME = 9,
  MANIFEST_DISPLAY_NOT_SUPPORTED = 10,
  MANIFEST_MISSING_SUITABLE_ICON = 11,
  NO_MATCHING_SERVICE_WORKER = 12,
  NO_ACCEPTABLE_ICON = 13,
  CANNOT_DOWNLOAD_ICON = 14,
  NO_ICON_AVAILABLE = 15,
  PLATFORM_NOT_SUPPORTED_ON_ANDROID = 16,
  NO_ID_SPECIFIED = 17,
  IDS_DO_NOT_MATCH = 18,
  ALREADY_INSTALLED = 19,
  INSUFFICIENT_ENGAGEMENT = 20,
  PACKAGE_NAME_OR_START_URL_EMPTY = 21,
  PREVIOUSLY_BLOCKED = 22,
  PREVIOUSLY_IGNORED = 23,
  SHOWING_NATIVE_APP_BANNER = 24,
  SHOWING_WEB_APP_BANNER = 25,
  FAILED_TO_CREATE_BANNER = 26,
  URL_NOT_SUPPORTED_FOR_WEBAPK = 27,
  IN_INCOGNITO = 28,
  NOT_OFFLINE_CAPABLE = 29,
  WAITING_FOR_MANIFEST = 30,
  WAITING_FOR_INSTALLABLE_CHECK = 31,
  NO_GESTURE = 32,
  WAITING_FOR_NATIVE_DATA = 33,
  SHOWING_APP_INSTALLATION_DIALOG = 34,
  NO_URL_FOR_SERVICE_WORKER = 35,
  PREFER_RELATED_APPLICATIONS = 36,
  MAX_ERROR_CODE,
};

// Returns a user-readable description for |code|, or an empty string if |code|
// should not be exposed.
std::string GetErrorMessage(InstallableStatusCode code);

// Logs a message associated with |code| to the devtools console attached to
// |web_contents|. Does nothing if |web_contents| is nullptr.
void LogErrorToConsole(content::WebContents* web_contents,
                       InstallableStatusCode code);

#endif  // CHROME_BROWSER_INSTALLABLE_INSTALLABLE_LOGGING_H_
