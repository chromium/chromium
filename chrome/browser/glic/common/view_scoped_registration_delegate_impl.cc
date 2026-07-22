// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of ViewScopedRegistrationDelegate for non-Android platforms
// using views::View.

#include "chrome/browser/glic/common/view_scoped_registration_delegate.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "ui/views/view.h"

namespace glic {

namespace {
class GlicPanelScopedHotkeyRegistration
    : public LocalHotkeyManager::ScopedHotkeyRegistration {
 public:
  GlicPanelScopedHotkeyRegistration(ui::Accelerator accelerator,
                                    base::WeakPtr<views::View> glic_view)
      : accelerator_(accelerator), glic_view_(glic_view) {
    CHECK(!accelerator.IsEmpty());
    CHECK(glic_view_);
    glic_view_->AddAccelerator(accelerator_);
  }

  ~GlicPanelScopedHotkeyRegistration() override {
    if (!glic_view_) {
      return;
    }
    glic_view_->RemoveAccelerator(accelerator_);
  }

 private:
  ui::Accelerator accelerator_;
  base::WeakPtr<views::View> glic_view_;
};
}  // namespace

ViewScopedRegistrationDelegate::ViewScopedRegistrationDelegate(
    base::WeakPtr<LocalHotkeyManager::Panel> panel)
    : panel_(panel) {}

ViewScopedRegistrationDelegate::~ViewScopedRegistrationDelegate() = default;

std::unique_ptr<LocalHotkeyManager::ScopedHotkeyRegistration>
ViewScopedRegistrationDelegate::CreateScopedHotkeyRegistration(
    ui::Accelerator accelerator,
    base::WeakPtr<ui::AcceleratorTarget> target) {
  CHECK(panel_);
  CHECK(panel_->GetView());
  return std::make_unique<GlicPanelScopedHotkeyRegistration>(accelerator,
                                                             panel_->GetView());
}

}  // namespace glic
