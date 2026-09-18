// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/cue_target.h"

#include <utility>

#include "base/notreached.h"

namespace contextual_cueing {

GlicCueActionData::GlicCueActionData() = default;
GlicCueActionData::~GlicCueActionData() = default;
GlicCueActionData::GlicCueActionData(const GlicCueActionData&) = default;
GlicCueActionData::GlicCueActionData(GlicCueActionData&&) = default;
GlicCueActionData& GlicCueActionData::operator=(const GlicCueActionData&) =
    default;

bool CueTarget::SupportsEditPrompt() const {
  return false;
}

const char* GetName(CueTargetType type) {
  switch (type) {
    case CueTargetType::kGlic:
      return "Glic";
    case CueTargetType::kTestSource:
      return "TestSource";
    case CueTargetType::kIndigo:
      return "Indigo";
  }
}

const char* GetName(CueIntrusiveness intrusiveness) {
  switch (intrusiveness) {
    case CueIntrusiveness::kLoud:
      return "Loud";
    case CueIntrusiveness::kQuiet:
      return "Quiet";
  }
}

bool CueTarget::SupportsIntrusiveness(CueIntrusiveness intrusiveness) const {
  if (RequiresModelExecution()) {
    return intrusiveness == CueIntrusiveness::kLoud;
  }
  return SupportsIntrusivenessImpl(intrusiveness);
}

bool CueTarget::SupportsIntrusivenessImpl(
    CueIntrusiveness intrusiveness) const {
  return true;
}

}  // namespace contextual_cueing
