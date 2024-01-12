// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/watcher.h"

namespace ash::file_system_provider {

WatcherKey::WatcherKey(const base::FilePath& entry_path, bool recursive)
    : entry_path(entry_path), recursive(recursive) {
}

WatcherKey::~WatcherKey() = default;

bool WatcherKey::Comparator::operator()(const WatcherKey& a,
                                        const WatcherKey& b) const {
  if (a.entry_path != b.entry_path)
    return a.entry_path < b.entry_path;
  return a.recursive < b.recursive;
}

Subscriber::Subscriber() : persistent(false) {
}

Subscriber::Subscriber(const Subscriber& other) = default;

Subscriber::~Subscriber() = default;

Watcher::Watcher() : recursive(false) {
}

Watcher::Watcher(const Watcher& other) = default;

Watcher::~Watcher() = default;

}  // namespace ash::file_system_provider
