// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_NEVER_LOAD_COMPONENT_H_
#define CHROME_BROWSER_AI_AI_NEVER_LOAD_COMPONENT_H_

#include "chrome/browser/ai/ai_model_download_progress_manager.h"

namespace on_device_ai {

// Never loads the total bytes. This is useful for blocking the loading bar from
// reaching 100% before the API is ready to create an instance. The renderer
// itself will send 100% just before creating the API instance.
class AINeverLoadComponent : public AIModelDownloadProgressManager::Component {
 public:
  explicit AINeverLoadComponent(int64_t total_bytes);
  ~AINeverLoadComponent() override;
};
}  // namespace on_device_ai

#endif  // CHROME_BROWSER_AI_AI_NEVER_LOAD_COMPONENT_H_
