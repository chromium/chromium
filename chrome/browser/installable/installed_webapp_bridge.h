// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTALLABLE_INSTALLED_WEBAPP_BRIDGE_H_
#define CHROME_BROWSER_INSTALLABLE_INSTALLED_WEBAPP_BRIDGE_H_

#include "base/macros.h"
#include "chrome/browser/installable/installed_webapp_provider.h"

class InstalledWebappBridge {
 public:
  static InstalledWebappProvider::RuleList
  GetInstalledWebappNotificationPermissions();

  static void SetProviderInstance(InstalledWebappProvider* provider);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(InstalledWebappBridge);
};

#endif  // CHROME_BROWSER_INSTALLABLE_INSTALLED_WEBAPP_BRIDGE_H_
