// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SCREEN_AI_SCREEN_AI_LIBRARY_PROVIDER_IMPL_H_
#define CHROME_BROWSER_SCREEN_AI_SCREEN_AI_LIBRARY_PROVIDER_IMPL_H_

#include "chrome/browser/screen_ai/screen_ai_library_provider.h"

namespace screen_ai {

// Provides the Screen AI library that `ScreenAIInstallState` keeps track of.
class ScreenAILibraryProviderImpl : public ScreenAILibraryProvider {
 public:
  ScreenAILibraryProviderImpl();
  ScreenAILibraryProviderImpl(const ScreenAILibraryProviderImpl&) = delete;
  ScreenAILibraryProviderImpl& operator=(const ScreenAILibraryProviderImpl&) =
      delete;
  ~ScreenAILibraryProviderImpl() override;

  // ScreenAILibraryProvider:
  base::FilePath GetBinaryPath() const override;
  void SetLastUsageTime() override;
  void Download() override;
};

}  // namespace screen_ai

#endif  // CHROME_BROWSER_SCREEN_AI_SCREEN_AI_LIBRARY_PROVIDER_IMPL_H_
