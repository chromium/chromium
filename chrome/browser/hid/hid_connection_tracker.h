// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_HID_CONNECTION_TRACKER_H_
#define CHROME_BROWSER_HID_HID_CONNECTION_TRACKER_H_

#include "base/containers/flat_map.h"
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

  virtual void IncrementConnectionCount(const url::Origin& origin);
  virtual void DecrementConnectionCount(const url::Origin& origin);

  virtual void ShowContentSettingsExceptions();
  virtual void ShowSiteSettings(const url::Origin& origin);

  // This is used by either the destructor or
  // HidConnectionTrackerFactory::BrowserContextShutdown to remove its profile
  // from HidSystemTrayIcon.
  void CleanUp();

  int total_connection_count() { return total_connection_count_; }
  Profile* profile() { return profile_; }

  const base::flat_map<url::Origin, int>& origins() { return origins_; }

 private:
  int total_connection_count_ = 0;
  raw_ptr<Profile> profile_;

  // The structure that tracks the connection count for each origin that has
  // active connection(s).
  base::flat_map<url::Origin, int> origins_;
};

#endif  // CHROME_BROWSER_HID_HID_CONNECTION_TRACKER_H_
