// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/window_properties.h"

#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/immersive/immersive_fullscreen_controller.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_pin_type.h"
#include "ash/public/cpp/window_state_type.h"
#include "ash/public/interfaces/window_pin_type.mojom.h"
#include "ash/public/interfaces/window_properties.mojom.h"
#include "ash/public/interfaces/window_state_type.mojom.h"
#include "base/unguessable_token.h"
#include "services/ws/public/mojom/window_manager.mojom.h"
#include "ui/accessibility/platform/aura_window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/window.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/wm/core/window_properties.h"

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(ASH_PUBLIC_EXPORT,
                                       ash::mojom::WindowPinType)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(ASH_PUBLIC_EXPORT,
                                       ash::mojom::WindowStateType)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(ASH_PUBLIC_EXPORT,
                                       ash::BackdropWindowMode)

namespace ash {
namespace {

bool IsValidWindowVisibilityAnimationTransition(int64_t value) {
  return value == wm::ANIMATE_SHOW || value == wm::ANIMATE_HIDE ||
         value == wm::ANIMATE_BOTH || value == wm::ANIMATE_NONE;
}

}  // namespace

void RegisterWindowProperties(aura::PropertyConverter* property_converter) {
  property_converter->RegisterStringProperty(
      kArcPackageNameKey, ws::mojom::WindowManager::kArcPackageName_Property);
  property_converter->RegisterPrimitiveProperty(
      aura::client::kAppType, mojom::kAppType_Property,
      aura::PropertyConverter::CreateAcceptAnyValueCallback());
  property_converter->RegisterPrimitiveProperty(
      kBlockedForAssistantSnapshotKey,
      mojom::kBlockedForAssistantSnapshot_Property,
      aura::PropertyConverter::CreateAcceptAnyValueCallback());
  property_converter->RegisterPrimitiveProperty(
      kCanConsumeSystemKeysKey, mojom::kCanConsumeSystemKeys_Property,
      aura::PropertyConverter::CreateAcceptAnyValueCallback());
  property_converter->RegisterStringProperty(
      ui::kChildAXTreeID, ws::mojom::WindowManager::kChildAXTreeID_Property);
  property_converter->RegisterPrimitiveProperty(
      kFrameActiveColorKey,
      ws::mojom::WindowManager::kFrameActiveColor_Property,
      aura::PropertyConverter::CreateAcceptAnyValueCallback());
  property_converter->RegisterPrimitiveProperty(
      kFrameInactiveColorKey,
      ws::mojom::WindowManager::kFrameInactiveColor_Property,
      aura::PropertyConverter::CreateAcceptAnyValueCallback());
  property_converter->RegisterPrimitiveProperty(
      kHideShelfWhenFullscreenKey, mojom::kHideShelfWhenFullscreen_Property,
      aura::PropertyConverter::CreateAcceptAnyValueCallback());
  property_converter->RegisterPrimitiveProperty(
      kImmersiveImpliedByFullscreen,
      mojom::kImmersiveImpliedByFullscreen_Property,
      aura::PropertyConverter::CreateAcceptAnyValueCallback());
  property_converter->RegisterPrimitiveProperty(
      kImmersiveIsActive, mojom::kImmersiveIsActive_Property,
      aura::PropertyConverter::CreateAcceptAnyValueCallback());
  property_converter->RegisterRectProperty(
      kImmersiveTopContainerBoundsInScreen,
      mojom::kImmersiveTopContainerBoundsInScreen_Property);
  property_converter->RegisterPrimitiveProperty(
      kIsDeferredTabDraggingTargetWindowKey,
      mojom::kIsDeferredTabDraggingTargetWindow_Property,
      aura::PropertyConverter::CreateAcceptAnyValueCallback());
  property_converter->RegisterPrimitiveProperty(
      kIsDraggingTabsKey, mojom::kIsDraggingTabs_Property,
      aura::PropertyConverter::CreateAcceptAnyValueCallback());
  property_converter->RegisterPrimitiveProperty(
      kIsShowingInOverviewKey, mojom::kIsShowingInOverview_Property,
      aura::PropertyConverter::CreateAcceptAnyValueCallback());
  property_converter->RegisterPrimitiveProperty(
      kPanelAttachedKey, ws::mojom::WindowManager::kPanelAttached_Property,
      aura::PropertyConverter::CreateAcceptAnyValueCallback());
  property_converter->RegisterPrimitiveProperty(
      kRenderTitleAreaProperty,
      ws::mojom::WindowManager::kRenderParentTitleArea_Property,
      aura::PropertyConverter::CreateAcceptAnyValueCallback());
  property_converter->RegisterPrimitiveProperty(
      kShelfItemTypeKey, ws::mojom::WindowManager::kShelfItemType_Property,
      base::BindRepeating(&IsValidShelfItemType));
  property_converter->RegisterPrimitiveProperty(
      aura::client::kTopViewInset, mojom::kTopViewInset_Property,
      aura::PropertyConverter::CreateAcceptAnyValueCallback());
  property_converter->RegisterPrimitiveProperty(
      kWindowStateTypeKey, mojom::kWindowStateType_Property,
      base::BindRepeating(&IsValidWindowStateType));
  property_converter->RegisterPrimitiveProperty(
      kWindowPinTypeKey, mojom::kWindowPinType_Property,
      base::BindRepeating(&IsValidWindowPinType));
  property_converter->RegisterPrimitiveProperty(
      kWindowPositionManagedTypeKey, mojom::kWindowPositionManaged_Property,
      aura::PropertyConverter::CreateAcceptAnyValueCallback());
  property_converter->RegisterStringProperty(
      kShelfIDKey, ws::mojom::WindowManager::kShelfID_Property);
  property_converter->RegisterRectProperty(
      kRestoreBoundsOverrideKey, mojom::kRestoreBoundsOverride_Property);
  property_converter->RegisterPrimitiveProperty(
      kRestoreWindowStateTypeOverrideKey,
      mojom::kRestoreWindowStateTypeOverride_Property,
      base::BindRepeating(&IsValidWindowStateType));
  property_converter->RegisterPrimitiveProperty(
      kTabDroppedWindowStateTypeKey, mojom::kTabDroppedWindowStateType_Property,
      base::BindRepeating(&IsValidWindowStateType));
  property_converter->RegisterPrimitiveProperty(
      aura::client::kTitleShownKey,
      ws::mojom::WindowManager::kWindowTitleShown_Property,
      aura::PropertyConverter::CreateAcceptAnyValueCallback());
  property_converter->RegisterWindowPtrProperty(
      kTabDraggingSourceWindowKey, mojom::kTabDraggingSourceWindow_Property);
  property_converter->RegisterTimeDeltaProperty(
      wm::kWindowVisibilityAnimationDurationKey,
      ws::mojom::WindowManager::kWindowVisibilityAnimationDuration_Property);
  property_converter->RegisterPrimitiveProperty(
      wm::kWindowVisibilityAnimationTransitionKey,
      ws::mojom::WindowManager::kWindowVisibilityAnimationTransition_Property,
      base::BindRepeating(&IsValidWindowVisibilityAnimationTransition));
  property_converter->RegisterPrimitiveProperty(
      wm::kWindowVisibilityAnimationTypeKey,
      ws::mojom::WindowManager::kWindowVisibilityAnimationType_Property,
      aura::PropertyConverter::CreateAcceptAnyValueCallback());
  property_converter->RegisterPrimitiveProperty(
      wm::kWindowVisibilityAnimationVerticalPositionKey,
      ws::mojom::WindowManager::
          kWindowVisibilityAnimationVerticalPosition_Property,
      aura::PropertyConverter::CreateAcceptAnyValueCallback());
}

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kArcPackageNameKey, nullptr);
DEFINE_UI_CLASS_PROPERTY_KEY(BackdropWindowMode,
                             kBackdropWindowMode,
                             BackdropWindowMode::kAuto);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kBlockedForAssistantSnapshotKey, false);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kCanAttachToAnotherWindowKey, true);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kCanConsumeSystemKeysKey, false);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kHideInOverviewKey, false);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kHideShelfWhenFullscreenKey, true);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kImmersiveImpliedByFullscreen, true);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kImmersiveIsActive, false);
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Rect,
                                   kImmersiveTopContainerBoundsInScreen,
                                   nullptr);
DEFINE_UI_CLASS_PROPERTY_KEY(
    int,
    kImmersiveWindowType,
    ImmersiveFullscreenController::WindowType::WINDOW_TYPE_OTHER);
DEFINE_UI_CLASS_PROPERTY_KEY(bool,
                             kIsDeferredTabDraggingTargetWindowKey,
                             false);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kIsDraggingTabsKey, false);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kIsShowingInOverviewKey, false);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kPanelAttachedKey, true);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kRenderTitleAreaProperty, false);
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Rect,
                                   kRestoreBoundsOverrideKey,
                                   nullptr);
DEFINE_UI_CLASS_PROPERTY_KEY(mojom::WindowStateType,
                             kRestoreWindowStateTypeOverrideKey,
                             mojom::WindowStateType::DEFAULT);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSearchKeyAcceleratorReservedKey, false);
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kShelfIDKey, nullptr);
DEFINE_UI_CLASS_PROPERTY_KEY(int32_t, kShelfItemTypeKey, TYPE_UNDEFINED);
DEFINE_UI_CLASS_PROPERTY_KEY(aura::Window*,
                             kTabDraggingSourceWindowKey,
                             nullptr);
DEFINE_UI_CLASS_PROPERTY_KEY(mojom::WindowStateType,
                             kTabDroppedWindowStateTypeKey,
                             mojom::WindowStateType::DEFAULT);

DEFINE_UI_CLASS_PROPERTY_KEY(SkColor, kFrameActiveColorKey, kDefaultFrameColor);
DEFINE_UI_CLASS_PROPERTY_KEY(SkColor,
                             kFrameInactiveColorKey,
                             kDefaultFrameColor);
DEFINE_UI_CLASS_PROPERTY_KEY(mojom::WindowPinType,
                             kWindowPinTypeKey,
                             mojom::WindowPinType::NONE);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kWindowPositionManagedTypeKey, false);
DEFINE_UI_CLASS_PROPERTY_KEY(mojom::WindowStateType,
                             kWindowStateTypeKey,
                             mojom::WindowStateType::DEFAULT);

}  // namespace ash
