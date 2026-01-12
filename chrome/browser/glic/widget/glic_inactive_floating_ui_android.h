// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_INACTIVE_FLOATING_UI_ANDROID_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_INACTIVE_FLOATING_UI_ANDROID_H_

#include <memory>
#include <string>

#include "chrome/browser/glic/service/glic_ui_embedder.h"
#include "ui/gfx/geometry/size.h"

namespace glic {

class GlicFloatingUi;

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
  void Focus() override;
  bool HasFocus() override;
  std::unique_ptr<GlicUiEmbedder> CreateInactiveEmbedder() const override;
  mojom::PanelState GetPanelState() const override;
  gfx::Size GetPanelSize() override;
  std::string DescribeForTesting() override;

 private:
  GlicInactiveFloatingUi();
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_INACTIVE_FLOATING_UI_ANDROID_H_
