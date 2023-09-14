// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_CONNECTION_TRACKER_H_
#define CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_CONNECTION_TRACKER_H_

#include "base/containers/flat_map.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/origin.h"

class DeviceSystemTrayIcon;

// Manages the opened device connection count by the profile.
class DeviceConnectionTracker : public KeyedService {
 public:
  struct OriginState {
    // The number of active connections.
    int count;
    // The last time the state was updated.
    base::TimeTicks timestamp;
    // String representation for the origin.
    std::string name;

    bool operator==(const OriginState& other) const {
      return count == other.count && timestamp == other.timestamp &&
             name == other.name;
    }
  };

  explicit DeviceConnectionTracker(Profile* profile);
  DeviceConnectionTracker(DeviceConnectionTracker&&) = delete;
  DeviceConnectionTracker& operator=(DeviceConnectionTracker&) = delete;
  ~DeviceConnectionTracker() override;

  virtual void IncrementConnectionCount(const url::Origin& origin);
  virtual void DecrementConnectionCount(const url::Origin& origin);

  virtual void ShowContentSettingsExceptions() = 0;
  virtual void ShowSiteSettings(const url::Origin& origin);

  // This is used by either the destructor or
  // the corresponding ConnectionTrackerFactory::BrowserContextShutdown to
  // remove its profile from DeviceSystemTrayIcon.
  void CleanUp();

  int total_connection_count() { return total_connection_count_; }
  Profile* profile() { return profile_; }

  const base::flat_map<url::Origin, OriginState>& origins() { return origins_; }

  // The time period that an origin remains tracked before it is removed from
  // `origins_`.
  static constexpr base::TimeDelta kOriginInactiveTime = base::Seconds(3);

  // Removes the `origin` from the `origins_` list if it has not had any new
  // connections since `timestamp`.
  void CleanUpOrigin(const url::Origin& origin,
                     const base::TimeTicks& timestamp);

  int total_connection_count_ = 0;
  raw_ptr<Profile> profile_;

  // The structure that tracks the connection count for each origin that has
  // active connection(s).
  base::flat_map<url::Origin, OriginState> origins_;

  // Connection from whitelisted origins will not be tracked.
  std::vector<url::Origin> whitelisted_origins_;

 private:
  virtual DeviceSystemTrayIcon* GetSystemTrayIcon() = 0;

  base::WeakPtrFactory<DeviceConnectionTracker> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_CONNECTION_TRACKER_H_
