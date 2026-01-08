// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_COMMON_GLIC_TAB_OBSERVER_ANDROID_H_
#define CHROME_BROWSER_GLIC_COMMON_GLIC_TAB_OBSERVER_ANDROID_H_

#include "chrome/browser/glic/common/glic_tab_observer.h"

class Profile;

// Stub implementation of GlicTabObserver for Android.
class GlicTabObserverAndroid : public GlicTabObserver {
 public:
  GlicTabObserverAndroid(Profile* profile, EventCallback callback);
  ~GlicTabObserverAndroid() override;
};

#endif  // CHROME_BROWSER_GLIC_COMMON_GLIC_TAB_OBSERVER_ANDROID_H_
