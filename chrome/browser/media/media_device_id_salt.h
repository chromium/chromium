// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_DEVICE_ID_SALT_H_
#define CHROME_BROWSER_MEDIA_MEDIA_DEVICE_ID_SALT_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_member.h"

class PrefService;

// MediaDeviceIDSalt is responsible for creating and retrieving a salt string
// that is used for creating MediaSource IDs that can be cached by a web
// service. If the cache is cleared, the  MediaSourceIds are invalidated.
//
// The class is reference counted so that it can be used in the
// callback returned by ResourceContext::GetMediaDeviceIDSalt.
class MediaDeviceIDSalt : public base::RefCountedThreadSafe<MediaDeviceIDSalt> {
 public:
  explicit MediaDeviceIDSalt(PrefService* pref_service);

  MediaDeviceIDSalt(const MediaDeviceIDSalt&) = delete;
  MediaDeviceIDSalt& operator=(const MediaDeviceIDSalt&) = delete;

  std::string GetSalt() const;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
  static void Reset(PrefService* pref_service);

 private:
  friend class base::RefCountedThreadSafe<MediaDeviceIDSalt>;
  ~MediaDeviceIDSalt();

  mutable StringPrefMember media_device_id_salt_;
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_DEVICE_ID_SALT_H_
