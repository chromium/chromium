// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_EXECUTION_ON_DEVICE_MODEL_UI_METADATA_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_EXECUTION_ON_DEVICE_MODEL_UI_METADATA_H_

#include <optional>
#include <string>

#include "components/optimization_guide/core/model_execution/on_device_model_names.h"

namespace optimization_guide {

// Localized UI metadata for an on-device base model shown on user-facing
// surfaces such as chrome://settings/ai/ondeviceai and download notifications.
struct OnDeviceModelUiMetadata {
  // High-level capability category used for grouping and metrics.
  OnDeviceModelCategory category;
  // Localized user-facing title of the capability (e.g. "General purpose").
  std::u16string category_title;
  // Localized description of what the capability does.
  std::u16string category_description;
  // User-facing display name of the underlying model (e.g. "Gemma 4").
  std::u16string model_display_name;
};

// Returns localized UI metadata for `base_model`, or std::nullopt if the model
// is hidden from user-facing surfaces until approved.
std::optional<OnDeviceModelUiMetadata> GetOnDeviceModelUiMetadata(
    OnDeviceBaseModel base_model);

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_EXECUTION_ON_DEVICE_MODEL_UI_METADATA_H_
