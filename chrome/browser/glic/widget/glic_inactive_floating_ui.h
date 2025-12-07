// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_INACTIVE_FLOATING_UI_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_INACTIVE_FLOATING_UI_H_

#include "chrome/browser/glic/service/glic_ui_embedder.h"

namespace glic {

class GlicFloatingUi;

// A GlicUiEmbedder for inactive Glic instances. This will show a
// blurred screenshot of the previously active UI.
class GlicInactiveFloatingUi : public GlicUiEmbedder {
 public:
  static std::unique_ptr<GlicInactiveFloatingUi> From(
      const GlicFloatingUi& active_ui);

  ~GlicInactiveFloatingUi() override;

  // GlicUiEmbedder:
  Host::EmbedderDelegate* GetHostEmbedderDelegate() override;
  void Show(const ShowOptions& options) override;
  bool IsShowing() const override;
  void Close() override;
  std::unique_ptr<GlicUiEmbedder> CreateInactiveEmbedder() const override;
  void Focus() override;
  bool HasFocus() override;
  base::WeakPtr<views::View> GetView() override;
  mojom::PanelState GetPanelState() const override;
  gfx::Size GetPanelSize() override;
  std::string DescribeForTesting() override;

 private:
  std::unique_ptr<views::View> CreateView();
  GlicInactiveFloatingUi();
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_INACTIVE_FLOATING_UI_H_
