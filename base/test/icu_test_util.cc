// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/icu_test_util.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/i18n/rtl.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace base {
namespace test {

ScopedRestoreICUDefaultLocale::ScopedRestoreICUDefaultLocale()
    : ScopedRestoreICUDefaultLocale(std::string()) {}

ScopedRestoreICUDefaultLocale::ScopedRestoreICUDefaultLocale(
    const std::string& locale)
    : default_locale_(uloc_getDefault()) {
  if (!locale.empty())
    i18n::SetICUDefaultLocale(locale.data());
}

ScopedRestoreICUDefaultLocale::~ScopedRestoreICUDefaultLocale() {
  i18n::SetICUDefaultLocale(default_locale_.data());
}

ScopedRestoreDefaultTimezone::ScopedRestoreDefaultTimezone(const char* zoneid) {
  original_zone_.reset(icu::TimeZone::createDefault());
  icu::TimeZone::adoptDefault(icu::TimeZone::createTimeZone(zoneid));
}

ScopedRestoreDefaultTimezone::~ScopedRestoreDefaultTimezone() {
  icu::TimeZone::adoptDefault(original_zone_.release());
}

void InitializeICUForTesting() {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTestDoNotInitializeIcu)) {
    i18n::AllowMultipleInitializeCallsForTesting();
    i18n::InitializeICU();
  }
}

}  // namespace test
}  // namespace base
