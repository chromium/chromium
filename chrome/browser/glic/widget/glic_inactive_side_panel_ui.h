// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_INACTIVE_SIDE_PANEL_UI_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_INACTIVE_SIDE_PANEL_UI_H_

#include "base/scoped_observation.h"
#include "chrome/browser/glic/service/glic_ui_embedder.h"
#include "chrome/browser/ui/views/side_panel/glic/glic_side_panel_coordinator.h"

namespace glic {

class GlicSidePanelUi;

// A GlicUiEmbedder for inactive Glic instances. This will show a
// blurred screenshot of the previously active UI.
class GlicInactiveSidePanelUi : public GlicUiEmbedder,
                                public GlicSidePanelCoordinator::StateObserver {
 public:
  static std::unique_ptr<GlicInactiveSidePanelUi> From(
      const GlicSidePanelUi& active_ui,
      base::WeakPtr<tabs::TabInterface> tab);

  ~GlicInactiveSidePanelUi() override;

  // GlicUiEmbedder:
  Host::EmbedderDelegate* GetHostEmbedderDelegate() override;
  void Show() override;
  bool IsShowing() const override;
  void Close() override;
  std::unique_ptr<GlicUiEmbedder> CreateInactiveEmbedder() const override;

  // GlicSidePanelCoordinator::StateObserver:
  void VisibilityChanged(bool visible) override;

 private:
  explicit GlicInactiveSidePanelUi(base::WeakPtr<tabs::TabInterface> tab);
  std::unique_ptr<views::View> CreateView(
      base::WeakPtr<tabs::TabInterface> tab);

  base::ScopedObservation<GlicSidePanelCoordinator,
                          GlicSidePanelCoordinator::StateObserver>
      coordinator_observation_{this};
  base::WeakPtr<tabs::TabInterface> tab_;
  bool is_showing_ = false;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_INACTIVE_SIDE_PANEL_UI_H_
