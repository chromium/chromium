// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_WATCHER_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_WATCHER_H_

#include <map>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "storage/browser/file_system/watcher_manager.h"
#include "url/gurl.h"

namespace chromeos {
namespace file_system_provider {

struct Watcher;
struct Subscriber;

// Key for storing a watcher in the map. There may be two watchers per path,
// as long as one is recursive, and the other one not.
struct WatcherKey {
  WatcherKey(const base::FilePath& entry_path, bool recursive);
  ~WatcherKey();

  struct Comparator {
    bool operator()(const WatcherKey& a, const WatcherKey& b) const;
  };

  base::FilePath entry_path;
  bool recursive;
};

// List of watchers.
typedef std::map<WatcherKey, Watcher, WatcherKey::Comparator> Watchers;

// Map of subscribers for notifications about a watcher.
typedef std::map<GURL, Subscriber> Subscribers;

// Represents a subscriber for notification about a watcher. There may be up to
// one subscriber per origin for the same watcher.
struct Subscriber {
  Subscriber();
  Subscriber(const Subscriber& other);
  ~Subscriber();

  // Origin of the subscriber.
  GURL origin;

  // Whether the subscriber should be restored after shutdown or not.
  bool persistent;

  // Callback to be called for each watcher notification. It's optional, but
  // not allowed for persistent watchers. In case of persistent subscribers,
  // the notification should be handled using observers, as the callback can't
  // be restored after shutdown.
  storage::WatcherManager::NotificationCallback notification_callback;
};

// Represents a watcher on a file system.
struct Watcher {
  Watcher();
  Watcher(const Watcher& other);
  ~Watcher();

  // Map of subscribers for notifications of the watcher.
  Subscribers subscribers;

  // Path of the watcher.
  base::FilePath entry_path;

  // Whether watching is recursive or not.
  bool recursive;

  // Tag of the last notification for this watcher. May be empty if not
  // supported.
  std::string last_tag;
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_WATCHER_H_
