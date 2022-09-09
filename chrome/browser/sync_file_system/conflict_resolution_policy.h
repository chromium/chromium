// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_CONFLICT_RESOLUTION_POLICY_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_CONFLICT_RESOLUTION_POLICY_H_

namespace sync_file_system {

enum ConflictResolutionPolicy {
  // Resolution policy unknown or not initialized. Usually indicates an error.
  CONFLICT_RESOLUTION_POLICY_UNKNOWN = 0,

  // The service automatically resolves a conflict by choosing the one
  // that is updated more recently.
  CONFLICT_RESOLUTION_POLICY_LAST_WRITE_WIN,

  // The service does nothing and just leaves conflicting files in
  // 'conflicted' state.
  CONFLICT_RESOLUTION_POLICY_MANUAL,

  CONFLICT_RESOLUTION_POLICY_MAX,
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_CONFLICT_RESOLUTION_POLICY_H_
