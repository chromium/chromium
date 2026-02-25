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
  void Close(const CloseOptions& options) override;
  std::unique_ptr<GlicUiEmbedder> CreateInactiveEmbedder() const override;
  void Focus() override;
  bool HasFocus() override;
#if !BUILDFLAG(IS_ANDROID)
  base::WeakPtr<views::View> GetView() override;
#endif
  mojom::PanelState GetPanelState() const override;
  gfx::Size GetPanelSize() override;
  std::string DescribeForTesting() override;

 private:
  std::unique_ptr<views::View> CreateView();
  GlicInactiveFloatingUi();
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_INACTIVE_FLOATING_UI_H_
