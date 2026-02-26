// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/common/glic_panel_hotkey_delegate.h"

#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/base/accelerators/command.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/widget/glic_view.h"
#else
#include "ui/android/window_android.h"
#endif

namespace glic {

namespace {

static constexpr std::array kSupportedHotkeys = {
    glic::LocalHotkeyManager::Hotkey::kClose,
    glic::LocalHotkeyManager::Hotkey::kFocusToggle,
    glic::LocalHotkeyManager::Hotkey::kZoomIn,
    glic::LocalHotkeyManager::Hotkey::kZoomOut,
    glic::LocalHotkeyManager::Hotkey::kZoomReset,
#if BUILDFLAG(IS_WIN)
    glic::LocalHotkeyManager::Hotkey::kTitleBarContextMenu,
#endif
};

#if !BUILDFLAG(IS_ANDROID)
// Implementation of ScopedHotkeyRegistration specifically for the Glic panel.
// It registers and unregisters accelerators directly with the GlicView.
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

GlicPanelHotkeyDelegate::GlicPanelHotkeyDelegate(
    base::WeakPtr<LocalHotkeyManager::Panel> panel)
    : panel_(panel) {}

GlicPanelHotkeyDelegate::~GlicPanelHotkeyDelegate() = default;

const base::span<const LocalHotkeyManager::Hotkey>
GlicPanelHotkeyDelegate::GetSupportedHotkeys() const {
  return kSupportedHotkeys;
}

bool GlicPanelHotkeyDelegate::AcceleratorPressed(
    LocalHotkeyManager::Hotkey hotkey) {
  if (!panel_ || !panel_->HasFocus()) {
    return false;
  }

  switch (hotkey) {
    case LocalHotkeyManager::Hotkey::kClose:
      panel_->Close(CloseOptions());
      return true;
    case glic::LocalHotkeyManager::Hotkey::kFocusToggle:
      if (panel_->ActivateBrowser()) {
        base::RecordAction(base::UserMetricsAction("Glic.FocusHotKey"));
        return true;
      }
      return false;
    case LocalHotkeyManager::Hotkey::kZoomIn:
      if (!base::FeatureList::IsEnabled(features::kGlicClientZoomControl)) {
        return false;
      }
      panel_->Zoom(mojom::ZoomAction::kZoomIn);
      return true;
    case LocalHotkeyManager::Hotkey::kZoomOut:
      if (!base::FeatureList::IsEnabled(features::kGlicClientZoomControl)) {
        return false;
      }
      panel_->Zoom(mojom::ZoomAction::kZoomOut);
      return true;
    case LocalHotkeyManager::Hotkey::kZoomReset:
      if (!base::FeatureList::IsEnabled(features::kGlicClientZoomControl)) {
        return false;
      }
      panel_->Zoom(mojom::ZoomAction::kReset);
      return true;
#if BUILDFLAG(IS_WIN)
    case LocalHotkeyManager::Hotkey::kTitleBarContextMenu:
      panel_->ShowTitleBarContextMenuAt(gfx::Point());
      return true;
#endif  //  BUILDFLAG(IS_WIN)

    default:
      NOTREACHED() << "no handling implemented for "
                   << LocalHotkeyManager::HotkeyToString(hotkey);
  }
}

#if !BUILDFLAG(IS_ANDROID)
// Not supported on Android. Local hotkeys are handled in Java.
std::unique_ptr<LocalHotkeyManager::ScopedHotkeyRegistration>
GlicPanelHotkeyDelegate::CreateScopedHotkeyRegistration(
    ui::Accelerator accelerator,
    base::WeakPtr<ui::AcceleratorTarget> target) {
  CHECK(panel_);
  return std::make_unique<GlicPanelScopedHotkeyRegistration>(accelerator,
                                                             panel_->GetView());
}
#endif

std::unique_ptr<LocalHotkeyManager> MakeGlicWindowHotkeyManager(
    base::WeakPtr<LocalHotkeyManager::Panel> panel) {
  return std::make_unique<LocalHotkeyManager>(
      panel, std::make_unique<GlicPanelHotkeyDelegate>(panel));
}

}  // namespace glic
