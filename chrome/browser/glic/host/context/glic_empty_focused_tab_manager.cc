// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_empty_focused_tab_manager.h"

namespace glic {

GlicEmptyFocusedTabManager::GlicEmptyFocusedTabManager() = default;

GlicEmptyFocusedTabManager::~GlicEmptyFocusedTabManager() = default;

base::CallbackListSubscription
GlicEmptyFocusedTabManager::AddFocusedTabChangedCallback(
    FocusedTabChangedCallback callback) {
  return {};
}

base::CallbackListSubscription
GlicEmptyFocusedTabManager::AddFocusedTabDataChangedCallback(
    FocusedTabDataChangedCallback callback) {
  return {};
}

bool GlicEmptyFocusedTabManager::IsTabFocused(
    tabs::TabHandle tab_handle) const {
  return false;
}

FocusedTabData GlicEmptyFocusedTabManager::GetFocusedTabData() {
  return FocusedTabData(std::string("no focusable tab"), nullptr);
}

}  // namespace glic
