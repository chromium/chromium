// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_FLOATING_UI_ANDROID_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_FLOATING_UI_ANDROID_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/glic/service/glic_ui_embedder.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/gfx/geometry/rect.h"

class BrowserWindowInterface;
class Profile;

namespace glic {

class GlicInstanceMetrics;

class GlicFloatingUi : public GlicUiEmbedder {
 public:
  GlicFloatingUi(Profile* profile,
                 BrowserWindowInterface* browser,
                 GlicUiEmbedder::Delegate& delegate,
                 GlicInstanceMetrics& instance_metrics);
  GlicFloatingUi(Profile* profile,
                 gfx::Rect initial_bounds,
                 tabs::TabHandle source_tab,
                 GlicUiEmbedder::Delegate& delegate,
                 GlicInstanceMetrics& instance_metrics);
  ~GlicFloatingUi() override;

  static gfx::Size GetDefaultSize();
  static gfx::Size GetCompositeViewDefaultSize();

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
  const raw_ref<GlicUiEmbedder::Delegate> delegate_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_FLOATING_UI_ANDROID_H_
