// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_token_hardware_checker.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_features_util.h"

namespace borealis {

namespace {

constexpr uint64_t kGibi = 1024ull * 1024 * 1024;

// Regex used for CPU checks on intel processors, this means "any i{3,5,7}
// processor". e.g.:
//  - Valid:   11th Gen Intel(R) Core(TM) i5-1145G7 @ 2.60GHz
//  - Valid:   Intel(R) Core(TM) 5 ...
//  - Invalid: Intel(R) Pentium(R) Gold 7505
constexpr char kIntelCpuRegex[] = "((i[357]-)|(Core.* [357]))";

// As above, for AMD processors, e.g. "AMD Ryzen 3 5125C with Radeon Graphics".
constexpr char kAmdCpuRegex[] = "Ryzen [357]";

}  // namespace

// static
bool BorealisTokenHardwareChecker::BuildAndCheck(Data data) {
  return BorealisTokenHardwareChecker(std::move(data)).Check();
}

BorealisTokenHardwareChecker::BorealisTokenHardwareChecker(Data data)
    : TokenHardwareChecker(std::move(data)) {}

BorealisTokenHardwareChecker::~BorealisTokenHardwareChecker() = default;

bool BorealisTokenHardwareChecker::Check() const {
  if (IsBoard("volteer")) {
    bool valid_model =
        ModelIn({"delbin", "voxel", "volta", "lindar", "elemi", "volet",
                 "drobit", "lillipup", "delbing", "eldrid", "chronicler"});
    if (HasSufficientHardware(kIntelCpuRegex) && valid_model) {
      return true;
    }
    return false;
  } else if (BoardIn({"brya", "adlrvp", "brask", "hatch"})) {
    if (HasSufficientHardware(kIntelCpuRegex)) {
      return true;
    }
    return false;
  } else if (BoardIn({"guybrush", "majolica"})) {
    if (HasSufficientHardware(kAmdCpuRegex)) {
      return true;
    }
    return false;
  } else if (BoardIn({"aurora"})) {
    return true;
  } else if (BoardIn({"myst"})) {
    return true;
  } else if (IsBoard("nissa")) {
    if (HasSufficientHardware(kIntelCpuRegex) && InTargetSegment()) {
      return true;
    }
  } else if (IsBoard("skyrim")) {
    if (HasSufficientHardware(kAmdCpuRegex) && InTargetSegment()) {
      return true;
    }
  } else if (IsBoard("rex")) {
    // TODO(307825451): .* allows any CPU, add the correct cpu regex once we
    // know what that is.
    if (HasSufficientHardware(".*")) {
      return true;
    }
    return false;
  }
  return false;
}

bool BorealisTokenHardwareChecker::HasSufficientHardware(
    const std::string& cpu_regex) const {
  return HasMemory(7 * kGibi) && CpuRegexMatches(cpu_regex);
}

bool BorealisTokenHardwareChecker::InTargetSegment() const {
  return base::FeatureList::IsEnabled(
      ash::features::kFeatureManagementBorealis);
}

}  // namespace borealis
