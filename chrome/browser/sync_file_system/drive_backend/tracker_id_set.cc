// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/tracker_id_set.h"

#include "base/check_op.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"

namespace sync_file_system {
namespace drive_backend {

TrackerIDSet::TrackerIDSet() : active_tracker_(0) {}

TrackerIDSet::TrackerIDSet(const TrackerIDSet& other) = default;

TrackerIDSet::~TrackerIDSet() = default;

void TrackerIDSet::Insert(const FileTracker& tracker) {
  if (tracker.active())
    InsertActiveTracker(tracker.tracker_id());
  else
    InsertInactiveTracker(tracker.tracker_id());
}

void TrackerIDSet::InsertActiveTracker(int64_t tracker_id) {
  CHECK(tracker_id, base::NotFatalUntil::M162);
  CHECK(!active_tracker_, base::NotFatalUntil::M162);

  active_tracker_ = tracker_id;
  tracker_ids_.insert(tracker_id);
}

void TrackerIDSet::InsertInactiveTracker(int64_t tracker_id) {
  CHECK(tracker_id, base::NotFatalUntil::M162);
  CHECK_NE(active_tracker_, tracker_id, base::NotFatalUntil::M162);

  tracker_ids_.insert(tracker_id);
}

void TrackerIDSet::Erase(int64_t tracker_id) {
  CHECK(tracker_ids_.contains(tracker_id), base::NotFatalUntil::M162);

  if (active_tracker_ == tracker_id)
    active_tracker_ = 0;
  tracker_ids_.erase(tracker_id);
}

void TrackerIDSet::Activate(int64_t tracker_id) {
  CHECK(!active_tracker_, base::NotFatalUntil::M162);
  CHECK(tracker_ids_.contains(tracker_id), base::NotFatalUntil::M162);
  active_tracker_ = tracker_id;
}

void TrackerIDSet::Deactivate(int64_t tracker_id) {
  CHECK_EQ(active_tracker_, tracker_id, base::NotFatalUntil::M162);
  CHECK(tracker_ids_.contains(tracker_id), base::NotFatalUntil::M162);
  active_tracker_ = 0;
}

}  // namespace drive_backend
}  // namespace sync_file_system
