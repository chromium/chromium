// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_token_hardware_checker.h"

#include "base/logging.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_features_util.h"

namespace borealis {

namespace {

constexpr uint64_t kGibi = 1024ull * 1024 * 1024;

// Regex used for CPU checks on intel processors, this means "any 11th
// generation or greater i5/i7 processor".
constexpr char kBorealisCapableIntelCpuRegex[] = "[1-9][1-9].. Gen.*i[357]-";

}  // namespace

using AllowStatus = BorealisFeatures::AllowStatus;

// static
AllowStatus BorealisTokenHardwareChecker::BuildAndCheck(Data data) {
  return BorealisTokenHardwareChecker(std::move(data)).Check();
}

BorealisTokenHardwareChecker::BorealisTokenHardwareChecker(Data data)
    : TokenHardwareChecker(std::move(data)) {}

BorealisTokenHardwareChecker::~BorealisTokenHardwareChecker() = default;

AllowStatus BorealisTokenHardwareChecker::Check() const {
  // Tokens provide more fine-grained control over whether borealis can be run
  // on a specific device. The different kinds of token are:
  //  * "Super" token: Allows borealis on any device.
  //  * "Test" token: Allows borealis on any device with sufficient hardware
  //    (where *-borealis boards are always considered sufficient).
  //  * /board token: Similar to the super token, but only works for a subset
  //  of boards.
  //
  // All tokens will only function if borealis is already available on that
  // board based on its use flags.

  // The "super" token.
  if (TokenHashMatches("i9n6HT3+3Bo:C1p^_qk!\\",
                       "X1391g+2yiuBQrceA3gRGrT7+DQcaYGR/GkmFscyOfQ=")) {
    LOG(WARNING) << "Super-token provided, bypassing hardware checks.";
    return AllowStatus::kAllowed;
  }

  // The "test" token.
  if (TokenHashMatches("MpOI9+d58she4,97rI",
                       "Eec1m+UrIkLUu3L6mV+5zTYZId6HJ+vz+50MseJJaGw=")) {
    LOG(WARNING) << "Test-token provided, bypassing hardware checks.";
    return AllowStatus::kAllowed;
  }

  // The board-specific tokens.
  if (BoardIn({"hatch-borealis", "puff-borealis", "zork-borealis",
               "volteer-borealis", "aurora-borealis"})) {
    if (TokenHashMatches("MXlY+SFZ!2,P_k^02]hK",
                         "FbxB2mxNa/uqskX4X+NqHhAE6ebHeWC0u+Y+UlGEB/4=")) {
      LOG(WARNING) << "Dogfooder token provided, bypassing hardware checks.";
      return AllowStatus::kAllowed;
    }
    return AllowStatus::kIncorrectToken;
  } else if (IsBoard("volteer")) {
    if (TokenHashMatches("w/8GMLXyB.EOkFaP/-AA",
                         "waiTIRjxZCFjFIRkuUVlnAbiDOMBSzyp3iSJl5x3YwA=")) {
      LOG(WARNING) << "Vendor token provided, bypassing hardware checks.";
      return AllowStatus::kAllowed;
    }
    if (!ModelIn({"delbin", "voxel", "volta", "lindar", "elemi", "volet",
                  "drobit", "lillipup", "delbing", "eldrid", "chronicler"})) {
      return AllowStatus::kUnsupportedModel;
    }
    return ReleasedBoardChecks(kBorealisCapableIntelCpuRegex);
  } else if (BoardIn({"brya", "adlrvp", "brask"})) {
    if (TokenHashMatches("tPl24iMxXNR,w$h6,g",
                         "LWULWUcemqmo6Xvdu2LalOYOyo/V4/CkljTmAneXF+U=")) {
      LOG(WARNING) << "Vendor token provided, bypassing hardware checks.";
      return AllowStatus::kAllowed;
    }
    return ReleasedBoardChecks(kBorealisCapableIntelCpuRegex);
  } else if (BoardIn({"guybrush", "majolica"})) {
    if (TokenHashMatches("^_GkTVWDP.FQo5KclS",
                         "ftqv2wT3qeJKajioXqd+VrEW34CciMsigH3MGfMiMsU=")) {
      LOG(WARNING) << "Vendor token provided, bypassing hardware checks.";
      return AllowStatus::kAllowed;
    }
    return ReleasedBoardChecks("Ryzen [357]");
  } else if (IsBoard("draco")) {
    return AllowStatus::kAllowed;
  } else if (IsBoard("nissa")) {
    if (TokenHashMatches("nissa/!wcers4vuP7+2a/X$C8",
                         "24/U3nXWbTno/VJwp17HI+UDzWd77iXj5oDgavIZhoI=")) {
      LOG(WARNING) << "Nissa vendor token provided, bypassing hardware checks.";
      return AllowStatus::kAllowed;
    }
    // TODO(b/274537000): unblock for non-developer users.
    return AllowStatus::kIncorrectToken;
  } else if (IsBoard("skyrim")) {
    if (TokenHashMatches("skyrim/!2-DxWY_cL/nXF1U+oV",
                         "esBGhWX18eOMlNrqOS5oEcFfyy0MbNJ5VWz+92iVOwk=")) {
      LOG(WARNING)
          << "Skyrim vendor token provided, bypassing hardware checks.";
      return AllowStatus::kAllowed;
    }
    // TODO(b/274537000): unblock for non-developer users.
    return AllowStatus::kIncorrectToken;
  }
  return AllowStatus::kIncorrectToken;
}

AllowStatus BorealisTokenHardwareChecker::ReleasedBoardChecks(
    const std::string& cpu_regex) const {
  if (!HasMemory(7 * kGibi)) {
    return AllowStatus::kHardwareChecksFailed;
  }
  return CpuRegexMatches(cpu_regex) ? AllowStatus::kAllowed
                                    : AllowStatus::kHardwareChecksFailed;
}

}  // namespace borealis
