// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_INACTIVE_SIDE_PANEL_UI_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_INACTIVE_SIDE_PANEL_UI_H_

#include "base/notimplemented.h"
#include "chrome/browser/glic/host/glic_ui_embedder.h"

namespace glic {

// A GlicUiEmbedder for inactive Glic instances. This will show a
// blurred screenshot of the previously active UI.
class GlicInactiveSidePanelUi : public GlicUiEmbedder {
 public:
  GlicInactiveSidePanelUi() = default;
  ~GlicInactiveSidePanelUi() override = default;

  // GlicUiEmbedder:
  Host::Delegate* GetHostDelegate() override { return nullptr; }
  void Show() override { NOTIMPLEMENTED(); }
  std::unique_ptr<views::View> CreateView() override {
    NOTIMPLEMENTED();
    return nullptr;
  }
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_INACTIVE_SIDE_PANEL_UI_H_
