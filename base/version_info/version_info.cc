// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/version_info/version_info.h"

#include <string>

#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/version.h"

namespace version_info {

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
