// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/common/app_ui_observer.h"

#include <utility>

#include "base/functional/callback_forward.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/common/url_pattern_set.h"

namespace chromeos {

AppUiObserver::AppUiObserver(
    content::WebContents* contents,
    extensions::URLPatternSet pattern_set,
    base::OnceClosure on_app_ui_closed_callback,
    base::RepeatingCallback<void(bool)> on_app_ui_focus_change_callback)
    : content::WebContentsObserver(contents),
      pattern_set_(std::move(pattern_set)),
      on_app_ui_closed_callback_(std::move(on_app_ui_closed_callback)),
      on_app_ui_focus_change_callback_(
          std::move(on_app_ui_focus_change_callback)) {}

AppUiObserver::~AppUiObserver() = default;

void AppUiObserver::OnWebContentsFocused(
    content::RenderWidgetHost* render_widget_host) {
  if (on_app_ui_focus_change_callback_) {
    on_app_ui_focus_change_callback_.Run(true);
  }
}

void AppUiObserver::OnWebContentsLostFocus(
    content::RenderWidgetHost* render_widget_host) {
  if (on_app_ui_focus_change_callback_) {
    on_app_ui_focus_change_callback_.Run(false);
  }
}

void AppUiObserver::PrimaryPageChanged(content::Page& page) {
  if (pattern_set_.MatchesURL(web_contents()->GetLastCommittedURL())) {
    // Do nothing if the URL still matches.
    return;
  }

  // Results in the destruction of `this`, nothing should be called afterwards.
  std::move(on_app_ui_closed_callback_).Run();
}

void AppUiObserver::WebContentsDestroyed() {
  // Results in the destruction of `this`, nothing should be called afterwards.
  std::move(on_app_ui_closed_callback_).Run();
}

}  // namespace chromeos
