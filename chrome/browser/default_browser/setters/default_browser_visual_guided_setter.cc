// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/setters/default_browser_visual_guided_setter.h"

#include "base/notreached.h"

namespace default_browser {

DefaultBrowserVisualGuidedSetter::DefaultBrowserVisualGuidedSetter() = default;

DefaultBrowserVisualGuidedSetter::~DefaultBrowserVisualGuidedSetter() = default;

DefaultBrowserSetterType DefaultBrowserVisualGuidedSetter::GetType() const {
  return DefaultBrowserSetterType::kVisualGuide;
}

void DefaultBrowserVisualGuidedSetter::Execute(
    DefaultBrowserSetterCompletionCallback on_complete) {
  std::move(on_complete).Run(DefaultBrowserState::UNKNOWN_DEFAULT);
}

}  // namespace default_browser
