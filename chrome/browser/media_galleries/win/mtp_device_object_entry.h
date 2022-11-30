// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_WIN_MTP_DEVICE_OBJECT_ENTRY_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_WIN_MTP_DEVICE_OBJECT_ENTRY_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/time/time.h"

// MTPDeviceObjectEntry contains the media transfer protocol device object
// property values that are obtained using
// IPortableDeviceProperties::GetValues().
struct MTPDeviceObjectEntry {
  MTPDeviceObjectEntry();  // Necessary for STL.
  MTPDeviceObjectEntry(const std::wstring& object_id,
                       const std::u16string& object_name,
                       bool is_directory,
                       int64_t size,
                       const base::Time& last_modified_time);

  // The object identifier obtained using IEnumPortableDeviceObjectIDs::Next(),
  // e.g. "o299".
  std::wstring object_id;

  // Friendly name of the object, e.g. "IMG_9911.jpeg".
  std::u16string name;

  // True if the current object is a directory/folder/album content type.
  bool is_directory;

  // The object file size in bytes, e.g. "882992".
  int64_t size;

  // Last modified time of the object.
  base::Time last_modified_time;
};

typedef std::vector<MTPDeviceObjectEntry> MTPDeviceObjectEntries;

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_WIN_MTP_DEVICE_OBJECT_ENTRY_H_
