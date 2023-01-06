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
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/views/widget/widget.h"

namespace arc {

namespace {

constexpr gfx::Size kPortraitPhoneDp(412, 732);
constexpr gfx::Size kLandscapeTabletDp(1064, 600);
constexpr int kDisplayEdgeOffsetDp = 32;

using ResizeCallback = base::OnceCallback<void(views::Widget*)>;

gfx::Size GetPossibleSizeInWorkArea(views::Widget* widget,
                                    const gfx::Size& preferred_size) {
  auto size = gfx::SizeF(preferred_size);
  const float preferred_aspect_ratio = size.width() / size.height();

  auto workarea = widget->GetWorkAreaBoundsInScreen();

  // Shrink workarea with the edge offset.
  workarea.Inset(gfx::Insets(kDisplayEdgeOffsetDp));

  // Limit |size| to |workarea| but keep the aspect ratio.
  if (size.width() > workarea.width()) {
    size.set_width(workarea.width());
    size.set_height(workarea.width() / preferred_aspect_ratio);
  }
  if (size.height() > workarea.height()) {
    size.set_width(workarea.height() * preferred_aspect_ratio);
    size.set_height(workarea.height());
  }

  const auto* shell_surface_base =
      exo::GetShellSurfaceBaseForWindow(widget->GetNativeWindow());
  // |shell_surface_base| can be null in unittests.
  if (shell_surface_base)
    size.SetToMax(gfx::SizeF(shell_surface_base->GetMinimumSize()));

  return gfx::ToFlooredSize(size);
}

void ResizeToPhone(views::Widget* widget) {
  if (widget->IsMaximized())
    widget->Restore();
  widget->CenterWindow(GetPossibleSizeInWorkArea(widget, kPortraitPhoneDp));

  RecordResizeLockAction(ResizeLockActionType::ResizeToPhone);
}

void ResizeToTablet(views::Widget* widget) {
  if (widget->IsMaximized())
    widget->Restore();

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
      /*parent=*/target_widget->GetNativeWindow(),
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

ResizeCompatMode PredictCurrentMode(const views::Widget* widget) {
  return PredictCurrentMode(widget->GetNativeWindow());
}

ResizeCompatMode PredictCurrentMode(const aura::Window* window) {
  const auto resize_lock_type = window->GetProperty(ash::kArcResizeLockTypeKey);
  if (resize_lock_type == ash::ArcResizeLockType::NONE ||
      resize_lock_type == ash::ArcResizeLockType::RESIZE_ENABLED_TOGGLABLE) {
    return ResizeCompatMode::kResizable;
  }

  const int width = window->bounds().width();
  const int height = window->bounds().height();
  // We don't use the exact size here to predict tablet or phone size because
  // the window size might be bigger than it due to the ARC app-side minimum
  // size constraints.
  if (width <= height)
    return ResizeCompatMode::kPhone;

  return ResizeCompatMode::kTablet;
}

bool ShouldShowSplashScreenDialog(ArcResizeLockPrefDelegate* pref_delegate) {
  int show_count = pref_delegate->GetShowSplashScreenDialogCount();
  if (show_count == 0)
    return false;

  pref_delegate->SetShowSplashScreenDialogCount(--show_count);
  return true;
}

int GetPortraitPhoneSizeWidth() {
  return kPortraitPhoneDp.width();
}

}  // namespace arc
