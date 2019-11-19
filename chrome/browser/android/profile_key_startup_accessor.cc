// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/profile_key_startup_accessor.h"

#include "base/logging.h"
#include "base/no_destructor.h"

ProfileKeyStartupAccessor::ProfileKeyStartupAccessor() : key_(nullptr) {}

// static
ProfileKeyStartupAccessor* ProfileKeyStartupAccessor::GetInstance() {
  static base::NoDestructor<ProfileKeyStartupAccessor> instance;
  return instance.get();
}

void ProfileKeyStartupAccessor::SetProfileKey(ProfileKey* key) {
  DCHECK(!key_);
  key_ = key;
}

void ProfileKeyStartupAccessor::Reset() {
  key_ = nullptr;
}
