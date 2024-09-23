// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/window_properties.h"

#include "ash/public/cpp/resize_shadow_type.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_backdrop.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/aura/window.h"
#include "ui/wm/core/window_properties.h"

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(ASH_PUBLIC_EXPORT, ash::WindowBackdrop*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(ASH_PUBLIC_EXPORT, bool*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(ASH_PUBLIC_EXPORT, float*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(ASH_PUBLIC_EXPORT, SkRegion*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(ASH_PUBLIC_EXPORT,
                                       ash::ArcGameControlsFlag)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(ASH_PUBLIC_EXPORT,
                                       ash::ArcResizeLockType)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(ASH_PUBLIC_EXPORT, ash::ResizeShadowType)

namespace ash {

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kAppIDKey, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(ArcGameControlsFlag,
                             kArcGameControlsFlagsKey,
                             static_cast<ArcGameControlsFlag>(0))
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kArcPackageNameKey, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(ArcResizeLockType,
                             kArcResizeLockTypeKey,
                             ArcResizeLockType::NONE)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(WindowBackdrop, kWindowBackdropKey, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kCanConsumeSystemKeysKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(int32_t, kClientAccessibilityIdKey, -1)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kExcludeInMruKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kExcludeFromTransientTreeTransformKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kIgnoreWindowActivationKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kFrameRateThrottleKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kHideInOverviewKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kHideInShelfKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kIsDraggingTabsKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kHideInDeskMiniViewKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kForceVisibleInMiniViewKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(uint64_t, kLacrosProfileId, 0)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(bool, kMinimizeOnBackKey, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kPipOriginalWindowKey, false)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(float, kPipSnapFractionKey, nullptr)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Size,
                                   kUnresizableSnappedSizeKey,
                                   nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kRenderTitleAreaProperty, false)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Rect,
                                   kRestoreBoundsOverrideKey,
                                   nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(chromeos::WindowStateType,
                             kRestoreWindowStateTypeOverrideKey,
                             chromeos::WindowStateType::kDefault)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSearchKeyAcceleratorReservedKey, false)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kShelfIDKey, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(int32_t, kShelfItemTypeKey, TYPE_UNDEFINED)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(SkRegion,
                                   kSystemGestureExclusionKey,
                                   nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kWindowPositionManagedTypeKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kShowCursorOnKeypress, false)

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kWindowPipTypeKey, false)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Rect,
                                   kWindowPipResizeHandleBoundsKey,
                                   nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(ResizeShadowType,
                             kResizeShadowTypeKey,
                             ResizeShadowType::kUnlock)

}  // namespace ash
