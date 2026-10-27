// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_web_contents_manager.h"

namespace glic {

bool GlicWebContentsManager::IsShowingNativeLoading() const {
  return false;
}

base::CallbackListSubscription
GlicWebContentsManager::RegisterNativeLoadingChangedCallback(
    NativeLoadingChangedCallback callback) {
  return {};
}

void GlicWebContentsManager::OnHostNativeLoadingChanged(bool is_showing) {}

}  // namespace glic
