// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/obsolete_system/obsolete_system.h"

#include "base/cpu.h"
#include "build/build_config.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"

// static
bool ObsoleteSystem::IsObsoleteNowOrSoon() {
  return false;
}

// static
std::u16string ObsoleteSystem::LocalizedObsoleteString() {
  return std::u16string();
}

// static
bool ObsoleteSystem::IsEndOfTheLine() {
  return true;
}

// static
const char* ObsoleteSystem::GetLinkURL() {
  return "";
}
