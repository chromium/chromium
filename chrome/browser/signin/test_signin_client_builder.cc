// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/test_signin_client_builder.h"

#include "chrome/browser/profiles/profile.h"
#include "components/signin/public/base/test_signin_client.h"

namespace signin {

std::unique_ptr<KeyedService> BuildTestSigninClient(
    content::BrowserContext* context) {
  return std::make_unique<TestSigninClient>(
      static_cast<Profile*>(context)->GetPrefs());
}

}  // namespace signin
