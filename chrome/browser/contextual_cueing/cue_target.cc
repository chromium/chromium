// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/cue_target.h"

namespace contextual_cueing {

GlicCueActionData::GlicCueActionData() = default;
GlicCueActionData::~GlicCueActionData() = default;
GlicCueActionData::GlicCueActionData(const GlicCueActionData&) = default;
GlicCueActionData::GlicCueActionData(GlicCueActionData&&) = default;
GlicCueActionData& GlicCueActionData::operator=(const GlicCueActionData&) =
    default;

const char* GetName(CueTargetType type) {
  switch (type) {
    case CueTargetType::kGlic:
      return "Glic";
  }
}
}  // namespace contextual_cueing
