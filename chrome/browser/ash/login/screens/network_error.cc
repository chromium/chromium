// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/network_error.h"

#include "base/notreached.h"

namespace chromeos {

namespace {

const char kErrorReasonProxyAuthCancelled[] = "proxy auth cancelled";
const char kErrorReasonProxyAuthSupplied[] = "proxy auth supplied";
const char kErrorReasonProxyConnectionFailed[] = "proxy connection failed";
const char kErrorReasonProxyConfigChanged[] = "proxy config changed";
const char kErrorReasonLoadingTimeout[] = "loading timeout";
const char kErrorReasonPortalDetected[] = "portal detected";
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
    case ERROR_REASON_PORTAL_DETECTED:
      return kErrorReasonPortalDetected;
    case ERROR_REASON_NETWORK_STATE_CHANGED:
      return kErrorReasonNetworkStateChanged;
    case ERROR_REASON_UPDATE:
      return kErrorReasonUpdate;
    case ERROR_REASON_FRAME_ERROR:
      return kErrorReasonFrameError;
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace chromeos
