// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/android_profile_browser_collection_service.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"

AndroidProfileBrowserCollectionService::AndroidProfileBrowserCollectionService(
    Profile* profile)
    : ProfileBrowserCollection(profile) {
  GlobalBrowserCollection* global_browser_collection =
      GlobalBrowserCollection::GetInstance();
  browser_collection_observation_.Observe(global_browser_collection);

  // The GlobalBrowserCollection has already been alive and possibly
  // accumulating browsers. We need a count of what it has so far.
  global_browser_collection->ForEach(
      [&](BrowserWindowInterface* browser) {
        if (browser->GetProfile() == &profile_.get()) {
          browser_count_++;
        }
        return true;
      },
      BrowserCollection::Order::kCreation);
}

AndroidProfileBrowserCollectionService::
    ~AndroidProfileBrowserCollectionService() = default;

bool AndroidProfileBrowserCollectionService::IsEmpty() const {
  return GetSize() == 0;
}

size_t AndroidProfileBrowserCollectionService::GetSize() const {
  return browser_count_;
}

// Note that this function loops through the whole GlobalBrowserCollection and
// makes an extra copy of the ProfileBrowserCollection vector. These lists
// should be small, so this is not expected to cause any noticeable performance
// issue. In the unlikely case that performance becomes important here, or if we
// have another reason for ProfileBrowserCollection to own its own browser
// lists, we can re-examine this approach:
// https://chromium-review.googlesource.com/c/chromium/src/+/7592561.
AndroidProfileBrowserCollectionService::BrowserVector
AndroidProfileBrowserCollectionService::GetBrowsers(Order order) {
  if (IsEmpty()) {
    return {};
  }

  BrowserVector browsers;
  GlobalBrowserCollection::GetInstance()->ForEach(
      [&](BrowserWindowInterface* browser) {
        if (browser->GetProfile() == &profile_.get()) {
          browsers.push_back(browser);
        }
        return true;
      },
      order);

  CHECK(GetSize() == browsers.size());

  return browsers;
}

void AndroidProfileBrowserCollectionService::OnBrowserCreated(
    BrowserWindowInterface* browser) {
  if (browser->GetProfile() != &profile_.get()) {
    return;
  }

  browser_count_++;
  for (BrowserCollectionObserver& observer : observers()) {
    observer.OnBrowserCreated(browser);
  }
}

void AndroidProfileBrowserCollectionService::OnBrowserClosed(
    BrowserWindowInterface* browser) {
  if (browser->GetProfile() != &profile_.get()) {
    return;
  }

  browser_count_--;
  for (BrowserCollectionObserver& observer : observers()) {
    observer.OnBrowserClosed(browser);
  }
}

void AndroidProfileBrowserCollectionService::OnBrowserActivated(
    BrowserWindowInterface* browser) {
  if (browser->GetProfile() != &profile_.get()) {
    return;
  }

  for (BrowserCollectionObserver& observer : observers()) {
    observer.OnBrowserActivated(browser);
  }
}

void AndroidProfileBrowserCollectionService::OnBrowserDeactivated(
    BrowserWindowInterface* browser) {
  if (browser->GetProfile() != &profile_.get()) {
    return;
  }

  for (BrowserCollectionObserver& observer : observers()) {
    observer.OnBrowserDeactivated(browser);
  }
}
