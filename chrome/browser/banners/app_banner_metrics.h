// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BANNERS_APP_BANNER_METRICS_H_
#define CHROME_BROWSER_BANNERS_APP_BANNER_METRICS_H_

#include "chrome/browser/installable/installable_logging.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"

namespace banners {

// This enum backs a UMA histogram, so it should be treated as append-only.
enum DisplayEvent {
  DISPLAY_EVENT_MIN = 0,
  DISPLAY_EVENT_BANNER_REQUESTED = 1,
  DISPLAY_EVENT_BLOCKED_PREVIOUSLY = 2,
  DISPLAY_EVENT_PROMOTED_TOO_MANY_OTHERS = 3,
  DISPLAY_EVENT_CREATED = 4,
  DISPLAY_EVENT_INSTALLED_PREVIOUSLY = 5,
  DISPLAY_EVENT_IGNORED_PREVIOUSLY = 6,
  DISPLAY_EVENT_LACKS_SERVICE_WORKER = 7,
  DISPLAY_EVENT_NOT_VISITED_ENOUGH = 8,
  DISPLAY_EVENT_NATIVE_APP_BANNER_REQUESTED = 9,
  DISPLAY_EVENT_WEB_APP_BANNER_REQUESTED = 10,
  DISPLAY_EVENT_NATIVE_APP_BANNER_CREATED = 11,
  DISPLAY_EVENT_WEB_APP_BANNER_CREATED = 12,
  DISPLAY_EVENT_MAX = 13,
};

// This enum backs a UMA histogram, so it should be treated as append-only.
enum InstallEvent {
  INSTALL_EVENT_MIN = 20,
  INSTALL_EVENT_NATIVE_APP_INSTALL_TRIGGERED = 21,
  // Deprecated: INSTALL_EVENT_NATIVE_APP_INSTALL_STARTED = 22,
  // Deprecated: INSTALL_EVENT_NATIVE_APP_INSTALL_COMPLETED = 23,
  INSTALL_EVENT_WEB_APP_INSTALLED = 24,
  INSTALL_EVENT_MAX = 25,
};

// This enum backs a UMA histogram, so it should be treated as append-only.
enum DismissEvent {
  DISMISS_EVENT_MIN = 40,
  DISMISS_EVENT_ERROR = 41,
  DISMISS_EVENT_APP_OPEN = 42,
  DISMISS_EVENT_BANNER_CLICK = 43,
  DISMISS_EVENT_BANNER_SWIPE = 44,
  DISMISS_EVENT_CLOSE_BUTTON = 45,
  // Deprecated: DISMISS_EVENT_INSTALL_TIMEOUT = 46,
  DISMISS_EVENT_DISMISSED = 47,
  DISMISS_EVENT_AMBIENT_INFOBAR_DISMISSED = 48,
  DISMISS_EVENT_MAX = 49,
};

// This enum backs a UMA histogram, so it should be treated as append-only.
enum UserResponse {
  USER_RESPONSE_MIN = 0,
  USER_RESPONSE_NATIVE_APP_ACCEPTED = 1,
  USER_RESPONSE_WEB_APP_ACCEPTED = 2,
  USER_RESPONSE_NATIVE_APP_DISMISSED = 3,
  USER_RESPONSE_WEB_APP_DISMISSED = 4,
  // Deprecated: USER_RESPONSE_NATIVE_APP_IGNORED = 5,
  // Deprecated: USER_RESPONSE_WEB_APP_IGNORED = 6,
  USER_RESPONSE_MAX = 7,
};

// This enum backs a UMA histogram, so it should be treated as append-only.
enum BeforeInstallEvent {
  BEFORE_INSTALL_EVENT_MIN = 0,
  BEFORE_INSTALL_EVENT_CREATED = 1,
  BEFORE_INSTALL_EVENT_COMPLETE = 2,
  BEFORE_INSTALL_EVENT_NO_ACTION = 3,
  BEFORE_INSTALL_EVENT_PREVENT_DEFAULT_CALLED = 4,
  BEFORE_INSTALL_EVENT_PROMPT_CALLED_AFTER_PREVENT_DEFAULT = 5,
  BEFORE_INSTALL_EVENT_PROMPT_NOT_CALLED_AFTER_PREVENT_DEFAULT = 6,
  BEFORE_INSTALL_EVENT_MAX = 7,
};

extern const char kDismissEventHistogram[];
extern const char kDisplayEventHistogram[];
extern const char kInstallEventHistogram[];
extern const char kMinutesHistogram[];
extern const char kUserResponseHistogram[];
extern const char kBeforeInstallEventHistogram[];
extern const char kInstallableStatusCodeHistogram[];
extern const char kInstallDisplayModeHistogram[];

void TrackDismissEvent(int event);
void TrackDisplayEvent(int event);
void TrackInstallEvent(int event);
void TrackMinutesFromFirstVisitToBannerShown(int minutes);
void TrackUserResponse(int event);
void TrackBeforeInstallEvent(int event);
void TrackInstallableStatusCode(InstallableStatusCode code);
void TrackInstallDisplayMode(blink::mojom::DisplayMode display);

}  // namespace banners

#endif  // CHROME_BROWSER_BANNERS_APP_BANNER_METRICS_H_
