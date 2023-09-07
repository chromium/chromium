// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_COMMON_APP_UI_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_COMMON_APP_UI_OBSERVER_H_

#include "base/functional/callback_forward.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/common/url_pattern_set.h"

namespace chromeos {

// Tracks the status of a WebContents of an app UI.
// * `contents`: the WebContents to track.
// * `pattern_set`: for matching the app UI.
// * `on_app_ui_closed_callback`: Will be called when the app UI is closed. The
//    callback is responsible to delete the observer.
// * `on_app_ui_focus_change_callback`: Will be called with a boolean to
//    indicate if the App UI is focused or not.
class AppUiObserver : public content::WebContentsObserver {
 public:
  AppUiObserver(
      content::WebContents* contents,
      extensions::URLPatternSet pattern_set,
      base::OnceClosure on_app_ui_closed_callback,
      base::RepeatingCallback<void(bool)> on_app_ui_focus_change_callback);
  AppUiObserver(AppUiObserver&) = delete;
  AppUiObserver& operator=(AppUiObserver&) = delete;
  ~AppUiObserver() override;

  // content::WebContentsObserver overrides:
  void OnWebContentsFocused(
      content::RenderWidgetHost* render_widget_host) override;
  void OnWebContentsLostFocus(
      content::RenderWidgetHost* render_widget_host) override;
  void PrimaryPageChanged(content::Page& page) override;
  void WebContentsDestroyed() override;

 private:
  extensions::URLPatternSet pattern_set_;
  base::OnceClosure on_app_ui_closed_callback_;
  base::RepeatingCallback<void(bool)> on_app_ui_focus_change_callback_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_COMMON_APP_UI_OBSERVER_H_
