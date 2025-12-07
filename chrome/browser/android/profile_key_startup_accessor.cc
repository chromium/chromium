// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/profile_key_startup_accessor.h"

#include <type_traits>

#include "base/check.h"
#include "base/no_destructor.h"

ProfileKeyStartupAccessor::ProfileKeyStartupAccessor() : key_(nullptr) {}

// Hack to satisfy the compiler: ProfileKeyStartupAccessor's raw_ptr member
// may or may not be trivially destructible. If it isn't we have to use
// NoDestructor; if it is, we must not use NoDestructor. We use constexpr if
// to choose which to do, but the compiler will complain about the unused
// branch unless it's inside of a template, which is why this function exists.
template <typename T>
T* GetInstanceTemplated() {
  if constexpr (std::is_trivially_destructible_v<T>) {
    static T instance;
    return &instance;
  } else {
    static base::NoDestructor<T> instance;
    return instance.get();
  }
}

// static
ProfileKeyStartupAccessor* ProfileKeyStartupAccessor::GetInstance() {
  return GetInstanceTemplated<ProfileKeyStartupAccessor>();
}

void ProfileKeyStartupAccessor::SetProfileKey(ProfileKey* key) {
  DCHECK(!key_);
  key_ = key;
}

void ProfileKeyStartupAccessor::Reset() {
  key_ = nullptr;
}
