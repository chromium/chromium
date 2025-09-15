// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_INACTIVE_SIDE_PANEL_UI_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_INACTIVE_SIDE_PANEL_UI_H_

#include "chrome/browser/glic/host/glic_ui_embedder.h"

namespace glic {

class GlicSidePanelUi;

// A GlicUiEmbedder for inactive Glic instances. This will show a
// blurred screenshot of the previously active UI.
class GlicInactiveSidePanelUi : public GlicUiEmbedder {
 public:
  static std::unique_ptr<GlicInactiveSidePanelUi> From(
      const GlicSidePanelUi& active_ui);

  ~GlicInactiveSidePanelUi() override;

  // GlicUiEmbedder:
  Host::Delegate* GetHostDelegate() override;
  void Show() override;
  void Close() override;
  std::unique_ptr<views::View> CreateView() override;
  std::unique_ptr<GlicUiEmbedder> CreateInactiveEmbedder() const override;

 private:
  GlicInactiveSidePanelUi();
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_INACTIVE_SIDE_PANEL_UI_H_
