// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEIC_GEIC_VIEW_H_
#define CHROME_BROWSER_GEIC_GEIC_VIEW_H_

#include <array>

#include "chrome/browser/pwc/privileged_web_contents.h"
#include "content/public/common/page_zoom.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace input {
struct NativeWebKeyboardEvent;
}  // namespace input

namespace geic {

// View hosting the GEiC side panel WebContents. Handles zoom keyboard shortcuts
// and forwards unhandled keyboard events.
class GeicView : public views::WebView,
                 public pwc::PrivilegedWebContents::EmbedderDelegate {
  METADATA_HEADER(GeicView, views::WebView)

 public:
  static inline constexpr int kGeicWebViewId = 888;

  // Discrete zoom factors allowed in GEiC.
  // Starting at 1.0 (zero state / default), allows zooming in up to 5 times
  // (max 2.0). Zooming out below the zero state (1.0) is not allowed.
  static constexpr std::array<double, 6> kZoomFactors = {1.0, 1.1,  1.25,
                                                         1.5, 1.75, 2.0};

  explicit GeicView(Profile* profile);
  GeicView(const GeicView&) = delete;
  GeicView& operator=(const GeicView&) = delete;
  ~GeicView() override;

  // Adjusts the zoom level of the WebContents according to `kZoomFactors`.
  void Zoom(content::PageZoom zoom);

  // Returns the current zoom factor (e.g. 1.0, 1.1, 1.25).
  double GetZoomFactor() const;

  // views::WebView:
  void SetWebContents(content::WebContents* web_contents) override;

  // views::View:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

  // pwc::PrivilegedWebContents::EmbedderDelegate:
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  void ContentsZoomChange(bool zoom_in) override;

 private:
  bool ContainsOrHasFocus() const;

  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
};

}  // namespace geic

#endif  // CHROME_BROWSER_GEIC_GEIC_VIEW_H_
