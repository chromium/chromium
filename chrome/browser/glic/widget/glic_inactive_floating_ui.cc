// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_inactive_floating_ui.h"

#include "base/notimplemented.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace glic {

// static
std::unique_ptr<GlicInactiveFloatingUi> GlicInactiveFloatingUi::From(
    const GlicFloatingUi& active_ui) {
  // Using `new` to access a private constructor.
  return base::WrapUnique(new GlicInactiveFloatingUi());
}

GlicInactiveFloatingUi::GlicInactiveFloatingUi() = default;
GlicInactiveFloatingUi::~GlicInactiveFloatingUi() = default;

std::unique_ptr<views::View> GlicInactiveFloatingUi::CreateView() {
  // TODO: implement CreateView. This should set up the contents for the
  // floating UI and be called from the constructor.
  NOTIMPLEMENTED();
  return std::make_unique<views::View>();
}

Host::EmbedderDelegate* GlicInactiveFloatingUi::GetHostEmbedderDelegate() {
  return nullptr;
}

void GlicInactiveFloatingUi::Show(const ShowOptions& options) {
  // TODO: implement show.
  NOTIMPLEMENTED();
}

bool GlicInactiveFloatingUi::IsShowing() const {
  NOTIMPLEMENTED();
  return false;
}

void GlicInactiveFloatingUi::Close() {
  // TODO: implement close.
  NOTIMPLEMENTED();
}

base::WeakPtr<views::View> GlicInactiveFloatingUi::GetView() {
  return nullptr;
}

gfx::Size GlicInactiveFloatingUi::GetPanelSize() {
  return gfx::Size();
}

mojom::PanelState GlicInactiveFloatingUi::GetPanelState() const {
  mojom::PanelState state;
  state.kind = glic::mojom::PanelStateKind::kHidden;
  return state;
}

std::unique_ptr<GlicUiEmbedder> GlicInactiveFloatingUi::CreateInactiveEmbedder()
    const {
  NOTREACHED() << "The embedder is already inactive.";
}

void GlicInactiveFloatingUi::Focus() {
  NOTIMPLEMENTED();
}

bool GlicInactiveFloatingUi::HasFocus() {
  NOTIMPLEMENTED();
  return false;
}

std::string GlicInactiveFloatingUi::DescribeForTesting() {
  return "InactiveFloatingUi";
}

}  // namespace glic
