// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_FAKE_SIGNIN_MANAGER_BUILDER_H_
#define CHROME_BROWSER_SIGNIN_FAKE_SIGNIN_MANAGER_BUILDER_H_

#include <memory>
#include <string>

#include "build/build_config.h"
#include "components/signin/core/browser/fake_signin_manager.h"

namespace content {
class BrowserContext;
}

class KeyedService;
class Profile;

// Helper function to be used with KeyedService::SetTestingFactory().
// In order to match the API of SigninManagerFactory::GetForProfile(), returns a
// FakeSigninManagerBase* on ChromeOS, and a FakeSigninManager* on all other
// platforms. The returned instance is initialized.
std::unique_ptr<KeyedService> BuildFakeSigninManagerForTesting(
    content::BrowserContext* context);

class FakeSigninManagerForTesting
#if defined(OS_CHROMEOS)
    : public FakeSigninManagerBase {
#else
    : public FakeSigninManager {
#endif

 public:
  FakeSigninManagerForTesting(Profile* profile);
};

#endif  // CHROME_BROWSER_SIGNIN_FAKE_SIGNIN_MANAGER_BUILDER_H_
