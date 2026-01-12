// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_floating_ui_android.h"

#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/widget/glic_inactive_floating_ui_android.h"

namespace glic {

GlicFloatingUi::GlicFloatingUi(Profile* profile,
                               BrowserWindowInterface* browser,
                               GlicUiEmbedder::Delegate& delegate,
                               GlicInstanceMetrics& instance_metrics)
    : delegate_(delegate) {}

GlicFloatingUi::GlicFloatingUi(Profile* profile,
                               gfx::Rect initial_bounds,
                               tabs::TabHandle source_tab,
                               GlicUiEmbedder::Delegate& delegate,
                               GlicInstanceMetrics& instance_metrics)
    : delegate_(delegate) {}

GlicFloatingUi::~GlicFloatingUi() = default;

// static
gfx::Size GlicFloatingUi::GetDefaultSize() {
  return gfx::Size();
}

// static
gfx::Size GlicFloatingUi::GetCompositeViewDefaultSize() {
  return gfx::Size();
}

Host::EmbedderDelegate* GlicFloatingUi::GetHostEmbedderDelegate() {
  return nullptr;
}

void GlicFloatingUi::Show(const ShowOptions& options) {}

bool GlicFloatingUi::IsShowing() const {
  return false;
}

void GlicFloatingUi::Close(const CloseOptions& options) {}

void GlicFloatingUi::Focus() {}

bool GlicFloatingUi::HasFocus() {
  return false;
}

std::unique_ptr<GlicUiEmbedder> GlicFloatingUi::CreateInactiveEmbedder() const {
  return GlicInactiveFloatingUi::From(*this);
}

mojom::PanelState GlicFloatingUi::GetPanelState() const {
  return mojom::PanelState();
}

gfx::Size GlicFloatingUi::GetPanelSize() {
  return gfx::Size();
}

std::string GlicFloatingUi::DescribeForTesting() {
  return "";
}

}  // namespace glic
