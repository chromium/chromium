// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/cros_display_config.h"

#include <memory>
#include <utility>

#include "ash/display/display_configuration_controller.h"
#include "ash/display/display_prefs.h"
#include "ash/display/overscan_calibrator.h"
#include "ash/display/resolution_notification_controller.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/display/touch_calibrator_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/public/mojom/cros_display_config.mojom.h"
#include "ash/shell.h"
#include "ash/touch/ash_touch_transform_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/bind.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/display_util.h"
#include "ui/display/mojom/display_mojom_traits.h"
#include "ui/display/screen.h"

namespace ash {

namespace {

// Maximum allowed bounds origin absolute value.
constexpr int kMaxBoundsOrigin = 200 * 1000;

// This is the default range of display width in logical pixels allowed after
// applying display zoom.
constexpr int kDefaultMaxZoomWidth = 4096;
constexpr int kDefaultMinZoomWidth = 640;

display::DisplayManager* GetDisplayManager() {
  return Shell::Get()->display_manager();
}

int64_t GetDisplayId(const std::string& display_id_str) {
  int64_t display_id;
  if (!base::StringToInt64(display_id_str, &display_id))
    display_id = display::kInvalidDisplayId;
  return display_id;
}

// Gets the display with the provided string id.
display::Display GetDisplay(const std::string& display_id_str) {
  int64_t display_id = GetDisplayId(display_id_str);
  if (display_id == display::kInvalidDisplayId)
    return display::Display();
  display::DisplayManager* display_manager = GetDisplayManager();
  if (display_manager->IsInUnifiedMode() &&
      display_id != display::kUnifiedDisplayId) {
    // In unified desktop mode, the mirroring displays, which constitue the
    // combined unified display, are contained in the software mirroring
    // displays list in the display manager.
    // The active list of displays only contains a single unified display entry
    // with id |kUnifiedDisplayId|.
    return display_manager->GetMirroringDisplayById(display_id);
  }
  return display_manager->GetDisplayForId(display_id);
}

mojom::DisplayLayoutPosition GetMojomDisplayLayoutPosition(
    display::DisplayPlacement::Position position) {
  switch (position) {
    case display::DisplayPlacement::TOP:
      return mojom::DisplayLayoutPosition::kTop;
    case display::DisplayPlacement::RIGHT:
      return mojom::DisplayLayoutPosition::kRight;
    case display::DisplayPlacement::BOTTOM:
      return mojom::DisplayLayoutPosition::kBottom;
    case display::DisplayPlacement::LEFT:
      return mojom::DisplayLayoutPosition::kLeft;
  }
  NOTREACHED();
  return mojom::DisplayLayoutPosition::kLeft;
}

display::DisplayPlacement::Position GetDisplayPlacementPosition(
    mojom::DisplayLayoutPosition position) {
  switch (position) {
    case mojom::DisplayLayoutPosition::kTop:
      return display::DisplayPlacement::TOP;
    case mojom::DisplayLayoutPosition::kRight:
      return display::DisplayPlacement::RIGHT;
    case mojom::DisplayLayoutPosition::kBottom:
      return display::DisplayPlacement::BOTTOM;
    case mojom::DisplayLayoutPosition::kLeft:
      return display::DisplayPlacement::LEFT;
  }
  NOTREACHED();
  return display::DisplayPlacement::LEFT;
}

std::vector<mojom::DisplayLayoutPtr> GetDisplayLayouts() {
  auto layouts = std::vector<mojom::DisplayLayoutPtr>();
  display::Screen* screen = display::Screen::GetScreen();
  const std::vector<display::Display>& displays = screen->GetAllDisplays();
  display::DisplayManager* display_manager = GetDisplayManager();
  for (const display::Display& display : displays) {
    const display::DisplayPlacement placement =
        display_manager->GetCurrentResolvedDisplayLayout().FindPlacementById(
            display.id());
    if (placement.display_id == display::kInvalidDisplayId)
      continue;
    auto layout = mojom::DisplayLayout::New();
    layout->id = base::NumberToString(placement.display_id);
    layout->parent_id = base::NumberToString(placement.parent_display_id);
    layout->position = GetMojomDisplayLayoutPosition(placement.position);
    layout->offset = placement.offset;
    layouts.emplace_back(std::move(layout));
  }
  return layouts;
}

std::vector<mojom::DisplayLayoutPtr> GetDisplayUnifiedLayouts() {
  auto layouts = std::vector<mojom::DisplayLayoutPtr>();
  display::DisplayManager* display_manager = GetDisplayManager();

  const display::UnifiedDesktopLayoutMatrix& matrix =
      display_manager->current_unified_desktop_matrix();
  for (size_t row_index = 0; row_index < matrix.size(); ++row_index) {
    const auto& row = matrix[row_index];
    for (size_t column_index = 0; column_index < row.size(); ++column_index) {
      if (column_index == 0 && row_index == 0) {
        // No placement for the primary display.
        continue;
      }
      auto layout = mojom::DisplayLayout::New();
      const int64_t display_id = row[column_index];
      // Parent display is either the one in the above row, or the one on the
      // left in the same row.
      const int64_t parent_id = column_index == 0
                                    ? matrix[row_index - 1][column_index]
                                    : row[column_index - 1];
      layout->id = base::NumberToString(display_id);
      layout->parent_id = base::NumberToString(parent_id);
      layout->position = column_index == 0
                             ? mojom::DisplayLayoutPosition::kBottom
                             : mojom::DisplayLayoutPosition::kRight;
      layout->offset = 0;
      layouts.emplace_back(std::move(layout));
    }
  }
  return layouts;
}

mojom::DisplayConfigResult SetDisplayLayoutMode(
    const mojom::DisplayLayoutInfo& info) {
  display::DisplayManager* display_manager = GetDisplayManager();
  if (info.layout_mode == mojom::DisplayLayoutMode::kNormal) {
    display_manager->SetDefaultMultiDisplayModeForCurrentDisplays(
        display::DisplayManager::EXTENDED);
    display_manager->SetMirrorMode(display::MirrorMode::kOff, base::nullopt);
    return mojom::DisplayConfigResult::kSuccess;
  }

  if (info.layout_mode == mojom::DisplayLayoutMode::kUnified) {
    if (!display_manager->unified_desktop_enabled())
      return mojom::DisplayConfigResult::kUnifiedNotEnabledError;
    display_manager->SetDefaultMultiDisplayModeForCurrentDisplays(
        display::DisplayManager::UNIFIED);
    display_manager->SetMirrorMode(display::MirrorMode::kOff, base::nullopt);
    return mojom::DisplayConfigResult::kSuccess;
  }

  DCHECK(info.layout_mode == mojom::DisplayLayoutMode::kMirrored);

  // 'Normal' mirror mode.
  if (!info.mirror_source_id) {
    display_manager->SetMirrorMode(display::MirrorMode::kNormal, base::nullopt);
    return mojom::DisplayConfigResult::kSuccess;
  }

  // 'Mixed' mirror mode.
  display::Display source = GetDisplay(*info.mirror_source_id);
  if (source.id() == display::kInvalidDisplayId)
    return mojom::DisplayConfigResult::kMirrorModeSourceIdError;
  display::DisplayIdList destination_ids;
  if (info.mirror_destination_ids) {
    for (const std::string& id_str : *info.mirror_destination_ids) {
      int64_t destination_id = GetDisplayId(id_str);
      if (destination_id == display::kInvalidDisplayId)
        return mojom::DisplayConfigResult::kMirrorModeDestIdError;
      destination_ids.emplace_back(destination_id);
    }
  } else {
    const std::vector<display::Display>& displays =
        display::Screen::GetScreen()->GetAllDisplays();
    for (const display::Display& display : displays)
      destination_ids.emplace_back(display.id());
  }
  base::Optional<display::MixedMirrorModeParams> mixed_params(
      base::in_place, source.id(), destination_ids);
  const display::MixedMirrorModeParamsErrors error_type =
      display::ValidateParamsForMixedMirrorMode(
          display_manager->GetCurrentDisplayIdList(), *mixed_params);
  switch (error_type) {
    case display::MixedMirrorModeParamsErrors::kErrorSingleDisplay:
      return mojom::DisplayConfigResult::kMirrorModeSingleDisplayError;
    case display::MixedMirrorModeParamsErrors::kErrorSourceIdNotFound:
      return mojom::DisplayConfigResult::kMirrorModeSourceIdError;
    case display::MixedMirrorModeParamsErrors::kErrorDestinationIdsEmpty:
    case display::MixedMirrorModeParamsErrors::kErrorDestinationIdNotFound:
    case display::MixedMirrorModeParamsErrors::kErrorDuplicateId:
      return mojom::DisplayConfigResult::kMirrorModeDestIdError;
    case display::MixedMirrorModeParamsErrors::kSuccess:
      break;
  }
  display_manager->SetMirrorMode(display::MirrorMode::kMixed, mixed_params);
  return mojom::DisplayConfigResult::kSuccess;
}

mojom::DisplayModePtr GetDisplayMode(
    const display::ManagedDisplayInfo& display_info,
    const display::ManagedDisplayMode& display_mode) {
  auto result = mojom::DisplayMode::New();
  bool is_internal = display::Display::HasInternalDisplay() &&
                     display::Display::InternalDisplayId() == display_info.id();
  gfx::Size size_dip = display_mode.GetSizeInDIP(is_internal);
  result->size = size_dip;
  result->size_in_native_pixels = display_mode.size();
  result->device_scale_factor = display_mode.device_scale_factor();
  result->refresh_rate = display_mode.refresh_rate();
  result->is_native = display_mode.native();
  result->is_interlaced = display_mode.is_interlaced();
  return result;
}

display::Display::Rotation DisplayRotationFromRotationOptions(
    mojom::DisplayRotationOptions option) {
  switch (option) {
    case mojom::DisplayRotationOptions::kAutoRotate:
      return display::Display::ROTATE_0;

    case mojom::DisplayRotationOptions::kZeroDegrees:
      return display::Display::ROTATE_0;

    case mojom::DisplayRotationOptions::k90Degrees:
      return display::Display::ROTATE_90;

    case mojom::DisplayRotationOptions::k180Degrees:
      return display::Display::ROTATE_180;

    case mojom::DisplayRotationOptions::k270Degrees:
      return display::Display::ROTATE_270;
  }
}

mojom::DisplayRotationOptions RotationOptionsFromDisplayRotation(
    display::Display::Rotation rotation) {
  const bool is_in_tablet_physical_state =
      Shell::Get()->tablet_mode_controller()->is_in_tablet_physical_state();
  const bool is_auto_rotate_enabled =
      !Shell::Get()->screen_orientation_controller()->user_rotation_locked();
  if (is_in_tablet_physical_state && is_auto_rotate_enabled)
    return mojom::DisplayRotationOptions::kAutoRotate;

  switch (rotation) {
    case display::Display::ROTATE_0:
      return mojom::DisplayRotationOptions::kZeroDegrees;

    case display::Display::ROTATE_90:
      return mojom::DisplayRotationOptions::k90Degrees;

    case display::Display::ROTATE_180:
      return mojom::DisplayRotationOptions::k180Degrees;

    case display::Display::ROTATE_270:
      return mojom::DisplayRotationOptions::k270Degrees;
  }
}

mojom::DisplayUnitInfoPtr GetDisplayUnitInfo(const display::Display& display,
                                             int64_t primary_id) {
  display::DisplayManager* display_manager = GetDisplayManager();
  const display::ManagedDisplayInfo& display_info =
      display_manager->GetDisplayInfo(display.id());

  auto info = mojom::DisplayUnitInfo::New();
  info->id = base::NumberToString(display.id());
  info->name = display_manager->GetDisplayNameForId(display.id());

  if (!display_info.manufacturer_id().empty() ||
      !display_info.product_id().empty() ||
      (display_info.year_of_manufacture() !=
       display::kInvalidYearOfManufacture)) {
    info->edid = mojom::Edid::New();
    info->edid->manufacturer_id = display_info.manufacturer_id();
    info->edid->product_id = display_info.product_id();
    info->edid->year_of_manufacture = display_info.year_of_manufacture();
  }

  info->is_primary = display.id() == primary_id;
  info->is_internal = display.IsInternal();
  info->is_enabled = true;
  info->is_in_tablet_physical_state =
      Shell::Get()->tablet_mode_controller()->is_in_tablet_physical_state();
  const bool has_accelerometer_support =
      display.accelerometer_support() ==
      display::Display::AccelerometerSupport::AVAILABLE;
  info->has_touch_support =
      display.touch_support() == display::Display::TouchSupport::AVAILABLE;
  info->has_accelerometer_support = has_accelerometer_support;

  const float device_dpi = display_info.device_dpi();
  info->dpi_x = device_dpi * display.size().width() /
                display_info.bounds_in_native().width();
  info->dpi_y = device_dpi * display.size().height() /
                display_info.bounds_in_native().height();

  info->rotation_options =
      RotationOptionsFromDisplayRotation(display.rotation());
  info->bounds = display.bounds();
  info->overscan = display_manager->GetOverscanInsets(display.id());
  info->work_area = display.work_area();

  int display_mode_index = 0;
  display::ManagedDisplayMode active_mode;
  bool has_active_mode = display_manager->GetActiveModeForDisplayId(
      display_info.id(), &active_mode);
  for (const display::ManagedDisplayMode& display_mode :
       display_info.display_modes()) {
    info->available_display_modes.emplace_back(
        GetDisplayMode(display_info, display_mode));
    if (has_active_mode && display_mode.IsEquivalent(active_mode))
      info->selected_display_mode_index = display_mode_index;
    ++display_mode_index;
  }

  info->display_zoom_factor = display_info.zoom_factor();
  if (has_active_mode) {
    auto zoom_levels = display::GetDisplayZoomFactors(active_mode);
    // Ensure that the current zoom factor is in the list.
    display::InsertDsfIntoList(&zoom_levels, display_info.zoom_factor());
    info->available_display_zoom_factors.assign(zoom_levels.begin(),
                                                zoom_levels.end());

  } else {
    info->available_display_zoom_factors.push_back(display_info.zoom_factor());
  }

  return info;
}

// Validates that DisplayProperties are valid with the current DisplayManager
// configuration. Returns an error on failure.
mojom::DisplayConfigResult ValidateDisplayProperties(
    const mojom::DisplayConfigProperties& properties,
    const display::Display& display) {
  display::DisplayManager* display_manager = GetDisplayManager();

  int64_t id = display.id();
  if (id == display::kInvalidDisplayId)
    return mojom::DisplayConfigResult::kInvalidDisplayIdError;

  // Overscan cannot be changed for the internal display, and should be at most
  // half of the screen size.
  if (properties.overscan) {
    if (display.IsInternal())
      return mojom::DisplayConfigResult::kNotSupportedOnInternalDisplayError;

    if (properties.overscan->left() < 0 || properties.overscan->top() < 0 ||
        properties.overscan->right() < 0 || properties.overscan->bottom() < 0) {
      DLOG(ERROR) << "Negative overscan";
      return mojom::DisplayConfigResult::kPropertyValueOutOfRangeError;
    }

    const gfx::Insets overscan = display_manager->GetOverscanInsets(id);
    int screen_width = display.bounds().width() + overscan.width();
    int screen_height = display.bounds().height() + overscan.height();

    if ((properties.overscan->left() + properties.overscan->right()) * 2 >
            screen_width ||
        (properties.overscan->top() + properties.overscan->bottom()) * 2 >
            screen_height) {
      DLOG(ERROR) << "Overscan: " << properties.overscan->ToString()
                  << " exceeds bounds: " << screen_width << "x"
                  << screen_height;
      return mojom::DisplayConfigResult::kPropertyValueOutOfRangeError;
    }
  }

  // The bounds cannot be changed for the primary display and should be inside
  // a reasonable bounds.
  if (properties.bounds_origin) {
    const display::Display& primary =
        display::Screen::GetScreen()->GetPrimaryDisplay();
    if (id == primary.id() || properties.set_primary)
      return mojom::DisplayConfigResult::kNotSupportedOnInternalDisplayError;
    if (properties.bounds_origin->x() > kMaxBoundsOrigin ||
        properties.bounds_origin->x() < -kMaxBoundsOrigin ||
        properties.bounds_origin->y() > kMaxBoundsOrigin ||
        properties.bounds_origin->y() < -kMaxBoundsOrigin) {
      DLOG(ERROR) << "Bounds origin out of range";
      return mojom::DisplayConfigResult::kPropertyValueOutOfRangeError;
    }
  }

  if (properties.display_zoom_factor > 0) {
    display::ManagedDisplayMode current_mode;
    if (!display_manager->GetActiveModeForDisplayId(id, &current_mode)) {
      DLOG(ERROR) << "No active mode for display: " << id;
      return mojom::DisplayConfigResult::kInvalidDisplayIdError;
    }
    // This check is added to limit the range of display zoom that can be
    // applied via the system display API. The said range is such that when a
    // display zoom is applied, the final logical width in pixels should lie
    // within the range of 640 pixels and 4096 pixels.
    const int landscape_width =
        std::max(current_mode.size().width(), current_mode.size().height());
    const int max_allowed_width =
        std::max(kDefaultMaxZoomWidth, landscape_width);
    const int min_allowed_width =
        std::min(kDefaultMinZoomWidth, landscape_width);
    int current_width = static_cast<float>(landscape_width) /
                        current_mode.device_scale_factor();
    if (current_width / properties.display_zoom_factor > max_allowed_width ||
        current_width / properties.display_zoom_factor < min_allowed_width) {
      DLOG(ERROR) << "Display zoom factor out of range";
      return mojom::DisplayConfigResult::kPropertyValueOutOfRangeError;
    }
  }

  return mojom::DisplayConfigResult::kSuccess;
}

// Sets the display layout for the target display in reference to the primary
// display.
void SetDisplayLayoutFromBounds(const gfx::Rect& primary_display_bounds,
                                int64_t primary_display_id,
                                const gfx::Rect& target_display_bounds,
                                int64_t target_display_id) {
  display::DisplayPlacement placement(
      display::DisplayLayout::CreatePlacementForRectangles(
          primary_display_bounds, target_display_bounds));
  placement.display_id = target_display_id;
  placement.parent_display_id = primary_display_id;

  std::unique_ptr<display::DisplayLayout> layout(new display::DisplayLayout);
  layout->placement_list.push_back(placement);
  layout->primary_id = primary_display_id;

  Shell::Get()->display_configuration_controller()->SetDisplayLayout(
      std::move(layout));
}

// Attempts to set the display mode for display |id|.
mojom::DisplayConfigResult SetDisplayMode(
    int64_t id,
    const mojom::DisplayMode& display_mode,
    mojom::DisplayConfigSource source) {
  display::DisplayManager* display_manager = GetDisplayManager();

  display::ManagedDisplayMode current_mode;
  if (!display_manager->GetActiveModeForDisplayId(id, &current_mode))
    return mojom::DisplayConfigResult::kInvalidDisplayIdError;

  display::ManagedDisplayMode new_mode(
      display_mode.size_in_native_pixels, display_mode.refresh_rate,
      display_mode.is_interlaced, display_mode.is_native,
      display_mode.device_scale_factor);

  if (!new_mode.IsEquivalent(current_mode)) {
    // For the internal display, the display mode will be applied directly.
    // Otherwise a confirm/revert notification will be prepared first, and the
    // display mode will be applied. If the user accepts the mode change by
    // dismissing the notification, MaybeStoreDisplayPrefs() will be called back
    // to persist the new preferences.
    if (!Shell::Get()
             ->resolution_notification_controller()
             ->PrepareNotificationAndSetDisplayMode(
                 id, current_mode, new_mode, source, base::BindOnce([]() {
                   Shell::Get()->display_prefs()->MaybeStoreDisplayPrefs();
                 }))) {
      return mojom::DisplayConfigResult::kSetDisplayModeError;
    }
  }

  return mojom::DisplayConfigResult::kSuccess;
}

display::TouchCalibrationData::CalibrationPointPair GetCalibrationPair(
    const mojom::TouchCalibrationPair& pair) {
  return std::make_pair(pair.display_point, pair.touch_point);
}

}  // namespace

// -----------------------------------------------------------------------------
// CrosDisplayConfig::ObserverImpl:

// Observes display and tablet mode events, and notifies the
// CrosDisplayConfigObservers with OnDisplayConfigChanged() in response to those
// events.
class CrosDisplayConfig::ObserverImpl
    : public display::DisplayObserver,
      public TabletModeObserver,
      public ScreenOrientationController::Observer {
 public:
  explicit ObserverImpl() {
    display::Screen::GetScreen()->AddObserver(this);
    Shell::Get()->tablet_mode_controller()->AddObserver(this);
    Shell::Get()->screen_orientation_controller()->AddObserver(this);
  }

  ~ObserverImpl() override {
    Shell::Get()->screen_orientation_controller()->RemoveObserver(this);
    Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
    display::Screen::GetScreen()->RemoveObserver(this);
  }

  void AddObserver(
      mojo::PendingAssociatedRemote<mojom::CrosDisplayConfigObserver>
          observer) {
    observers_.Add(mojo::AssociatedRemote<mojom::CrosDisplayConfigObserver>(
        std::move(observer)));
  }

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override {
    NotifyObserversDisplayConfigChanged();
  }

  void OnDisplayRemoved(const display::Display& old_display) override {
    NotifyObserversDisplayConfigChanged();
  }

  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override {
    NotifyObserversDisplayConfigChanged();
  }

  // TabletModeObserver:
  void OnTabletPhysicalStateChanged() override {
    NotifyObserversDisplayConfigChanged();
  }

  // ScreenOrientationController::Observer:
  void OnUserRotationLockChanged() override {
    NotifyObserversDisplayConfigChanged();
  }

 private:
  void NotifyObserversDisplayConfigChanged() {
    for (auto& observer : observers_)
      observer->OnDisplayConfigChanged();
  }

  mojo::AssociatedRemoteSet<mojom::CrosDisplayConfigObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(ObserverImpl);
};

// -----------------------------------------------------------------------------
// CrosDisplayConfig:

CrosDisplayConfig::CrosDisplayConfig()
    : observer_impl_(std::make_unique<ObserverImpl>()) {}

CrosDisplayConfig::~CrosDisplayConfig() = default;

void CrosDisplayConfig::BindReceiver(
    mojo::PendingReceiver<mojom::CrosDisplayConfigController> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void CrosDisplayConfig::AddObserver(
    mojo::PendingAssociatedRemote<mojom::CrosDisplayConfigObserver> observer) {
  observer_impl_->AddObserver(std::move(observer));
}

void CrosDisplayConfig::GetDisplayLayoutInfo(
    GetDisplayLayoutInfoCallback callback) {
  display::DisplayManager* display_manager = GetDisplayManager();

  auto info = mojom::DisplayLayoutInfo::New();
  if (display_manager->IsInUnifiedMode()) {
    info->layout_mode = mojom::DisplayLayoutMode::kUnified;
  } else if (display_manager->IsInMirrorMode()) {
    info->layout_mode = mojom::DisplayLayoutMode::kMirrored;
    info->mirror_source_id =
        base::NumberToString(display_manager->mirroring_source_id());
    info->mirror_destination_ids = std::vector<std::string>();
    for (int64_t id : display_manager->GetMirroringDestinationDisplayIdList())
      info->mirror_destination_ids->emplace_back(base::NumberToString(id));
  } else {
    info->layout_mode = mojom::DisplayLayoutMode::kNormal;
  }

  if (display_manager->IsInUnifiedMode()) {
    info->layouts = GetDisplayUnifiedLayouts();
  } else if (display_manager->num_connected_displays() > 1) {
    info->layouts = GetDisplayLayouts();
  }

  std::move(callback).Run(std::move(info));
}

mojom::DisplayConfigResult SetDisplayLayouts(
    const std::vector<mojom::DisplayLayoutPtr>& layouts) {
  display::DisplayManager* display_manager = GetDisplayManager();
  display::DisplayLayoutBuilder builder(
      display_manager->GetCurrentResolvedDisplayLayout());
  int64_t root_id = display::kInvalidDisplayId;
  std::set<int64_t> layout_ids;
  builder.ClearPlacements();
  for (const mojom::DisplayLayoutPtr& layout_ptr : layouts) {
    const mojom::DisplayLayout& layout = *layout_ptr;
    display::Display display = GetDisplay(layout.id);
    if (display.id() == display::kInvalidDisplayId) {
      LOG(ERROR) << "Display layout has invalid id: " << layout.id;
      return mojom::DisplayConfigResult::kInvalidDisplayIdError;
    }
    display::Display parent = GetDisplay(layout.parent_id);
    if (parent.id() == display::kInvalidDisplayId) {
      if (root_id != display::kInvalidDisplayId) {
        LOG(ERROR) << "Display layout has invalid parent: " << layout.parent_id;
        return mojom::DisplayConfigResult::kInvalidDisplayLayoutError;
      }
      root_id = display.id();
      continue;  // No placement for root (primary) display.
    }
    layout_ids.insert(display.id());
    display::DisplayPlacement::Position position =
        GetDisplayPlacementPosition(layout.position);
    builder.AddDisplayPlacement(display.id(), parent.id(), position,
                                layout.offset);
  }

  const display::DisplayIdList display_ids =
      display_manager->GetCurrentDisplayIdList();
  std::unique_ptr<display::DisplayLayout> layout = builder.Build();
  if (display_manager->IsInUnifiedMode()) {
    if (root_id == display::kInvalidDisplayId) {
      // Look for a display with no layout info to use as the root.
      for (int64_t id : display_ids) {
        if (!base::Contains(layout_ids, id)) {
          root_id = id;
          break;
        }
      }
      if (root_id == display::kInvalidDisplayId) {
        LOG(ERROR) << "Invalid unified layout: No root id";
        return mojom::DisplayConfigResult::kInvalidDisplayLayoutError;
      }
    }
    layout->primary_id = root_id;
    display::UnifiedDesktopLayoutMatrix matrix;
    if (!display::BuildUnifiedDesktopMatrix(display_ids, *layout, &matrix)) {
      LOG(ERROR) << "Invalid unified layout: No proper conversion to a matrix";
      return mojom::DisplayConfigResult::kInvalidDisplayLayoutError;
    }
    Shell::Get()
        ->display_configuration_controller()
        ->SetUnifiedDesktopLayoutMatrix(matrix);
  } else {
    if (!display::DisplayLayout::Validate(display_ids, *layout)) {
      return mojom::DisplayConfigResult::kInvalidDisplayLayoutError;
    }
    Shell::Get()->display_configuration_controller()->SetDisplayLayout(
        std::move(layout));
  }
  return mojom::DisplayConfigResult::kSuccess;
}

void CrosDisplayConfig::SetDisplayLayoutInfo(
    mojom::DisplayLayoutInfoPtr info,
    SetDisplayLayoutInfoCallback callback) {
  mojom::DisplayConfigResult result = SetDisplayLayoutMode(*info);
  if (result != mojom::DisplayConfigResult::kSuccess) {
    std::move(callback).Run(result);
    return;
  }
  if (info->layouts) {
    result = SetDisplayLayouts(*info->layouts);
    if (result != mojom::DisplayConfigResult::kSuccess) {
      std::move(callback).Run(result);
      return;
    }
  }
  std::move(callback).Run(mojom::DisplayConfigResult::kSuccess);
}

void CrosDisplayConfig::GetDisplayUnitInfoList(
    bool single_unified,
    GetDisplayUnitInfoListCallback callback) {
  std::vector<mojom::DisplayUnitInfoPtr> info_list;
  display::DisplayManager* display_manager = GetDisplayManager();

  std::vector<display::Display> displays;
  int64_t primary_id;
  if (!display_manager->IsInUnifiedMode()) {
    displays = display::Screen::GetScreen()->GetAllDisplays();
    primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  } else if (single_unified) {
    for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i)
      displays.push_back(display_manager->GetDisplayAt(i));
    primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  } else {
    displays = display_manager->software_mirroring_display_list();
    primary_id = Shell::Get()
                     ->display_configuration_controller()
                     ->GetPrimaryMirroringDisplayForUnifiedDesktop()
                     .id();
  }

  for (const display::Display& display : displays)
    info_list.emplace_back(GetDisplayUnitInfo(display, primary_id));
  std::move(callback).Run(std::move(info_list));
}

void CrosDisplayConfig::SetDisplayProperties(
    const std::string& id,
    mojom::DisplayConfigPropertiesPtr properties,
    mojom::DisplayConfigSource source,
    SetDisplayPropertiesCallback callback) {
  const display::Display display = GetDisplay(id);
  mojom::DisplayConfigResult result =
      ValidateDisplayProperties(*properties, display);
  if (result != mojom::DisplayConfigResult::kSuccess) {
    std::move(callback).Run(result);
    return;
  }

  display::DisplayManager* display_manager = GetDisplayManager();
  DisplayConfigurationController* display_configuration_controller =
      Shell::Get()->display_configuration_controller();
  const display::Display& primary =
      display::Screen::GetScreen()->GetPrimaryDisplay();

  if (properties->set_primary && display.id() != primary.id()) {
    display_configuration_controller->SetPrimaryDisplayId(
        display.id(), false /* don't throttle */);
  }

  if (properties->overscan)
    display_manager->SetOverscanInsets(display.id(), *properties->overscan);

  if (properties->rotation) {
    const mojom::DisplayRotationOptions rotation_options =
        properties->rotation->rotation;
    const bool is_in_tablet_physical_state =
        Shell::Get()->tablet_mode_controller()->is_in_tablet_physical_state();
    auto* screen_orientation_controller =
        Shell::Get()->screen_orientation_controller();
    const bool auto_rotate_requested =
        rotation_options == mojom::DisplayRotationOptions::kAutoRotate;
    if (auto_rotate_requested && !is_in_tablet_physical_state) {
      LOG(ERROR) << "Auto-rotate is supported only when the device is in "
                 << "physical tablet state. This will be treated as a request "
                 << " to set the display rotation to 0 degrees.";
    }

    display::Display::Rotation rotation =
        DisplayRotationFromRotationOptions(properties->rotation->rotation);
    if (is_in_tablet_physical_state) {
      if (auto_rotate_requested) {
        if (screen_orientation_controller->user_rotation_locked())
          screen_orientation_controller->ToggleUserRotationLock();
      } else {
        screen_orientation_controller->SetLockToRotation(rotation);
      }
    } else {
      display_configuration_controller->SetDisplayRotation(
          display.id(), rotation, display::Display::RotationSource::USER);
    }
  }

  if (properties->bounds_origin &&
      *properties->bounds_origin != display.bounds().origin()) {
    gfx::Rect display_bounds = display.bounds();
    display_bounds.Offset(
        properties->bounds_origin->x() - display.bounds().x(),
        properties->bounds_origin->y() - display.bounds().y());
    SetDisplayLayoutFromBounds(primary.bounds(), primary.id(), display_bounds,
                               display.id());
  }

  if (properties->display_zoom_factor > 0) {
    display_manager->UpdateZoomFactor(display.id(),
                                      properties->display_zoom_factor);
  }

  // Set the display mode. Note: if this returns an error, other properties
  // will have already been applied. TODO(stevenjb): Validate the display mode
  // before applying any properties.
  if (properties->display_mode) {
    result = SetDisplayMode(display.id(), *properties->display_mode, source);
    if (result != mojom::DisplayConfigResult::kSuccess) {
      std::move(callback).Run(result);
      return;
    }
  }

  std::move(callback).Run(mojom::DisplayConfigResult::kSuccess);
}

void CrosDisplayConfig::SetUnifiedDesktopEnabled(bool enabled) {
  GetDisplayManager()->SetUnifiedDesktopEnabled(enabled);
}

void CrosDisplayConfig::OverscanCalibration(
    const std::string& display_id,
    mojom::DisplayConfigOperation op,
    const base::Optional<gfx::Insets>& delta,
    OverscanCalibrationCallback callback) {
  display::Display display = GetDisplay(display_id);
  if (display.id() == display::kInvalidDisplayId) {
    std::move(callback).Run(mojom::DisplayConfigResult::kInvalidDisplayIdError);
    return;
  }

  OverscanCalibrator* calibrator = GetOverscanCalibrator(display_id);
  if (!calibrator && op != mojom::DisplayConfigOperation::kStart) {
    LOG(ERROR) << "Calibrator does not exist for op=" << op;
    std::move(callback).Run(
        mojom::DisplayConfigResult::kCalibrationNotAvailableError);
    return;
  }
  switch (op) {
    case mojom::DisplayConfigOperation::kStart: {
      DVLOG(1) << "OverscanCalibrationStart: " << display_id;
      gfx::Insets insets =
          Shell::Get()->window_tree_host_manager()->GetOverscanInsets(
              display.id());
      if (calibrator)
        DVLOG(1) << "Replacing existing calibrator for id: " << display_id;
      overscan_calibrators_[display_id] =
          std::make_unique<OverscanCalibrator>(display, insets);
      break;
    }
    case mojom::DisplayConfigOperation::kAdjust:
      DVLOG(1) << "OverscanCalibrationAdjust: " << display_id;
      if (!delta) {
        LOG(ERROR) << "Delta not provided for for adjust: " << display_id;
        std::move(callback).Run(
            mojom::DisplayConfigResult::kCalibrationFailedError);
        return;
      }
      calibrator->UpdateInsets(calibrator->insets() + *delta);
      break;
    case mojom::DisplayConfigOperation::kReset:
      DVLOG(1) << "OverscanCalibrationReset: " << display_id;
      calibrator->Reset();
      break;
    case mojom::DisplayConfigOperation::kComplete:
      DVLOG(1) << "OverscanCalibrationComplete: " << display_id;
      calibrator->Commit();
      overscan_calibrators_[display_id].reset();
      break;
    case mojom::DisplayConfigOperation::kShowNative:
      LOG(ERROR) << "Operation not supported: " << op;
      std::move(callback).Run(
          mojom::DisplayConfigResult::kInvalidOperationError);
      return;
      return;
  }
  std::move(callback).Run(mojom::DisplayConfigResult::kSuccess);
}

void CrosDisplayConfig::TouchCalibration(const std::string& display_id,
                                         mojom::DisplayConfigOperation op,
                                         mojom::TouchCalibrationPtr calibration,
                                         TouchCalibrationCallback callback) {
  display::Display display = GetDisplay(display_id);
  if (display.id() == display::kInvalidDisplayId) {
    std::move(callback).Run(mojom::DisplayConfigResult::kInvalidDisplayIdError);
    return;
  }
  if (display.IsInternal()) {
    LOG(ERROR) << "Internal display cannot be calibrated for touch: "
               << display_id;
    std::move(callback).Run(
        mojom::DisplayConfigResult::kCalibrationNotAvailableError);
    return;
  }
  if (!display::HasExternalTouchscreenDevice()) {
    LOG(ERROR)
        << "Touch calibration called with no external touch screen device.";
    std::move(callback).Run(
        mojom::DisplayConfigResult::kCalibrationNotAvailableError);
    return;
  }

  if (op == mojom::DisplayConfigOperation::kStart ||
      op == mojom::DisplayConfigOperation::kShowNative) {
    if (touch_calibrator_ && touch_calibrator_->IsCalibrating()) {
      LOG(ERROR) << "Touch calibration already active.";
      std::move(callback).Run(
          mojom::DisplayConfigResult::kCalibrationInProgressError);
      return;
    }
    if (!touch_calibrator_)
      touch_calibrator_ = std::make_unique<ash::TouchCalibratorController>();
    if (op == mojom::DisplayConfigOperation::kShowNative) {
      // For native calibration, |callback| is not run until calibration
      // completes.
      touch_calibrator_->StartCalibration(
          display, /*is_custom_calibration=*/false,
          base::BindOnce(
              [](TouchCalibrationCallback callback, bool result) {
                std::move(callback).Run(
                    result
                        ? mojom::DisplayConfigResult::kSuccess
                        : mojom::DisplayConfigResult::kCalibrationFailedError);
              },
              std::move(callback)));
      return;
    }
    // For custom calibration, start calibration and run |callback| now.
    touch_calibrator_->StartCalibration(display, /*is_custom_calibration=*/true,
                                        base::OnceCallback<void(bool)>());
    std::move(callback).Run(mojom::DisplayConfigResult::kSuccess);
    return;
  }

  if (op == mojom::DisplayConfigOperation::kReset) {
    ash::Shell::Get()->display_manager()->ClearTouchCalibrationData(
        display.id(), base::nullopt);
    std::move(callback).Run(mojom::DisplayConfigResult::kSuccess);
    return;
  }

  if (op != mojom::DisplayConfigOperation::kComplete) {
    LOG(ERROR) << "Unknown operation: " << op;
    std::move(callback).Run(
        mojom::DisplayConfigResult::kCalibrationNotStartedError);
    return;
  }

  if (!touch_calibrator_) {
    LOG(ERROR) << "Touch calibration not active.";
    std::move(callback).Run(
        mojom::DisplayConfigResult::kCalibrationNotStartedError);
    return;
  }

  if (!calibration || calibration->pairs.size() != 4) {
    LOG(ERROR) << "Touch calibration requires four calibration pairs.";
    std::move(callback).Run(
        mojom::DisplayConfigResult::kCalibrationInvalidDataError);
    return;
  }

  ash::Shell::Get()->touch_transformer_controller()->SetForCalibration(false);

  display::TouchCalibrationData::CalibrationPointPairQuad calibration_points;
  calibration_points[0] = GetCalibrationPair(*calibration->pairs[0]);
  calibration_points[1] = GetCalibrationPair(*calibration->pairs[1]);
  calibration_points[2] = GetCalibrationPair(*calibration->pairs[2]);
  calibration_points[3] = GetCalibrationPair(*calibration->pairs[3]);

  gfx::Size bounds = calibration->bounds;
  for (size_t row = 0; row < calibration_points.size(); row++) {
    // Coordinates for display and touch point cannot be negative.
    if (calibration_points[row].first.x() < 0 ||
        calibration_points[row].first.y() < 0 ||
        calibration_points[row].second.x() < 0 ||
        calibration_points[row].second.y() < 0) {
      LOG(ERROR)
          << "Display points and touch points cannot have negative coordinates";
      touch_calibrator_->StopCalibrationAndResetParams();
      std::move(callback).Run(
          mojom::DisplayConfigResult::kCalibrationInvalidDataError);
      return;
    }
    // Coordinates for display points cannot be greater than the screen
    // bounds.
    if (calibration_points[row].first.x() > bounds.width() ||
        calibration_points[row].first.y() > bounds.height()) {
      LOG(ERROR) << "Display point coordinates cannot be more than size of the "
                    "display.";
      touch_calibrator_->StopCalibrationAndResetParams();
      std::move(callback).Run(
          mojom::DisplayConfigResult::kCalibrationInvalidDataError);
      return;
    }
  }

  touch_calibrator_->CompleteCalibration(calibration_points, bounds);
  std::move(callback).Run(mojom::DisplayConfigResult::kSuccess);
}

OverscanCalibrator* CrosDisplayConfig::GetOverscanCalibrator(
    const std::string& id) {
  auto iter = overscan_calibrators_.find(id);
  return iter == overscan_calibrators_.end() ? nullptr : iter->second.get();
}

}  // namespace ash
