// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_INSTANT_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_SEARCH_INSTANT_SERVICE_OBSERVER_H_

#include "build/build_config.h"
#include "chrome/common/search/instant_types.h"

#if BUILDFLAG(IS_ANDROID)
#error "Instant is only used on desktop";
#endif

struct InstantMostVisitedInfo;

// InstantServiceObserver defines the observer interface for InstantService.
class InstantServiceObserver {
 public:
  // Indicates that the user's custom theme has changed in some way.
  virtual void NtpThemeChanged(NtpTheme);

  // Indicates that the most visited items have changed in some way.
  virtual void MostVisitedInfoChanged(const InstantMostVisitedInfo&);

 protected:
  virtual ~InstantServiceObserver() {}
};

#endif  // CHROME_BROWSER_SEARCH_INSTANT_SERVICE_OBSERVER_H_
