// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/compat_mode/resize_util.h"

#include <memory>

#include "ash/components/arc/compat_mode/arc_resize_lock_pref_delegate.h"
#include "ash/components/arc/compat_mode/arc_window_property_util.h"
#include "ash/components/arc/compat_mode/metrics.h"
#include "ash/components/arc/compat_mode/resize_confirmation_dialog_view.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/public/cpp/arc_compat_mode_util.h"
#include "ash/public/cpp/arc_resize_lock_type.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/public/cpp/window_properties.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/stl_util.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/shell_surface_util.h"
#include "components/strings/grit/components_strings.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/views/widget/widget.h"

namespace arc {

namespace {

// The following values must be the same with ARC-side hard-coded values.
// Also usually you should not directly refer to `kPortraitPhoneDp` as it may be
// adjusted on small displays (See `GetPossibleSizeInWorkArea`).
constexpr gfx::Size kPortraitPhoneDp(412, 732);
constexpr gfx::Size kLandscapeTabletDp(1064, 600);
constexpr int kDisplayEdgeOffsetDp = 27;

using ResizeCallback = base::OnceCallback<void(views::Widget*)>;

// The algorithm in `GetPossibleSizeInWorkArea` must be aligned with ARC-side.
gfx::Size GetPossibleSizeInWorkArea(aura::Window* window,
                                    const gfx::Size& preferred_size) {
  auto size = gfx::SizeF(preferred_size);
  const float preferred_aspect_ratio = size.width() / size.height();

  auto workarea =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).work_area();

  // Shrink workarea with the edge offset.
  workarea.Inset(gfx::Insets(kDisplayEdgeOffsetDp));
  auto* const frame_view = ash::NonClientFrameViewAsh::Get(window);
  if (frame_view) {
    workarea.Inset(
        gfx::Insets().set_top(frame_view->NonClientTopBorderHeight()));
  }

  // Limit |size| to |workarea| but keep the aspect ratio.
  if (size.width() > workarea.width()) {
    size.set_width(workarea.width());
    size.set_height(workarea.width() / preferred_aspect_ratio);
  }
  if (size.height() > workarea.height()) {
    size.set_width(workarea.height() * preferred_aspect_ratio);
    size.set_height(workarea.height());
  }

  const auto* shell_surface_base = exo::GetShellSurfaceBaseForWindow(window);
  // |shell_surface_base| can be null in unittests.
  if (shell_surface_base)
    size.SetToMax(gfx::SizeF(shell_surface_base->GetMinimumSize()));

  return gfx::ToFlooredSize(size);
}

gfx::Size GetPossibleSizeInWorkArea(const views::Widget* widget,
                                    const gfx::Size& preferred_size) {
  return GetPossibleSizeInWorkArea(widget->GetNativeWindow(), preferred_size);
}

void ResizeToPhone(views::Widget* widget) {
  // Clear the restore state/bounds key to make sure it's going to be restored
  // to normal state.
  widget->GetNativeWindow()->ClearProperty(aura::client::kRestoreShowStateKey);
  widget->GetNativeWindow()->ClearProperty(aura::client::kRestoreBoundsKey);
  // Always make sure the window is in normal state because the window might be
  // maximized/snapped.
  widget->GetNativeWindow()->SetProperty(aura::client::kShowStateKey,
                                         ui::mojom::WindowShowState::kNormal);

  widget->CenterWindow(GetPossibleSizeInWorkArea(widget, kPortraitPhoneDp));

  RecordResizeLockAction(ResizeLockActionType::ResizeToPhone);
}

void ResizeToTablet(views::Widget* widget) {
  // Clear the restore state/bounds key to make sure it's going to be restored
  // to normal state.
  widget->GetNativeWindow()->ClearProperty(aura::client::kRestoreShowStateKey);
  widget->GetNativeWindow()->ClearProperty(aura::client::kRestoreBoundsKey);
  // Always make sure the window is in normal state because the window might be
  // maximized/snapped.
  widget->GetNativeWindow()->SetProperty(aura::client::kShowStateKey,
                                         ui::mojom::WindowShowState::kNormal);

  // We here don't shrink the preferred size according to the available workarea
  // bounds like ResizeToPhone, because we'd like to let Android decide if the
  // ResizeToTablet operation fallbacks to the window state change operation.
  widget->CenterWindow(kLandscapeTabletDp);

  RecordResizeLockAction(ResizeLockActionType::ResizeToTablet);
}

void TurnOnResizeLock(views::Widget* widget,
                      ArcResizeLockPrefDelegate* pref_delegate) {
  const auto app_id = GetAppId(widget);
  if (app_id && pref_delegate->GetResizeLockState(*app_id) !=
                    mojom::ArcResizeLockState::ON) {
    pref_delegate->SetResizeLockState(*app_id, mojom::ArcResizeLockState::ON);

    RecordResizeLockAction(ResizeLockActionType::TurnOnResizeLock);
  }
}

void TurnOffResizeLock(views::Widget* target_widget,
                       ArcResizeLockPrefDelegate* pref_delegate) {
  const auto app_id = GetAppId(target_widget);
  if (!app_id || pref_delegate->GetResizeLockState(*app_id) ==
                     mojom::ArcResizeLockState::OFF) {
    return;
  }

  pref_delegate->SetResizeLockState(*app_id, mojom::ArcResizeLockState::OFF);

  RecordResizeLockAction(ResizeLockActionType::TurnOffResizeLock);

  auto* const toast_manager = ash::ToastManager::Get();
  // |toast_manager| can be null in some unittests.
  if (!toast_manager)
    return;

  constexpr char kTurnOffResizeLockToastId[] =
      "arc.compat_mode.turn_off_resize_lock";
  toast_manager->Cancel(kTurnOffResizeLockToastId);
  ash::ToastData toast(
      kTurnOffResizeLockToastId, ash::ToastCatalogName::kAppResizable,
      l10n_util::GetStringUTF16(IDS_ARC_COMPAT_MODE_DISABLE_RESIZE_LOCK_TOAST));
  toast_manager->Show(std::move(toast));
}

void TurnOffResizeLockWithConfirmationIfNeeded(
    views::Widget* target_widget,
    ArcResizeLockPrefDelegate* pref_delegate) {
  const auto app_id = GetAppId(target_widget);
  if (app_id && !pref_delegate->GetResizeLockNeedsConfirmation(*app_id)) {
    // The user has already agreed not to show the dialog again.
    TurnOffResizeLock(target_widget, pref_delegate);
    return;
  }

  // Set target app window as parent so that the dialog will be destroyed
  // together when the app window is destroyed (e.g. app crashed).
  ResizeConfirmationDialogView::Show(
      /*parent=*/target_widget,
      base::BindOnce(
          [](views::Widget* widget, ArcResizeLockPrefDelegate* delegate,
             bool accepted, bool do_not_ask_again) {
            if (accepted) {
              const auto app_id = GetAppId(widget);
              if (do_not_ask_again && app_id)
                delegate->SetResizeLockNeedsConfirmation(*app_id, false);

              TurnOffResizeLock(widget, delegate);
            }
          },
          base::Unretained(target_widget), base::Unretained(pref_delegate)));
}

}  // namespace

void ResizeLockToPhone(views::Widget* widget,
                       ArcResizeLockPrefDelegate* pref_delegate) {
  ResizeToPhone(widget);
  TurnOnResizeLock(widget, pref_delegate);
}

void ResizeLockToTablet(views::Widget* widget,
                        ArcResizeLockPrefDelegate* pref_delegate) {
  ResizeToTablet(widget);
  TurnOnResizeLock(widget, pref_delegate);
}

void EnableResizingWithConfirmationIfNeeded(
    views::Widget* widget,
    ArcResizeLockPrefDelegate* pref_delegate) {
  TurnOffResizeLockWithConfirmationIfNeeded(widget, pref_delegate);
}

bool ShouldShowSplashScreenDialog(ArcResizeLockPrefDelegate* pref_delegate) {
  int show_count = pref_delegate->GetShowSplashScreenDialogCount();
  if (show_count == 0)
    return false;

  pref_delegate->SetShowSplashScreenDialogCount(--show_count);
  return true;
}

int GetUnresizableSnappedWidth(aura::Window* window) {
  const auto& bounds = window->bounds();
  const bool isPortrait = bounds.width() <= bounds.height();
  const bool isNormal = window->GetProperty(aura::client::kShowStateKey) ==
                        ui::mojom::WindowShowState::kNormal;
  if (isPortrait && isNormal) {
    return bounds.width();
  }
  return kPortraitPhoneDp.width();
}

}  // namespace arc
