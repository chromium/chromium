// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVATE_AI_PRIVATE_AI_UTILS_H_
#define CHROME_BROWSER_PRIVATE_AI_PRIVATE_AI_UTILS_H_

#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/private_ai/proto/private_ai.pb.h"

namespace private_ai {

// Populates `paic_message` with `feature_name` and `execute_request`. If the
// `kPrivateExecuteRequest` feature is enabled, wraps the request in a
// `PrivateExecuteRequest` with variation IDs. Otherwise, sets it directly on
// `execute_request_ext`.
void PopulatePaicMessage(
    proto::FeatureName feature_name,
    const optimization_guide::proto::ExecuteRequest& execute_request,
    proto::PaicMessage* paic_message);

}  // namespace private_ai

#endif  // CHROME_BROWSER_PRIVATE_AI_PRIVATE_AI_UTILS_H_
