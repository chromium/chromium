// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_INACTIVE_SIDE_PANEL_UI_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_INACTIVE_SIDE_PANEL_UI_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/glic/service/glic_ui_embedder.h"
#include "chrome/browser/glic/widget/inactive_view_controller.h"
#include "chrome/browser/ui/views/side_panel/glic/glic_side_panel_coordinator.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view_observer.h"

namespace views {
class View;
}  // namespace views

namespace glic {

// A GlicUiEmbedder for inactive Glic instances. This will show a
// blurred screenshot of the previously active UI.
class GlicInactiveSidePanelUi : public GlicUiEmbedder,
                                public views::ViewObserver {
 public:
  static std::unique_ptr<GlicInactiveSidePanelUi> CreateForVisibleTab(
      base::WeakPtr<tabs::TabInterface> tab,
      GlicUiEmbedder::Delegate& delegate);
  static std::unique_ptr<GlicInactiveSidePanelUi> CreateForBackgroundTab(
      base::WeakPtr<tabs::TabInterface> tab,
      GlicUiEmbedder::Delegate& delegate);

  ~GlicInactiveSidePanelUi() override;

  // GlicUiEmbedder:
  Host::EmbedderDelegate* GetHostEmbedderDelegate() override;
  void Show(const ShowOptions& options) override;
  bool IsShowing() const override;
  void Close() override;
  std::unique_ptr<GlicUiEmbedder> CreateInactiveEmbedder() const override;
  void Focus() override;
  bool HasFocus() override;
  std::string DescribeForTesting() override;

  base::WeakPtr<views::View> GetView() override;
  mojom::PanelState GetPanelState() const override;
  gfx::Size GetPanelSize() override;

  // views::ViewObserver:
  void OnViewFocused(views::View* observed_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;

 private:
  explicit GlicInactiveSidePanelUi(base::WeakPtr<tabs::TabInterface> tab,
                                   GlicUiEmbedder::Delegate& delegate);
  GlicSidePanelCoordinator* GetGlicSidePanelCoordinator() const;

  InactiveViewController inactive_view_controller_;
  base::WeakPtr<tabs::TabInterface> tab_;
  raw_ref<GlicUiEmbedder::Delegate> delegate_;

  base::ScopedObservation<views::View, views::ViewObserver>
      scoped_view_observation_{this};

  base::WeakPtrFactory<GlicInactiveSidePanelUi> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_INACTIVE_SIDE_PANEL_UI_H_
