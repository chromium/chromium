// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/ui_default_factory.h"

#include <utility>

#include "chrome/browser/vr/ui.h"

namespace vr {

UiDefaultFactory::~UiDefaultFactory() {}

std::unique_ptr<UiInterface> UiDefaultFactory::Create(
    UiBrowserInterface* browser,
    const UiInitialState& ui_initial_state) {
  return std::make_unique<Ui>(browser, ui_initial_state);
}

std::unique_ptr<UiFactory> CreateUiFactory() {
  return std::make_unique<UiDefaultFactory>();
}

}  // namespace vr
