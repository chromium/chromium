// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_TRACKER_ID_SET_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_TRACKER_ID_SET_H_

#include <stddef.h>
#include <stdint.h>

#include <set>


namespace sync_file_system {
namespace drive_backend {

class FileTracker;

class TrackerIDSet {
 public:
  typedef std::set<int64_t> RawTrackerIDSet;
  typedef RawTrackerIDSet::iterator iterator;
  typedef RawTrackerIDSet::const_iterator const_iterator;

  TrackerIDSet();
  TrackerIDSet(const TrackerIDSet& other);
  ~TrackerIDSet();

  bool has_active() const { return !!active_tracker_; }
  void Insert(const FileTracker& tracker);
  void InsertActiveTracker(int64_t tracker_id);
  void InsertInactiveTracker(int64_t tracker_id);
  void Erase(int64_t tracker_id);
  void Activate(int64_t tracker_id);
  void Deactivate(int64_t tracker_id);

  int64_t active_tracker() const { return active_tracker_; }
  const RawTrackerIDSet& tracker_set() const { return tracker_ids_; }

  iterator begin() { return tracker_ids_.begin(); }
  iterator end() { return tracker_ids_.end(); }
  const_iterator begin() const { return tracker_ids_.begin(); }
  const_iterator end() const { return tracker_ids_.end(); }
  bool empty() const { return tracker_ids_.empty(); }
  size_t size() const { return tracker_ids_.size(); }

 private:
  int64_t active_tracker_;
  RawTrackerIDSet tracker_ids_;
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_TRACKER_ID_SET_H_
