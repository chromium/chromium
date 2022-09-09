// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MTPDeviceObjectEntry implementation.

#include "chrome/browser/media_galleries/win/mtp_device_object_entry.h"

MTPDeviceObjectEntry::MTPDeviceObjectEntry() : is_directory(false), size(0) {
}

MTPDeviceObjectEntry::MTPDeviceObjectEntry(const std::wstring& object_id,
                                           const std::u16string& object_name,
                                           bool is_directory,
                                           int64_t size,
                                           const base::Time& last_modified_time)
    : object_id(object_id),
      name(object_name),
      is_directory(is_directory),
      size(size),
      last_modified_time(last_modified_time) {}
