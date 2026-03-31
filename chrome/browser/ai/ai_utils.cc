// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_utils.h"

#include "base/logging.h"

namespace ai {

on_device_model::mojom::ResponseConstraintPtr ToMojomResponseConstraint(
    const optimization_guide::proto::ResponseConstraint& constraint) {
  switch (constraint.format_case()) {
    case optimization_guide::proto::ResponseConstraint::kRegex:
      if (constraint.regex().empty()) {
        VLOG(1) << "LLM response regex constraint is empty. This forbids the "
                   "model from creating any output.";
      }
      return on_device_model::mojom::ResponseConstraint::NewRegex(
          constraint.regex());
    case optimization_guide::proto::ResponseConstraint::kJsonSchema:
      if (constraint.json_schema().empty()) {
        VLOG(1) << "LLM response json schema constraint is empty. This forbids "
                   "the model from creating any output.";
      }
      return on_device_model::mojom::ResponseConstraint::NewJsonSchema(
          constraint.json_schema());
    case optimization_guide::proto::ResponseConstraint::FORMAT_NOT_SET:
      VLOG(1) << "LLM response constraint format is not set.";
      return nullptr;
  }
}

}  // namespace ai
