// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_BACKGROUND_NTP_CUSTOM_BACKGROUND_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_SEARCH_BACKGROUND_NTP_CUSTOM_BACKGROUND_SERVICE_OBSERVER_H_

#include "base/observer_list_types.h"

// Observer for NtpCustomBackgroundService.
class NtpCustomBackgroundServiceObserver : public base::CheckedObserver {
 public:
  // Called when the custom background image is updated.
  virtual void OnCustomBackgroundImageUpdated() = 0;
};

#endif  // CHROME_BROWSER_SEARCH_BACKGROUND_NTP_CUSTOM_BACKGROUND_SERVICE_OBSERVER_H_
