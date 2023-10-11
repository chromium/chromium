// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPOSE_COMPOSE_ENABLING_H_
#define CHROME_BROWSER_COMPOSE_COMPOSE_ENABLING_H_

#include "chrome/browser/profiles/profile_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class ComposeEnabling {
 public:
  static bool IsEnabledForProfile(Profile* profile);

 private:
  friend class ComposeEnablingTest;
  static bool IsEnabled(Profile* profile,
                        signin::IdentityManager* identity_manager);
};

#endif  // CHROME_BROWSER_COMPOSE_COMPOSE_ENABLING_H_
