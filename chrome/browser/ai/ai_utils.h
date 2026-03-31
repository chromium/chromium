// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_UTILS_H_
#define CHROME_BROWSER_AI_AI_UTILS_H_

#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace ai {

// Convert ResponseConstraint proto to mojom.
on_device_model::mojom::ResponseConstraintPtr ToMojomResponseConstraint(
    const optimization_guide::proto::ResponseConstraint& constraint);

}  // namespace ai

#endif  // CHROME_BROWSER_AI_AI_UTILS_H_
