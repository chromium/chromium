// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_INACTIVE_SIDE_PANEL_UI_ANDROID_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_INACTIVE_SIDE_PANEL_UI_ANDROID_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/service/glic_ui_embedder.h"

namespace tabs {
class TabInterface;
}

namespace glic {

class GlicInactiveSidePanelUi : public GlicUiEmbedder {
 public:
  static std::unique_ptr<GlicInactiveSidePanelUi> CreateForVisibleTab(
      base::WeakPtr<tabs::TabInterface> tab,
      GlicUiEmbedder::Delegate& delegate);
  static std::unique_ptr<GlicInactiveSidePanelUi> CreateForBackgroundTab(
      base::WeakPtr<tabs::TabInterface> tab,
      GlicUiEmbedder::Delegate& delegate);

  GlicInactiveSidePanelUi(base::WeakPtr<tabs::TabInterface> tab,
                          GlicUiEmbedder::Delegate& delegate);
  ~GlicInactiveSidePanelUi() override;

  // GlicUiEmbedder:
  Host::EmbedderDelegate* GetHostEmbedderDelegate() override;
  void Show(const ShowOptions& options) override;
  bool IsShowing() const override;
  void Close(const CloseOptions& options) override;
  void Focus() override;
  bool HasFocus() override;
  std::unique_ptr<GlicUiEmbedder> CreateInactiveEmbedder() const override;
  mojom::PanelState GetPanelState() const override;
  gfx::Size GetPanelSize() override;
  std::string DescribeForTesting() override;

 private:
  GlicSidePanelCoordinator* GetGlicSidePanelCoordinator() const;

  base::WeakPtr<tabs::TabInterface> tab_;
  const raw_ref<GlicUiEmbedder::Delegate> delegate_;

  base::WeakPtrFactory<GlicInactiveSidePanelUi> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_INACTIVE_SIDE_PANEL_UI_ANDROID_H_
