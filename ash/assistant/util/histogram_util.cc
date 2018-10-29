// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/util/histogram_util.h"

#include "ash/assistant/model/assistant_ui_model.h"
#include "base/metrics/histogram_macros.h"

namespace ash {
namespace assistant {
namespace util {

void IncrementAssistantQueryCountForEntryPoint(AssistantSource entry_point) {
  UMA_HISTOGRAM_ENUMERATION("Assistant.QueryCountPerEntryPoint", entry_point);
}

}  // namespace util
}  // namespace assistant
}  // namespace ash
