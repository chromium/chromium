// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_inactive_side_panel_ui.h"

#include "base/notimplemented.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace glic {

// static
std::unique_ptr<GlicInactiveSidePanelUi> GlicInactiveSidePanelUi::From(
    const GlicSidePanelUi& active_ui) {
  // Using `new` to access a private constructor.
  return base::WrapUnique(new GlicInactiveSidePanelUi());
}

GlicInactiveSidePanelUi::GlicInactiveSidePanelUi()
    : dummy_host_delegate_(std::make_unique<DummyHostDelegate>()) {}
GlicInactiveSidePanelUi::~GlicInactiveSidePanelUi() = default;

Host::Delegate* GlicInactiveSidePanelUi::GetHostDelegate() {
  return dummy_host_delegate_.get();
}

void GlicInactiveSidePanelUi::Show() {
  // TODO: implement show.
  NOTIMPLEMENTED();
}

void GlicInactiveSidePanelUi::Close() {
  // TODO: implement close.
  NOTIMPLEMENTED();
}

std::unique_ptr<views::View> GlicInactiveSidePanelUi::CreateView() {
  auto view = std::make_unique<views::View>();
  view->AddChildView(std::make_unique<views::Label>(u"Inactive"));
  return view;
}

std::unique_ptr<GlicUiEmbedder>
GlicInactiveSidePanelUi::CreateInactiveEmbedder() const {
  NOTREACHED() << "The embedder is already inactive.";
}

}  // namespace glic
