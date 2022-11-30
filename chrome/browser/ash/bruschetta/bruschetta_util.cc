// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_util.h"

namespace bruschetta {

const char kBruschettaVmName[] = "bru";
const char kBruschettaDisplayName[] = "Bruschetta";

const char* BruschettaResultString(const BruschettaResult res) {
#define ENTRY(name)            \
  case BruschettaResult::name: \
    return #name
  switch (res) {
    ENTRY(kUnknown);
    ENTRY(kSuccess);
    ENTRY(kDlcInstallError);
    ENTRY(kBiosNotAccessible);
    ENTRY(kStartVmFailed);
    ENTRY(kTimeout);
  }
#undef ENTRY
  return "unknown code";
}

guest_os::GuestId GetBruschettaId() {
  return guest_os::GuestId{guest_os::VmType::BRUSCHETTA, kBruschettaVmName,
                           "penguin"};
}

}  // namespace bruschetta
