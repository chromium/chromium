// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/common/app_ui_observer.h"

#include <utility>

#include "base/functional/callback_forward.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/common/url_pattern_set.h"

namespace chromeos {

AppUiObserver::AppUiObserver(content::WebContents* contents,
                             extensions::URLPatternSet pattern_set,
                             base::OnceClosure on_app_ui_closed_callback)
    : content::WebContentsObserver(contents),
      pattern_set_(std::move(pattern_set)),
      on_app_ui_closed_callback_(std::move(on_app_ui_closed_callback)) {}

AppUiObserver::~AppUiObserver() = default;

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
