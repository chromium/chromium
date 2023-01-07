// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class enumerates the media transfer protocol (MTP) device objects from
// a given object entry list.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_WIN_MTP_DEVICE_OBJECT_ENUMERATOR_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_WIN_MTP_DEVICE_OBJECT_ENUMERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include "base/files/file_path.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chrome/browser/media_galleries/win/mtp_device_object_entry.h"

// MTPDeviceObjectEnumerator is used to enumerate the media transfer protocol
// (MTP) device objects from a given object entry list.
// MTPDeviceObjectEnumerator supports MTP device file operations.
// MTPDeviceObjectEnumerator may only be used on a single thread.
class MTPDeviceObjectEnumerator {
 public:
  explicit MTPDeviceObjectEnumerator(const MTPDeviceObjectEntries& entries);

  MTPDeviceObjectEnumerator(const MTPDeviceObjectEnumerator&) = delete;
  MTPDeviceObjectEnumerator& operator=(const MTPDeviceObjectEnumerator&) =
      delete;

  ~MTPDeviceObjectEnumerator();

  base::FilePath Next();
  int64_t Size();
  bool IsDirectory();
  base::Time LastModifiedTime();

  // If the current file object entry is valid, returns an non-empty object id.
  // Returns an empty string otherwise.
  std::wstring GetObjectId() const;

 private:
  // Returns true if the enumerator has more entries to traverse, false
  // otherwise.
  bool HasMoreEntries() const;

  // Returns true if Next() has been called at least once, and the enumerator
  // has more entries to traverse.
  bool IsIndexReadyAndInRange() const;

  // List of directory file object entries.
  MTPDeviceObjectEntries object_entries_;

  // Index into |object_entries_|.
  // Should only be used when |is_index_ready_| is true.
  size_t index_;

  // Initially false. Set to true after Next() has been called.
  bool is_index_ready_;

  base::ThreadChecker thread_checker_;
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_WIN_MTP_DEVICE_OBJECT_ENUMERATOR_H_
