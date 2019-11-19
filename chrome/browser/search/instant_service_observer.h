// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_INSTANT_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_SEARCH_INSTANT_SERVICE_OBSERVER_H_

#include <vector>

#include "build/build_config.h"

#if defined(OS_ANDROID)
#error "Instant is only used on desktop";
#endif

struct InstantMostVisitedInfo;
struct NtpTheme;

// InstantServiceObserver defines the observer interface for InstantService.
class InstantServiceObserver {
 public:
  // Indicates that the user's custom theme has changed in some way.
  virtual void NtpThemeChanged(const NtpTheme&);

  // Indicates that the most visited items have changed in some way.
  virtual void MostVisitedInfoChanged(const InstantMostVisitedInfo&);

 protected:
  virtual ~InstantServiceObserver() {}
};

#endif  // CHROME_BROWSER_SEARCH_INSTANT_SERVICE_OBSERVER_H_
