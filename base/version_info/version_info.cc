// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/version_info/version_info.h"

#include <string>

#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/version.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/apk_info.h"
#endif

namespace version_info {

#if BUILDFLAG(IS_ANDROID)
std::string_view GetVersionNumber() {
  return base::android::apk_info::package_version_name();
}
#endif

int GetMajorVersionNumberAsInt() {
  DCHECK(GetVersion().IsValid());
  return GetVersion().components()[0];
}

std::string GetMajorVersionNumber() {
  return base::NumberToString(GetMajorVersionNumberAsInt());
}

const base::Version& GetVersion() {
  static const base::NoDestructor<base::Version> version(GetVersionNumber());
  return *version;
}

}  // namespace version_info
