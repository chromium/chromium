// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_SIDE_PANEL_UI_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_SIDE_PANEL_UI_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_ui_embedder.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/ui/views/side_panel/glic/glic_side_panel_coordinator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace tabs {
class TabInterface;
}

namespace glic {

class GlicInstance;

// Implementation of GlicUiEmbedder for side panel UIs.
class GlicSidePanelUi : public GlicUiEmbedder,
                        public Host::Delegate,
                        public GlicSidePanelCoordinator::StateObserver {
 public:
  GlicSidePanelUi(Profile* profile,
                  base::WeakPtr<tabs::TabInterface> tab,
                  GlicInstance& instance);
  ~GlicSidePanelUi() override;

  // GlicUiEmbedder:
  Host::Delegate* GetHostDelegate() override;
  void Show() override;
  void Close() override;
  std::unique_ptr<views::View> CreateView() override;
  std::unique_ptr<GlicUiEmbedder> CreateInactiveEmbedder() const override;

  // Host::Delegate:
  const mojom::PanelState& GetPanelState() const override;
  void Resize(const gfx::Size& size,
              base::TimeDelta duration,
              base::OnceClosure callback) override;
  void SetDraggableAreas(
      const std::vector<gfx::Rect>& draggable_areas) override;
  void EnableDragResize(bool enabled) override;
  void Attach() override;
  void Detach() override;
  void SetMinimumWidgetSize(const gfx::Size& size) override;
  bool IsShowing() const override;

  // GlicSidePanelCoordinator::StateObserver
  void VisibilityChanged(bool visible) override;

 private:
  base::ScopedObservation<GlicSidePanelCoordinator,
                          GlicSidePanelCoordinator::StateObserver>
      coordinator_observation_{this};
  mojom::PanelState panel_state_;

  raw_ptr<Profile> profile_;
  base::WeakPtr<tabs::TabInterface> tab_;
  // Owns this.
  raw_ref<GlicInstance> instance_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_SIDE_PANEL_UI_H_
