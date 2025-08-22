// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_never_load_component.h"

namespace on_device_ai {

AINeverLoadComponent::AINeverLoadComponent(int64_t total_bytes) {
  SetDownloadedBytes(0);
  SetTotalBytes(total_bytes);
}
AINeverLoadComponent::~AINeverLoadComponent() = default;

}  // namespace on_device_ai
