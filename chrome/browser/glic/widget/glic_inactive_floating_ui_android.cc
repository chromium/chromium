// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_inactive_floating_ui_android.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/glic/host/glic.mojom.h"

namespace glic {

// static
std::unique_ptr<GlicInactiveFloatingUi> GlicInactiveFloatingUi::From(
    const GlicFloatingUi& active_ui) {
  return base::WrapUnique(new GlicInactiveFloatingUi());
}

GlicInactiveFloatingUi::GlicInactiveFloatingUi() = default;

GlicInactiveFloatingUi::~GlicInactiveFloatingUi() = default;

Host::EmbedderDelegate* GlicInactiveFloatingUi::GetHostEmbedderDelegate() {
  return nullptr;
}

void GlicInactiveFloatingUi::Show(const ShowOptions& options) {}

bool GlicInactiveFloatingUi::IsShowing() const {
  return false;
}

void GlicInactiveFloatingUi::Close(const CloseOptions& options) {}

void GlicInactiveFloatingUi::Focus() {}

bool GlicInactiveFloatingUi::HasFocus() {
  return false;
}

std::unique_ptr<GlicUiEmbedder> GlicInactiveFloatingUi::CreateInactiveEmbedder()
    const {
  return nullptr;
}

mojom::PanelState GlicInactiveFloatingUi::GetPanelState() const {
  return mojom::PanelState();
}

gfx::Size GlicInactiveFloatingUi::GetPanelSize() {
  return gfx::Size();
}

std::string GlicInactiveFloatingUi::DescribeForTesting() {
  return "";
}

}  // namespace glic
