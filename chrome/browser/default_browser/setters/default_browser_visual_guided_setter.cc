// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/setters/default_browser_visual_guided_setter.h"

#include "base/notreached.h"
#include "base/types/to_address.h"
#include "chrome/browser/default_browser/default_browser_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "components/prefs/pref_service.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

namespace default_browser {

DefaultBrowserVisualGuidedSetter::DefaultBrowserVisualGuidedSetter(
    Profile& profile)
    : profile_(profile) {}

DefaultBrowserVisualGuidedSetter::~DefaultBrowserVisualGuidedSetter() = default;

DefaultBrowserSetterType DefaultBrowserVisualGuidedSetter::GetType() const {
  return DefaultBrowserSetterType::kVisualGuide;
}

void DefaultBrowserVisualGuidedSetter::Execute(
    DefaultBrowserSetterCompletionCallback on_complete) {
  NavigateParams params(base::to_address(profile_),
                        GetDefaultBrowserVisualGuideURL(),
                        ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.window_action = NavigateParams::WindowAction::kShowWindow;
  Navigate(&params);
  std::move(on_complete).Run(DefaultBrowserState::UNKNOWN_DEFAULT);
}

}  // namespace default_browser
