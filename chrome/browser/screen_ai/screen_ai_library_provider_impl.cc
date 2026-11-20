// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_library_provider_impl.h"

#include "chrome/browser/screen_ai/screen_ai_install_state.h"

namespace screen_ai {

ScreenAILibraryProviderImpl::ScreenAILibraryProviderImpl() = default;
ScreenAILibraryProviderImpl::~ScreenAILibraryProviderImpl() = default;

base::FilePath ScreenAILibraryProviderImpl::GetBinaryPath() const {
  return ScreenAIInstallState::GetInstance()->get_component_binary_path();
}

void ScreenAILibraryProviderImpl::SetLastUsageTime() {
  ScreenAIInstallState::GetInstance()->SetLastUsageTime();
}

void ScreenAILibraryProviderImpl::Download() {
  ScreenAIInstallState::GetInstance()->DownloadComponent();
}

}  // namespace screen_ai
