// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MTPDeviceObjectEnumerator implementation.

#include "chrome/browser/media_galleries/win/mtp_device_object_enumerator.h"

#include "base/logging.h"

MTPDeviceObjectEnumerator::MTPDeviceObjectEnumerator(
    const MTPDeviceObjectEntries& entries)
    : object_entries_(entries), index_(0U), is_index_ready_(false) {}

MTPDeviceObjectEnumerator::~MTPDeviceObjectEnumerator() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

base::FilePath MTPDeviceObjectEnumerator::Next() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (IsIndexReadyAndInRange())
    ++index_;  // Normal traversal.
  else if (!is_index_ready_)
    is_index_ready_ = true;  // First time calling Next().

  if (!HasMoreEntries())
    return base::FilePath();
  return base::FilePath(object_entries_[index_].name);
}

int64_t MTPDeviceObjectEnumerator::Size() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!IsIndexReadyAndInRange())
    return 0;
  return object_entries_[index_].size;
}

bool MTPDeviceObjectEnumerator::IsDirectory() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!IsIndexReadyAndInRange())
    return false;
  return object_entries_[index_].is_directory;
}

base::Time MTPDeviceObjectEnumerator::LastModifiedTime() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!IsIndexReadyAndInRange())
    return base::Time();
  return object_entries_[index_].last_modified_time;
}

base::string16 MTPDeviceObjectEnumerator::GetObjectId() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!IsIndexReadyAndInRange())
    return base::string16();
  return object_entries_[index_].object_id;
}

bool MTPDeviceObjectEnumerator::HasMoreEntries() const {
  return index_ < object_entries_.size();
}

bool MTPDeviceObjectEnumerator::IsIndexReadyAndInRange() const {
  return is_index_ready_ && HasMoreEntries();
}
