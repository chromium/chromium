// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/common/panel_focus_dependent_hotkey_manager.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/common/chrome_features.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/widget/glic_view.h"
#endif

namespace glic {

namespace {

static constexpr std::array kSupportedCommands = {
    glic::LocalHotkeyManager::Command::kClose,
    glic::LocalHotkeyManager::Command::kFocusToggle,
    glic::LocalHotkeyManager::Command::kZoomIn,
    glic::LocalHotkeyManager::Command::kZoomOut,
    glic::LocalHotkeyManager::Command::kZoomReset,
#if BUILDFLAG(IS_WIN)
    glic::LocalHotkeyManager::Command::kTitleBarContextMenu,
#endif
};

#if !BUILDFLAG(IS_ANDROID)
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
#endif

}  // namespace

ViewScopedRegistrationDelegate::ViewScopedRegistrationDelegate(
    base::WeakPtr<LocalHotkeyManager::Panel> panel)
    : panel_(panel) {}

ViewScopedRegistrationDelegate::~ViewScopedRegistrationDelegate() = default;

#if !BUILDFLAG(IS_ANDROID)
std::unique_ptr<LocalHotkeyManager::ScopedHotkeyRegistration>
ViewScopedRegistrationDelegate::CreateScopedHotkeyRegistration(
    ui::Accelerator accelerator,
    base::WeakPtr<ui::AcceleratorTarget> target) {
  CHECK(panel_);
  CHECK(panel_->GetView());
  return std::make_unique<GlicPanelScopedHotkeyRegistration>(accelerator,
                                                             panel_->GetView());
}
#endif

PanelFocusDependentHotkeyManager::PanelFocusDependentHotkeyManager(
    base::WeakPtr<LocalHotkeyManager::Panel> panel)
    : panel_(panel) {
  hotkey_manager_ = std::make_unique<LocalHotkeyManager>(
      std::make_unique<ViewScopedRegistrationDelegate>(panel), this,
      kSupportedCommands);
}

PanelFocusDependentHotkeyManager::~PanelFocusDependentHotkeyManager() = default;

bool PanelFocusDependentHotkeyManager::AcceleratorPressed(
    LocalHotkeyManager::Command command) {
  if (!panel_ || !panel_->HasFocus()) {
    return false;
  }

  switch (command) {
    case LocalHotkeyManager::Command::kClose: {
#if !BUILDFLAG(IS_ANDROID)
      if (panel_->HasSelectionOverlay()) {
        panel_->CloseSelectionOverlay();
        return true;
      }
#endif
      panel_->Close(CloseOptions());
      return true;
    }
    case glic::LocalHotkeyManager::Command::kFocusToggle:
      if (panel_->ActivateBrowser()) {
        base::RecordAction(base::UserMetricsAction("Glic.FocusHotKey"));
        return true;
      }
      return false;
    case LocalHotkeyManager::Command::kZoomIn:
      if (!base::FeatureList::IsEnabled(features::kGlicClientZoomControl)) {
        return false;
      }
      panel_->Zoom(mojom::ZoomAction::kZoomIn);
      return true;
    case LocalHotkeyManager::Command::kZoomOut:
      if (!base::FeatureList::IsEnabled(features::kGlicClientZoomControl)) {
        return false;
      }
      panel_->Zoom(mojom::ZoomAction::kZoomOut);
      return true;
    case LocalHotkeyManager::Command::kZoomReset:
      if (!base::FeatureList::IsEnabled(features::kGlicClientZoomControl)) {
        return false;
      }
      panel_->Zoom(mojom::ZoomAction::kReset);
      return true;
#if BUILDFLAG(IS_WIN)
    case LocalHotkeyManager::Command::kTitleBarContextMenu:
      panel_->ShowTitleBarContextMenuAt(gfx::Point());
      return true;
#endif  //  BUILDFLAG(IS_WIN)

    default:
      NOTREACHED() << "no handling implemented for "
                   << LocalHotkeyManager::CommandToString(command);
  }
}

bool PanelFocusDependentHotkeyManager::CanHandleAccelerators() const {
  return panel_ && panel_->HasFocus();
}

void PanelFocusDependentHotkeyManager::InitializeAccelerators() {
  hotkey_manager_->InitializeAccelerators();
}

}  // namespace glic
