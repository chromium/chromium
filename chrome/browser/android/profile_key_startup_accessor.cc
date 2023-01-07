// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/profile_key_startup_accessor.h"

#include "base/check.h"

ProfileKeyStartupAccessor::ProfileKeyStartupAccessor() : key_(nullptr) {}

// static
ProfileKeyStartupAccessor* ProfileKeyStartupAccessor::GetInstance() {
  static ProfileKeyStartupAccessor instance;
  return &instance;
}

void ProfileKeyStartupAccessor::SetProfileKey(ProfileKey* key) {
  DCHECK(!key_);
  key_ = key;
}

void ProfileKeyStartupAccessor::Reset() {
  key_ = nullptr;
}
