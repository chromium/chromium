// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/chrome_security_state_client.h"

#include "chrome/browser/ssl/chrome_security_state_model_delegate.h"

std::unique_ptr<SecurityStateModelDelegate>
ChromeSecurityStateClient::MaybeCreateSecurityStateModelDelegate() {
  return std::make_unique<ChromeSecurityStateModelDelegate>();
}
