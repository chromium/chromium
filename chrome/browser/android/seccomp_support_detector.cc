// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/seccomp_support_detector.h"

#include <sys/utsname.h>

#include <string_view>
#include <vector>

#include "base/compiler_specific.h"
#include "base/cpu.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"

namespace {

// Reports the kernel version obtained from uname.
void ReportKernelVersion() {
  // This method will report the kernel major and minor versions by
  // taking the lower 16 bits of each version number and combining
  // the two into a 32-bit number.

  utsname uts;
  if (uname(&uts) != 0) {
    return;
  }

  std::vector<std::string_view> parts = base::SplitStringPiece(
      uts.release, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() < 2) {
    return;
  }

  int major;
  if (!base::StringToInt(parts[0], &major)) {
    return;
  }

  // Handle minor versions with suffixes (e.g., "18-generic").
  std::string_view minor_str = parts[1];
  size_t suffix_pos = minor_str.find_first_not_of("0123456789");
  if (suffix_pos != std::string_view::npos) {
    minor_str = minor_str.substr(0, suffix_pos);
  }

  int minor;
  if (!base::StringToInt(minor_str, &minor)) {
    return;
  }

  int version = ((major & 0xFFFF) << 16) | (minor & 0xFFFF);
  base::UmaHistogramSparse("Android.KernelVersion", version);
}

}  // namespace

void ReportSeccompSupport() {
  ReportKernelVersion();
}
