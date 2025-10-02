// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_INACTIVE_SIDE_PANEL_UI_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_INACTIVE_SIDE_PANEL_UI_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/service/glic_ui_embedder.h"
#include "chrome/browser/glic/widget/blurred_screenshot_view_controller.h"
#include "chrome/browser/ui/views/side_panel/glic/glic_side_panel_coordinator.h"
#include "ui/gfx/image/image_skia.h"

namespace content {
class WebContents;
}

namespace glic {

// A GlicUiEmbedder for inactive Glic instances. This will show a
// blurred screenshot of the previously active UI.
class GlicInactiveSidePanelUi : public GlicUiEmbedder {
 public:
  static std::unique_ptr<GlicInactiveSidePanelUi> CreateForVisibleTab(
      base::WeakPtr<tabs::TabInterface> tab,
      content::WebContents* glic_webui_contents);
  static std::unique_ptr<GlicInactiveSidePanelUi> CreateForBackgroundTab(
      base::WeakPtr<tabs::TabInterface> tab);

  ~GlicInactiveSidePanelUi() override;

  // GlicUiEmbedder:
  Host::EmbedderDelegate* GetHostEmbedderDelegate() override;
  void Show() override;
  bool IsShowing() const override;
  void Close() override;
  std::unique_ptr<GlicUiEmbedder> CreateInactiveEmbedder() const override;

  void VisibilityChanged(bool visible);

 private:
  explicit GlicInactiveSidePanelUi(base::WeakPtr<tabs::TabInterface> tab);

  std::unique_ptr<views::View> CreateView(
      base::WeakPtr<tabs::TabInterface> tab);

  BlurredScreenshotViewController blurred_screenshot_view_controller_;
  base::WeakPtr<tabs::TabInterface> tab_;
  bool is_showing_ = false;

  base::CallbackListSubscription panel_visibility_subscription_;
  base::WeakPtrFactory<GlicInactiveSidePanelUi> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_INACTIVE_SIDE_PANEL_UI_H_
