// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/setters/default_browser_visual_guided_setter.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/types/to_address.h"
#include "build/build_config.h"
#include "chrome/browser/default_browser/default_browser_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "components/prefs/pref_service.h"
#include "net/base/url_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace default_browser {

DefaultBrowserVisualGuidedSetter::DefaultBrowserVisualGuidedSetter(
    Profile& profile)
    : profile_(profile) {}

DefaultBrowserVisualGuidedSetter::~DefaultBrowserVisualGuidedSetter() = default;

DefaultBrowserSetterType DefaultBrowserVisualGuidedSetter::GetType() const {
  return DefaultBrowserSetterType::kVisualGuide;
}

void DefaultBrowserVisualGuidedSetter::Execute(
    DefaultBrowserSetterCompletionCallback on_complete,
    const ExecuteParams& params) {
  GURL url = GetDefaultBrowserVisualGuideURL();

  if (params.can_pin_to_taskbar) {
    url = net::AppendQueryParameter(url, "can_pin_to_taskbar", "true");
  }

  NavigateParams navigation_params(base::to_address(profile_), url,
                                   ui::PAGE_TRANSITION_LINK);
  navigation_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  navigation_params.window_action = NavigateParams::WindowAction::kShowWindow;
  Navigate(&navigation_params);

  std::move(on_complete).Run(DefaultBrowserState::UNKNOWN_DEFAULT);
}

}  // namespace default_browser
