// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOGIN_DETECTION_LOGIN_DETECTION_KEYED_SERVICE_H_
#define CHROME_BROWSER_LOGIN_DETECTION_LOGIN_DETECTION_KEYED_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace login_detection {

// Keyed service that initializes login detection state (such as applying site
// isolation to previously saved OAuth logged-in sites) for a BrowserContext.
class LoginDetectionKeyedService : public KeyedService {
 public:
  explicit LoginDetectionKeyedService(content::BrowserContext* browser_context);
  ~LoginDetectionKeyedService() override;

  LoginDetectionKeyedService(const LoginDetectionKeyedService&) = delete;
  LoginDetectionKeyedService& operator=(const LoginDetectionKeyedService&) =
      delete;
};

}  // namespace login_detection

#endif  // CHROME_BROWSER_LOGIN_DETECTION_LOGIN_DETECTION_KEYED_SERVICE_H_
