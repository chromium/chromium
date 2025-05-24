// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_window_hotkey_delegate.h"

#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/base/accelerators/command.h"

namespace glic {

namespace {

static constexpr std::array kSupportedHotkeys = {
    glic::LocalHotkeyManager::Hotkey::kClose,
    glic::LocalHotkeyManager::Hotkey::kFocusToggle,
#if BUILDFLAG(IS_WIN)
    glic::LocalHotkeyManager::Hotkey::kTitleBarContextMenu,
#endif
};

// Implementation of ScopedHotkeyRegistration specifically for the Glic window.
// It registers and unregisters accelerators directly with the GlicView.
class GlicWindowScopedHotkeyRegistration
    : public LocalHotkeyManager::ScopedHotkeyRegistration {
 public:
  GlicWindowScopedHotkeyRegistration(ui::Accelerator accelerator,
                                     base::WeakPtr<views::View> glic_view)
      : accelerator_(accelerator), glic_view_(glic_view) {
    CHECK(!accelerator.IsEmpty());
    CHECK(glic_view_);
    glic_view_->AddAccelerator(accelerator_);
  }

  ~GlicWindowScopedHotkeyRegistration() override {
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

GlicWindowHotkeyDelegate::GlicWindowHotkeyDelegate(
    base::WeakPtr<GlicWindowController> window_controller)
    : window_controller_(window_controller) {}

GlicWindowHotkeyDelegate::~GlicWindowHotkeyDelegate() = default;

const base::span<const LocalHotkeyManager::Hotkey>
GlicWindowHotkeyDelegate::GetSupportedHotkeys() const {
  return kSupportedHotkeys;
}

bool GlicWindowHotkeyDelegate::AcceleratorPressed(
    LocalHotkeyManager::Hotkey hotkey) {
  if (!window_controller_) {
    return false;
  }

  switch (hotkey) {
    case LocalHotkeyManager::Hotkey::kClose:
      window_controller_->Close();
      return true;
    case glic::LocalHotkeyManager::Hotkey::kFocusToggle:
      if (window_controller_->IsAttached()) {
        window_controller_->attached_browser()->window()->Activate();
        return true;
      }
      if (auto* last_active = BrowserList::GetInstance()->GetLastActive()) {
        last_active->window()->Activate();
        base::RecordAction(base::UserMetricsAction("Glic.FocusHotKey"));
        return true;
      }
      return false;
#if BUILDFLAG(IS_WIN)
    case LocalHotkeyManager::Hotkey::kTitleBarContextMenu:
      window_controller_->ShowTitleBarContextMenuAt(gfx::Point());
      return true;
#endif  //  BUILDFLAG(IS_WIN)

    default:
      NOTREACHED() << "no handling implemented for "
                   << LocalHotkeyManager::HotkeyToString(hotkey);
  }
}

std::unique_ptr<LocalHotkeyManager::ScopedHotkeyRegistration>
GlicWindowHotkeyDelegate::CreateScopedHotkeyRegistration(
    ui::Accelerator accelerator,
    base::WeakPtr<ui::AcceleratorTarget> target) {
  CHECK(window_controller_);
  return std::make_unique<GlicWindowScopedHotkeyRegistration>(
      accelerator, window_controller_->GetGlicViewAsView());
}

std::unique_ptr<LocalHotkeyManager> MakeGlicWindowHotkeyManager(
    base::WeakPtr<GlicWindowController> window_controller) {
  return std::make_unique<LocalHotkeyManager>(
      window_controller,
      std::make_unique<GlicWindowHotkeyDelegate>(window_controller));
}

}  // namespace glic
