// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CHROME_SECURITY_STATE_CLIENT_H_
#define CHROME_BROWSER_SSL_CHROME_SECURITY_STATE_CLIENT_H_

#include <memory>
#include "components/security_state/content/android/security_state_client.h"

class ChromeSecurityStateClient : public security_state::SecurityStateClient {
 public:
  constexpr ChromeSecurityStateClient() = default;
  ~ChromeSecurityStateClient() = default;

  std::unique_ptr<SecurityStateModelDelegate>
  MaybeCreateSecurityStateModelDelegate() override;
};

#endif  // CHROME_BROWSER_SSL_CHROME_SECURITY_STATE_CLIENT_H_
