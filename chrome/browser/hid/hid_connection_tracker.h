// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_HID_CONNECTION_TRACKER_H_
#define CHROME_BROWSER_HID_HID_CONNECTION_TRACKER_H_

#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/origin.h"

// Manages the opened device connection count by the profile.
class HidConnectionTracker : public KeyedService {
 public:
  explicit HidConnectionTracker(Profile* profile);
  HidConnectionTracker(HidConnectionTracker&&) = delete;
  HidConnectionTracker& operator=(HidConnectionTracker&) = delete;
  ~HidConnectionTracker() override;

  virtual void IncrementConnectionCount();
  virtual void DecrementConnectionCount();

  // Generate a notification about a connection created for |origin|.
  virtual void NotifyDeviceConnected(const url::Origin& origin);

  virtual void ShowHidContentSettingsExceptions();
  virtual void ShowSiteSettings(const url::Origin& origin);

  // This is used by either the destructor or
  // HidConnectionTrackerFactory::BrowserContextShutdown to remove its profile
  // from HidSystemTrayIcon.
  void CleanUp();

  int connection_count() { return connection_count_; }
  Profile* profile() { return profile_; }

 private:
  int connection_count_ = 0;
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_HID_HID_CONNECTION_TRACKER_H_
