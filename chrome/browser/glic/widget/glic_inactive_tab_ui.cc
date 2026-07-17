// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_inactive_tab_ui.h"

#include "components/tabs/public/tab_interface.h"
#include "ui/gfx/geometry/size.h"

namespace glic {

GlicInactiveTabUi::GlicInactiveTabUi(base::WeakPtr<tabs::TabInterface> tab,
                                     GlicUiEmbedder::Delegate& delegate)
    : tab_(tab), delegate_(delegate) {}

GlicInactiveTabUi::~GlicInactiveTabUi() = default;

Host::EmbedderDelegate* GlicInactiveTabUi::GetHostEmbedderDelegate() {
  return nullptr;
}

void GlicInactiveTabUi::Show(const ShowOptions& options) {}

bool GlicInactiveTabUi::IsShowing() const {
  return false;
}

bool GlicInactiveTabUi::IsShowingOrBackgrounded() const {
  return tab_ != nullptr;
}

void GlicInactiveTabUi::Close(const CloseOptions& options) {
  if (tab_) {
    tab_->Close();
  }
}

void GlicInactiveTabUi::Focus() {}

bool GlicInactiveTabUi::HasFocus() {
  return false;
}

#if !BUILDFLAG(IS_ANDROID)
base::WeakPtr<views::View> GlicInactiveTabUi::GetView() {
  return nullptr;
}
#endif

std::unique_ptr<GlicUiEmbedder> GlicInactiveTabUi::CreateInactiveEmbedder()
    const {
  return std::make_unique<GlicInactiveTabUi>(tab_, *delegate_);
}

mojom::PanelState GlicInactiveTabUi::GetPanelState() const {
  return mojom::PanelState(mojom::PanelStateKind::kAttached, std::nullopt);
}

gfx::Size GlicInactiveTabUi::GetPanelSize() {
  return gfx::Size();
}

std::string GlicInactiveTabUi::DescribeForTesting() {
  return "GlicInactiveTabUi";
}

}  // namespace glic
