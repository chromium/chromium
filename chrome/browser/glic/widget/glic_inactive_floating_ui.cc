// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_inactive_floating_ui.h"

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

Host::Delegate* GlicInactiveFloatingUi::GetHostDelegate() {
  return nullptr;
}

void GlicInactiveFloatingUi::Show() {
  // TODO: implement show.
}

std::unique_ptr<views::View> GlicInactiveFloatingUi::CreateView() {
  auto view = std::make_unique<views::View>();
  view->AddChildView(std::make_unique<views::Label>(u"Inactive"));
  return view;
}

std::unique_ptr<GlicUiEmbedder> GlicInactiveFloatingUi::CreateInactiveEmbedder()
    const {
  NOTREACHED() << "The embedder is already inactive.";
}

}  // namespace glic
