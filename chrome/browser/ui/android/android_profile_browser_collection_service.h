// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_ANDROID_PROFILE_BROWSER_COLLECTION_SERVICE_H_
#define CHROME_BROWSER_UI_ANDROID_ANDROID_PROFILE_BROWSER_COLLECTION_SERVICE_H_

#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "components/keyed_service/core/keyed_service.h"

#if !BUILDFLAG(IS_ANDROID)
#error This file should only be included on Android.
#endif

class Profile;
class GlobalBrowserCollection;

// A profile keyed service that provides a collection of browser windows
// associated with a profile on Android.
class AndroidProfileBrowserCollectionService
    : public ProfileBrowserCollection,
      public KeyedService,
      public BrowserCollectionObserver {
 public:
  explicit AndroidProfileBrowserCollectionService(Profile* profile);

  AndroidProfileBrowserCollectionService(
      const AndroidProfileBrowserCollectionService&) = delete;
  AndroidProfileBrowserCollectionService& operator=(
      const AndroidProfileBrowserCollectionService&) = delete;

  ~AndroidProfileBrowserCollectionService() override;

  // BrowserCollection:
  bool IsEmpty() const override;
  size_t GetSize() const override;

 protected:
  // BrowserCollection:
  BrowserVector GetBrowsers(Order order) override;

 private:
  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;
  void OnBrowserActivated(BrowserWindowInterface* browser) override;
  void OnBrowserDeactivated(BrowserWindowInterface* browser) override;

  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};

  size_t browser_count_ = 0;
};

#endif  // CHROME_BROWSER_UI_ANDROID_ANDROID_PROFILE_BROWSER_COLLECTION_SERVICE_H_
