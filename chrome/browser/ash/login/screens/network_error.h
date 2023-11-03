// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_NETWORK_ERROR_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_NETWORK_ERROR_H_

#include <sstream>

namespace ash {

// TODO(jdufault): Remove Network prefix from NetworkError associated classes.
// See crbug.com/672142
class NetworkError {
 public:
  enum UIState {
    UI_STATE_UNKNOWN = 0,
    UI_STATE_UPDATE,
    UI_STATE_SIGNIN,
    UI_STATE_KIOSK_MODE,
    UI_STATE_AUTO_ENROLLMENT_ERROR,
  };

  enum ErrorState {
    ERROR_STATE_UNKNOWN = 0,
    ERROR_STATE_PORTAL,
    ERROR_STATE_OFFLINE,
    ERROR_STATE_PROXY,
    ERROR_STATE_LOADING_TIMEOUT,
    ERROR_STATE_NONE,
    // States above are being logged to histograms.
    // Please keep ERROR_STATE_NONE as the last one of the histogram values.
    // ERROR_STATE_KIOSK_ONLINE is a special case (not an actual error) and is
    // not reported in histogram.
    ERROR_STATE_KIOSK_ONLINE,
  };

  // Possible network error reasons.
  enum ErrorReason {
    ERROR_REASON_PROXY_AUTH_CANCELLED = 0,
    ERROR_REASON_PROXY_AUTH_SUPPLIED = 1,
    ERROR_REASON_PROXY_CONNECTION_FAILED = 2,
    ERROR_REASON_PROXY_CONFIG_CHANGED = 3,
    ERROR_REASON_LOADING_TIMEOUT = 4,
    // ERROR_REASON_PORTAL_DETECTED = 5,  // Deprecated.

    // Reason for a case when default network has changed.
    ERROR_REASON_NETWORK_STATE_CHANGED = 6,

    // Reason for a case when JS side requires error screen update.
    ERROR_REASON_UPDATE = 7,
    ERROR_REASON_FRAME_ERROR = 8,

    // Used as an "uninitialized" state for cases when limiting number of
    // GAIA reloads due to a single network error reason.
    ERROR_REASON_NONE = 9
  };

  static const char* ErrorReasonString(ErrorReason reason);

  friend std::ostream& operator<<(std::ostream& stream,
                                  const UIState& ui_state);
  friend std::ostream& operator<<(std::ostream& stream,
                                  const ErrorState& error_state);
};

std::ostream& operator<<(std::ostream& stream,
                         const NetworkError::UIState& ui_state);
std::ostream& operator<<(std::ostream& stream,
                         const NetworkError::ErrorState& error_state);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_NETWORK_ERROR_H_
