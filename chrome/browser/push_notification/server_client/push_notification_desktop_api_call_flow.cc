// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_notification/server_client/push_notification_desktop_api_call_flow.h"

namespace push_notification {

PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError
PushNotificationDesktopApiCallFlow::GetErrorForHttpResponseCode(
    int response_code) {
  if (response_code == 400) {
    return PushNotificationDesktopApiCallFlow::
        PushNotificationApiCallFlowError::kBadRequest;
  }

  if (response_code == 403) {
    return PushNotificationDesktopApiCallFlow::
        PushNotificationApiCallFlowError::kAuthenticationError;
  }

  if (response_code == 404) {
    return PushNotificationDesktopApiCallFlow::
        PushNotificationApiCallFlowError::kEndpointNotFound;
  }

  if (response_code >= 500 && response_code < 600) {
    return PushNotificationDesktopApiCallFlow::
        PushNotificationApiCallFlowError::kInternalServerError;
  }

  return PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError::
      kUnknown;
}

std::ostream& operator<<(
    std::ostream& stream,
    const PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError&
        error) {
  switch (error) {
    case PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError::
        kOffline:
      stream << "[offline]";
      break;
    case PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError::
        kEndpointNotFound:
      stream << "[endpoint not found]";
      break;
    case PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError::
        kAuthenticationError:
      stream << "[authentication error]";
      break;
    case PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError::
        kBadRequest:
      stream << "[bad request]";
      break;
    case PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError::
        kResponseMalformed:
      stream << "[response malformed]";
      break;
    case PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError::
        kInternalServerError:
      stream << "[internal server error]";
      break;
    case PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError::
        kUnknown:
      stream << "[unknown]";
      break;
  }
  return stream;
}

}  // namespace push_notification
