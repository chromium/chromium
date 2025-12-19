// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_SIDE_PANEL_UI_ANDROID_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_SIDE_PANEL_UI_ANDROID_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/service/glic_ui_embedder.h"

class Profile;

namespace tabs {
class TabInterface;
}

namespace glic {

class GlicInstanceMetrics;

class GlicSidePanelUi : public GlicUiEmbedder {
 public:
  GlicSidePanelUi(Profile* profile,
                  base::WeakPtr<tabs::TabInterface> tab,
                  GlicUiEmbedder::Delegate& delegate,
                  GlicInstanceMetrics& instance_metrics);
  ~GlicSidePanelUi() override;

  // GlicUiEmbedder:
  Host::EmbedderDelegate* GetHostEmbedderDelegate() override;
  void Show(const ShowOptions& options) override;
  bool IsShowing() const override;
  void Close() override;
  void Focus() override;
  bool HasFocus() override;
  std::unique_ptr<GlicUiEmbedder> CreateInactiveEmbedder() const override;
  mojom::PanelState GetPanelState() const override;
  gfx::Size GetPanelSize() override;
  std::string DescribeForTesting() override;

 private:
  base::WeakPtr<tabs::TabInterface> tab_;
  const raw_ref<GlicUiEmbedder::Delegate> delegate_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_SIDE_PANEL_UI_ANDROID_H_
