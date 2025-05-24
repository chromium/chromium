// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MICROSOFT_AUTH_MICROSOFT_AUTH_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MICROSOFT_AUTH_MICROSOFT_AUTH_SERVICE_OBSERVER_H_

#include "base/observer_list_types.h"

// Observer for MicrosoftAuthService.
class MicrosoftAuthServiceObserver : public base::CheckedObserver {
 public:
  // Called when the auth state is updated.
  virtual void OnAuthStateUpdated() = 0;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MICROSOFT_AUTH_MICROSOFT_AUTH_SERVICE_OBSERVER_H_
