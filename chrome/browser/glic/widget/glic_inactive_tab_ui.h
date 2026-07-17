// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_INACTIVE_TAB_UI_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_INACTIVE_TAB_UI_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/service/glic_ui_embedder.h"

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace glic {

// An embedder used to represent a Glic tab that is currently inactive or
// backgrounded.
// It provides:
// - Correct responses to `IsShowing()` (false) and `IsShowingOrBackgrounded()`
//   (true as long as the tab is valid).
// - Handling for `Close()` by closing the underlying tab.
class GlicInactiveTabUi : public GlicUiEmbedder {
 public:
  GlicInactiveTabUi(base::WeakPtr<tabs::TabInterface> tab,
                    GlicUiEmbedder::Delegate& delegate);
  ~GlicInactiveTabUi() override;

  // GlicUiEmbedder:
  Host::EmbedderDelegate* GetHostEmbedderDelegate() override;
  void Show(const ShowOptions& options) override;
  bool IsShowing() const override;
  bool IsShowingOrBackgrounded() const override;
  void Close(const CloseOptions& options) override;
  void Focus() override;
  bool HasFocus() override;
#if !BUILDFLAG(IS_ANDROID)
  base::WeakPtr<views::View> GetView() override;
#endif
  std::unique_ptr<GlicUiEmbedder> CreateInactiveEmbedder() const override;
  mojom::PanelState GetPanelState() const override;
  gfx::Size GetPanelSize() override;
  std::string DescribeForTesting() override;

 private:
  base::WeakPtr<tabs::TabInterface> tab_;
  raw_ref<GlicUiEmbedder::Delegate> delegate_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_INACTIVE_TAB_UI_H_
