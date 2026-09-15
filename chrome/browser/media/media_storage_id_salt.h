// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_STORAGE_ID_SALT_H_
#define CHROME_BROWSER_MEDIA_MEDIA_STORAGE_ID_SALT_H_

#include <stdint.h>

#include <vector>

class PrefRegistrySimple;
class Profile;

// MediaStorageIdSalt is responsible for creating and retrieving a salt string
// that is used when creating Storage IDs.
class MediaStorageIdSalt {
 public:
  enum { kSaltLength = 32 };

  MediaStorageIdSalt() = delete;
  MediaStorageIdSalt(const MediaStorageIdSalt&) = delete;
  MediaStorageIdSalt& operator=(const MediaStorageIdSalt&) = delete;

  // Retrieves the current salt for `profile`. If one does not currently exist
  // it is created. For off-the-record profiles, an ephemeral in-memory salt is
  // used.
  static std::vector<uint8_t> GetSalt(Profile* profile);

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_STORAGE_ID_SALT_H_
