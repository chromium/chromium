// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_CHROMEOS_MTP_DEVICE_OBJECT_ENUMERATOR_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_CHROMEOS_MTP_DEVICE_OBJECT_ENUMERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "services/device/public/mojom/mtp_file_entry.mojom.h"

// Used to enumerate top-level files of an media file system.
class MTPDeviceObjectEnumerator {
 public:
  explicit MTPDeviceObjectEnumerator(
      std::vector<device::mojom::MtpFileEntryPtr> entries);

  MTPDeviceObjectEnumerator(const MTPDeviceObjectEnumerator&) = delete;
  MTPDeviceObjectEnumerator& operator=(const MTPDeviceObjectEnumerator&) =
      delete;

  ~MTPDeviceObjectEnumerator();

  base::FilePath Next();
  int64_t Size();
  bool IsDirectory();
  base::Time LastModifiedTime();

  // If the current file entry is valid, returns true and fills in |entry_id|
  // with the entry identifier else returns false and |entry_id| is not set.
  bool GetEntryId(uint32_t* entry_id) const;

 private:
  // Returns true if the enumerator has more entries to traverse, false
  // otherwise.
  bool HasMoreEntries() const;

  // Returns true if Next() has been called at least once, and the enumerator
  // has more entries to traverse.
  bool IsIndexReadyAndInRange() const;

  // List of directory file entries information.
  const std::vector<device::mojom::MtpFileEntryPtr> file_entries_;

  // Index into |file_entries_|.
  // Should only be used when |is_index_ready_| is true.
  size_t index_;

  // Initially false. Set to true after Next() has been called.
  bool is_index_ready_;
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_CHROMEOS_MTP_DEVICE_OBJECT_ENUMERATOR_H_
