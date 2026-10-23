// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/geic_init.h"

#include "chrome/browser/glic/gemini_enterprise/geic_fre.h"

namespace geic {

void InitializeForProfile(Profile* profile) {
  // GEiC counterpart of the consumer `--glic-always-open-fre` handling in
  // `GlicKeyedService`. Must run before the FRE status is read.
  MaybeApplyResetFreSwitch(profile);
}

}  // namespace geic
