// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/chromeos/mtp_device_object_enumerator.h"

#include <utility>

#include "base/check.h"

MTPDeviceObjectEnumerator::MTPDeviceObjectEnumerator(
    std::vector<device::mojom::MtpFileEntryPtr> entries)
    : file_entries_(std::move(entries)), index_(0U), is_index_ready_(false) {}

MTPDeviceObjectEnumerator::~MTPDeviceObjectEnumerator() {
}

base::FilePath MTPDeviceObjectEnumerator::Next() {
  if (IsIndexReadyAndInRange())
    ++index_;  // Normal traversal.
  else if (!is_index_ready_)
    is_index_ready_ = true;  // First time calling Next().

  if (!HasMoreEntries())
    return base::FilePath();
  return base::FilePath(file_entries_[index_]->file_name);
}

int64_t MTPDeviceObjectEnumerator::Size() {
  if (!IsIndexReadyAndInRange())
    return 0;
  return file_entries_[index_]->file_size;
}

bool MTPDeviceObjectEnumerator::IsDirectory() {
  if (!IsIndexReadyAndInRange())
    return false;
  return file_entries_[index_]->file_type ==
         device::mojom::MtpFileEntry::FileType::FILE_TYPE_FOLDER;
}

base::Time MTPDeviceObjectEnumerator::LastModifiedTime() {
  if (!IsIndexReadyAndInRange())
    return base::Time();
  return base::Time::FromTimeT(file_entries_[index_]->modification_time);
}

bool MTPDeviceObjectEnumerator::GetEntryId(uint32_t* entry_id) const {
  DCHECK(entry_id);
  if (!IsIndexReadyAndInRange())
    return false;

  *entry_id = file_entries_[index_]->item_id;
  return true;
}

bool MTPDeviceObjectEnumerator::HasMoreEntries() const {
  return index_ < file_entries_.size();
}

bool MTPDeviceObjectEnumerator::IsIndexReadyAndInRange() const {
  return is_index_ready_ && HasMoreEntries();
}
