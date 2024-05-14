// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/network_error.h"

#include <sstream>

#include "base/notreached.h"

namespace ash {
namespace {

const char kErrorReasonProxyAuthCancelled[] = "proxy auth cancelled";
const char kErrorReasonProxyAuthSupplied[] = "proxy auth supplied";
const char kErrorReasonProxyConnectionFailed[] = "proxy connection failed";
const char kErrorReasonProxyConfigChanged[] = "proxy config changed";
const char kErrorReasonLoadingTimeout[] = "loading timeout";
const char kErrorReasonNetworkStateChanged[] = "network state changed";
const char kErrorReasonUpdate[] = "update";
const char kErrorReasonFrameError[] = "frame error";

}  // namespace

// static
const char* NetworkError::ErrorReasonString(ErrorReason reason) {
  switch (reason) {
    case ERROR_REASON_PROXY_AUTH_CANCELLED:
      return kErrorReasonProxyAuthCancelled;
    case ERROR_REASON_PROXY_AUTH_SUPPLIED:
      return kErrorReasonProxyAuthSupplied;
    case ERROR_REASON_PROXY_CONNECTION_FAILED:
      return kErrorReasonProxyConnectionFailed;
    case ERROR_REASON_PROXY_CONFIG_CHANGED:
      return kErrorReasonProxyConfigChanged;
    case ERROR_REASON_LOADING_TIMEOUT:
      return kErrorReasonLoadingTimeout;
    case ERROR_REASON_NETWORK_STATE_CHANGED:
      return kErrorReasonNetworkStateChanged;
    case ERROR_REASON_UPDATE:
      return kErrorReasonUpdate;
    case ERROR_REASON_FRAME_ERROR:
      return kErrorReasonFrameError;
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

std::ostream& operator<<(std::ostream& stream,
                         const NetworkError::UIState& ui_state) {
  switch (ui_state) {
    case NetworkError::UI_STATE_UNKNOWN:
      stream << "Unknown";
      break;
    case NetworkError::UI_STATE_UPDATE:
      stream << "Update";
      break;
    case NetworkError::UI_STATE_SIGNIN:
      stream << "Signin";
      break;
    case NetworkError::UI_STATE_KIOSK_MODE:
      stream << "Kiosk";
      break;
    case NetworkError::UI_STATE_AUTO_ENROLLMENT_ERROR:
      stream << "Enrollment";
      break;
  }
  return stream;
}

std::ostream& operator<<(std::ostream& stream,
                         const NetworkError::ErrorState& error_state) {
  switch (error_state) {
    case NetworkError::ERROR_STATE_UNKNOWN:
      stream << "Unknown";
      break;
    case NetworkError::ERROR_STATE_PORTAL:
      stream << "Portal";
      break;
    case NetworkError::ERROR_STATE_OFFLINE:
      stream << "Offline";
      break;
    case NetworkError::ERROR_STATE_PROXY:
      stream << "Proxy";
      break;
    case NetworkError::ERROR_STATE_LOADING_TIMEOUT:
      stream << "Timeout";
      break;
    case NetworkError::ERROR_STATE_NONE:
      stream << "None";
      break;
    case NetworkError::ERROR_STATE_KIOSK_ONLINE:
      stream << "Kiosk online";
      break;
  }
  return stream;
}

}  // namespace ash
