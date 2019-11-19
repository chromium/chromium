// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/window_properties.h"

#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/immersive/immersive_fullscreen_controller.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_pin_type.h"
#include "ash/public/cpp/window_state_type.h"
#include "ui/aura/window.h"
#include "ui/wm/core/window_properties.h"

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(ASH_PUBLIC_EXPORT, ash::WindowPinType)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(ASH_PUBLIC_EXPORT, ash::WindowStateType)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(ASH_PUBLIC_EXPORT,
                                       ash::BackdropWindowMode)

namespace ash {

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kAppIDKey, nullptr)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kArcPackageNameKey, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(BackdropWindowMode,
                             kBackdropWindowMode,
                             BackdropWindowMode::kAutoOpaque)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kBlockedForAssistantSnapshotKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kCanAttachToAnotherWindowKey, true)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kCanConsumeSystemKeysKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kHideInOverviewKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kHideShelfWhenFullscreenKey, true)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kImmersiveImpliedByFullscreen, true)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kImmersiveIsActive, false)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Rect,
                                   kImmersiveTopContainerBoundsInScreen,
                                   nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(
    int,
    kImmersiveWindowType,
    ImmersiveFullscreenController::WindowType::WINDOW_TYPE_OTHER)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kIsDeferredTabDraggingTargetWindowKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kIsDraggingTabsKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kIsShowingInOverviewKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kHideInDeskMiniViewKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kForceVisibleInMiniViewKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(WindowStateType,
                             kPrePipWindowStateTypeKey,
                             WindowStateType::kDefault)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kRenderTitleAreaProperty, false)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Rect,
                                   kRestoreBoundsOverrideKey,
                                   nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(WindowStateType,
                             kRestoreWindowStateTypeOverrideKey,
                             WindowStateType::kDefault)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSearchKeyAcceleratorReservedKey, false)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kShelfIDKey, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(int32_t, kShelfItemTypeKey, TYPE_UNDEFINED)
DEFINE_UI_CLASS_PROPERTY_KEY(aura::Window*,
                             kTabDraggingSourceWindowKey,
                             nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(SkColor, kFrameActiveColorKey, kDefaultFrameColor)
DEFINE_UI_CLASS_PROPERTY_KEY(SkColor,
                             kFrameInactiveColorKey,
                             kDefaultFrameColor)
DEFINE_UI_CLASS_PROPERTY_KEY(WindowPinType,
                             kWindowPinTypeKey,
                             WindowPinType::kNone)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kWindowPositionManagedTypeKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(WindowStateType,
                             kWindowStateTypeKey,
                             WindowStateType::kDefault)

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kWindowPipTypeKey, false)
}  // namespace ash
