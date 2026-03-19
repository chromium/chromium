// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/cros_display_config.h"

#include <optional>
#include <sstream>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/display/display_alignment_controller.h"
#include "ash/display/display_configuration_controller.h"
#include "ash/display/display_highlight_controller.h"
#include "ash/display/display_prefs.h"
#include "ash/display/overscan_calibrator.h"
#include "ash/display/resolution_notification_controller.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/display/touch_calibrator_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/shell.h"
#include "ash/touch/ash_touch_transform_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/crosapi/mojom/cros_display_config.mojom.h"
#include "components/device_event_log/device_event_log.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/util/display_manager_util.h"
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
  if (!base::StringToInt64(display_id_str, &display_id)) {
    display_id = display::kInvalidDisplayId;
  }
  return display_id;
}

display::Display GetDisplay(int64_t display_id) {
  if (display_id == display::kInvalidDisplayId) {
    return display::Display();
  }
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

// Gets the display with the provided string id.
display::Display GetDisplay(const std::string& display_id_str) {
  return GetDisplay(GetDisplayId(display_id_str));
}

std::vector<display::DisplayPlacement> GetDisplayLayouts() {
  std::vector<display::DisplayPlacement> layouts;
  display::Screen* screen = display::Screen::Get();
  display::DisplayManager* display_manager = GetDisplayManager();
  const std::vector<display::Display>& displays = screen->GetAllDisplays();
  for (const auto& display : displays) {
    const display::DisplayPlacement placement =
        display_manager->GetCurrentResolvedDisplayLayout().FindPlacementById(
            display.id());
    if (placement.display_id != display::kInvalidDisplayId) {
      layouts.push_back(std::move(placement));
    }
  }
  return layouts;
}

std::vector<display::DisplayPlacement> GetDisplayUnifiedLayouts() {
  std::vector<display::DisplayPlacement> layouts;
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
      display::DisplayPlacement layout;
      const int64_t display_id = row[column_index];
      // Parent display is either the one in the above row, or the one on the
      // left in the same row.
      const int64_t parent_id = column_index == 0
                                    ? matrix[row_index - 1][column_index]
                                    : row[column_index - 1];
      layout.display_id = display_id;
      layout.parent_display_id = parent_id;
      layout.position = column_index == 0 ? display::DisplayPlacement::BOTTOM
                                          : display::DisplayPlacement::RIGHT;
      layout.offset = 0;
      layouts.push_back(std::move(layout));
    }
  }
  return layouts;
}

DisplayConfigResult SetDisplayLayoutMode(const DisplayLayoutInfo& info) {
  display::DisplayManager* display_manager = GetDisplayManager();
  if (display_manager->num_connected_displays() < 2) {
    return DisplayConfigResult::kSingleDisplayError;
  }

  if (info.layout_mode == DisplayLayoutMode::kNormal) {
    display_manager->SetDefaultMultiDisplayModeForCurrentDisplays(
        display::DisplayManager::EXTENDED);
    display_manager->SetMirrorMode(display::MirrorMode::kOff, std::nullopt);
    return DisplayConfigResult::kSuccess;
  }

  if (info.layout_mode == DisplayLayoutMode::kUnified) {
    if (!display_manager->unified_desktop_enabled()) {
      return DisplayConfigResult::kUnifiedNotEnabledError;
    }
    display_manager->SetDefaultMultiDisplayModeForCurrentDisplays(
        display::DisplayManager::UNIFIED);
    display_manager->SetMirrorMode(display::MirrorMode::kOff, std::nullopt);
    return DisplayConfigResult::kSuccess;
  }

  CHECK_EQ(info.layout_mode, DisplayLayoutMode::kMirrored);

  if (!info.mirror_source_id.has_value()) {
    // 'Normal' mirror mode.
    display_manager->SetMirrorMode(display::MirrorMode::kNormal, std::nullopt);
    return DisplayConfigResult::kSuccess;
  }

  // 'Mixed' mirror mode.
  display::Display source = GetDisplay(*info.mirror_source_id);
  if (source.id() == display::kInvalidDisplayId) {
    return DisplayConfigResult::kMirrorModeSourceIdError;
  }
  display::DisplayIdList destination_ids;
  if (info.mirror_destination_ids.has_value()) {
    for (int64_t destination_id : *info.mirror_destination_ids) {
      if (destination_id == display::kInvalidDisplayId) {
        return DisplayConfigResult::kMirrorModeDestIdError;
      }
      destination_ids.emplace_back(destination_id);
    }
  } else {
    const std::vector<display::Display>& displays =
        display::Screen::Get()->GetAllDisplays();
    for (const display::Display& display : displays) {
      destination_ids.emplace_back(display.id());
    }
  }
  std::optional<display::MixedMirrorModeParams> mixed_params(
      std::in_place, source.id(), destination_ids);
  const display::MixedMirrorModeParamsErrors error_type =
      display::ValidateParamsForMixedMirrorMode(
          display_manager->GetConnectedDisplayIdList(), *mixed_params);
  switch (error_type) {
    case display::MixedMirrorModeParamsErrors::kErrorSingleDisplay:
      return DisplayConfigResult::kSingleDisplayError;
    case display::MixedMirrorModeParamsErrors::kErrorSourceIdNotFound:
      return DisplayConfigResult::kMirrorModeSourceIdError;
    case display::MixedMirrorModeParamsErrors::kErrorDestinationIdsEmpty:
    case display::MixedMirrorModeParamsErrors::kErrorDestinationIdNotFound:
    case display::MixedMirrorModeParamsErrors::kErrorDuplicateId:
      return DisplayConfigResult::kMirrorModeDestIdError;
    case display::MixedMirrorModeParamsErrors::kSuccess:
      break;
  }
  display_manager->SetMirrorMode(display::MirrorMode::kMixed, mixed_params);
  return DisplayConfigResult::kSuccess;
}

display::Display::Rotation DisplayRotationFromRotationOptions(
    crosapi::mojom::DisplayRotationOptions option) {
  switch (option) {
    case crosapi::mojom::DisplayRotationOptions::kAutoRotate:
      return display::Display::ROTATE_0;

    case crosapi::mojom::DisplayRotationOptions::kZeroDegrees:
      return display::Display::ROTATE_0;

    case crosapi::mojom::DisplayRotationOptions::k90Degrees:
      return display::Display::ROTATE_90;

    case crosapi::mojom::DisplayRotationOptions::k180Degrees:
      return display::Display::ROTATE_180;

    case crosapi::mojom::DisplayRotationOptions::k270Degrees:
      return display::Display::ROTATE_270;
  }
}

crosapi::mojom::DisplayRotationOptions RotationOptionsFromDisplayRotation(
    display::Display::Rotation rotation,
    bool is_internal) {
  auto* screen_orientation_controller =
      Shell::Get()->screen_orientation_controller();
  const bool is_auto_rotation_allowed =
      screen_orientation_controller->IsAutoRotationAllowed();
  const bool is_auto_rotate_enabled =
      !screen_orientation_controller->user_rotation_locked();
  if (is_auto_rotation_allowed && is_auto_rotate_enabled && is_internal) {
    return crosapi::mojom::DisplayRotationOptions::kAutoRotate;
  }

  switch (rotation) {
    case display::Display::ROTATE_0:
      return crosapi::mojom::DisplayRotationOptions::kZeroDegrees;

    case display::Display::ROTATE_90:
      return crosapi::mojom::DisplayRotationOptions::k90Degrees;

    case display::Display::ROTATE_180:
      return crosapi::mojom::DisplayRotationOptions::k180Degrees;

    case display::Display::ROTATE_270:
      return crosapi::mojom::DisplayRotationOptions::k270Degrees;
  }
}

DisplayUnitInfo GetDisplayUnitInfo(const display::Display& display,
                                   int64_t primary_id) {
  display::DisplayManager* display_manager = GetDisplayManager();
  const display::ManagedDisplayInfo& display_info =
      display_manager->GetDisplayInfo(display.id());

  DisplayUnitInfo info;
  info.id = display.id();
  info.name = display_manager->GetDisplayNameForId(display.id());

  if (!display_info.manufacturer_id().empty() ||
      !display_info.product_id().empty() ||
      (display_info.year_of_manufacture() !=
       display::kInvalidYearOfManufacture)) {
    info.edid.emplace();
    info.edid->manufacturer_id = display_info.manufacturer_id();
    info.edid->product_id = display_info.product_id();
    info.edid->year_of_manufacture = display_info.year_of_manufacture();
  }

  info.is_primary = display.id() == primary_id;
  info.is_internal = display.IsInternal();
  info.is_enabled = true;
  info.is_detected = display.detected();
  info.is_auto_rotation_allowed =
      Shell::Get()->screen_orientation_controller()->IsAutoRotationAllowed() &&
      display.IsInternal();
  const bool has_accelerometer_support =
      display.accelerometer_support() ==
      display::Display::AccelerometerSupport::AVAILABLE;
  info.has_touch_support =
      display.touch_support() == display::Display::TouchSupport::AVAILABLE;
  info.has_accelerometer_support = has_accelerometer_support;

  const float device_dpi = display_info.device_dpi();
  info.dpi_x = device_dpi * display.size().width() /
               display_info.bounds_in_native().width();
  info.dpi_y = device_dpi * display.size().height() /
               display_info.bounds_in_native().height();

  info.rotation_options = RotationOptionsFromDisplayRotation(
      display.rotation(), display.IsInternal());
  info.bounds = display.bounds();
  info.overscan = display_manager->GetOverscanInsets(display.id());
  info.work_area = display.work_area();

  int display_mode_index = 0;
  display::ManagedDisplayMode active_mode;
  bool has_active_mode = display_manager->GetActiveModeForDisplayId(
      display_info.id(), &active_mode);
  for (const display::ManagedDisplayMode& display_mode :
       display_info.display_modes()) {
    info.available_display_modes.push_back(display_mode);
    if (has_active_mode && display_mode.IsEquivalent(active_mode)) {
      info.selected_display_mode_index = display_mode_index;
    }
    ++display_mode_index;
  }

  info.display_zoom_factor = display_info.zoom_factor();
  if (has_active_mode) {
    auto zoom_levels = display::GetDisplayZoomFactors(active_mode);
    info.available_display_zoom_factors.reserve(zoom_levels.size());
    info.available_display_zoom_factors.assign(zoom_levels.begin(),
                                               zoom_levels.end());
  } else {
    info.available_display_zoom_factors.push_back(display_info.zoom_factor());
  }

  return info;
}

// Validates that DisplayProperties are valid with the current DisplayManager
// configuration. Returns an error on failure.
DisplayConfigResult ValidateDisplayProperties(
    const DisplayConfigProperties& properties,
    const display::Display& display) {
  display::DisplayManager* display_manager = GetDisplayManager();

  auto dump_state = [&display, &properties]() -> std::string {
    std::stringstream ss;
    ss << "display={" << display.ToString() << "}";
    ss << ", config properties={";
    if (properties.overscan.has_value()) {
      ss << "overscan=" << properties.overscan->ToString() << ", ";
    }
    if (properties.bounds_origin.has_value()) {
      ss << "bounds_origin=" << properties.bounds_origin->ToString() << ", ";
    }
    ss << "zoom_factor=" << properties.display_zoom_factor;
    return ss.str() + "}";
  };

  int64_t id = display.id();
  if (id == display::kInvalidDisplayId) {
    DISPLAY_LOG(ERROR) << "Invalid display id:" << dump_state();
    return DisplayConfigResult::kInvalidDisplayIdError;
  }

  // Overscan cannot be changed for the internal display, and should be at most
  // half of the screen size.
  if (properties.overscan) {
    if (display.IsInternal()) {
      DISPLAY_LOG(ERROR) << "Overscan is not supported on the internal display:"
                         << dump_state();
      return DisplayConfigResult::kNotSupportedOnInternalDisplayError;
    }

    if (properties.overscan->left() < 0 || properties.overscan->top() < 0 ||
        properties.overscan->right() < 0 || properties.overscan->bottom() < 0) {
      DISPLAY_LOG(ERROR) << "Negative overscan:" << dump_state();
      return DisplayConfigResult::kPropertyValueOutOfRangeError;
    }

    const gfx::Insets overscan = display_manager->GetOverscanInsets(id);
    int screen_width = display.bounds().width() + overscan.width();
    int screen_height = display.bounds().height() + overscan.height();

    if ((properties.overscan->left() + properties.overscan->right()) * 2 >
            screen_width ||
        (properties.overscan->top() + properties.overscan->bottom()) * 2 >
            screen_height) {
      DISPLAY_LOG(ERROR) << "Invalid Overscan: " << dump_state()
                         << ", overscan (" << properties.overscan->ToString()
                         << ") exceeds bounds (" << screen_width << "x"
                         << screen_height << ")";
      return DisplayConfigResult::kPropertyValueOutOfRangeError;
    }
  }

  // The bounds cannot be changed for the primary display and should be inside
  // a reasonable bounds.
  if (properties.bounds_origin) {
    const display::Display& primary =
        display::Screen::Get()->GetPrimaryDisplay();
    if (id == primary.id() || properties.set_primary) {
      LOG(ERROR) << "Not Supported on Internal Display:" << dump_state();
      return DisplayConfigResult::kNotSupportedOnInternalDisplayError;
    }
    if (properties.bounds_origin->x() > kMaxBoundsOrigin ||
        properties.bounds_origin->x() < -kMaxBoundsOrigin ||
        properties.bounds_origin->y() > kMaxBoundsOrigin ||
        properties.bounds_origin->y() < -kMaxBoundsOrigin) {
      DISPLAY_LOG(ERROR) << "Bounds origin out of range:" << dump_state();
      return DisplayConfigResult::kPropertyValueOutOfRangeError;
    }
  }

  // In Unified mode, the actual zoom factor will be picked by the system.
  if (properties.display_zoom_factor >
      0) {  // && !display_manager->IsInUnifiedMode()) {
    display::ManagedDisplayMode current_mode;
    if (!display_manager->GetActiveModeForDisplayId(id, &current_mode)) {
      DISPLAY_LOG(ERROR) << "No active mode for display:" << dump_state();
      return DisplayConfigResult::kInvalidDisplayIdError;
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
      DISPLAY_LOG(ERROR) << "Display zoom factor out of range:" << dump_state();
      return DisplayConfigResult::kPropertyValueOutOfRangeError;
    }
  }

  return DisplayConfigResult::kSuccess;
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
DisplayConfigResult SetDisplayMode(
    int64_t id,
    const display::ManagedDisplayMode& display_mode,
    crosapi::mojom::DisplayConfigSource source) {
  display::DisplayManager* display_manager = GetDisplayManager();

  display::ManagedDisplayMode current_mode;
  if (!display_manager->GetActiveModeForDisplayId(id, &current_mode)) {
    return DisplayConfigResult::kInvalidDisplayIdError;
  }

  if (!display_mode.IsEquivalent(current_mode)) {
    // For the internal display, the display mode will be applied directly.
    // Otherwise a confirm/revert notification will be prepared first, and the
    // display mode will be applied. If the user accepts the mode change by
    // dismissing the notification, MaybeStoreDisplayPrefs() will be called back
    // to persist the new preferences.
    if (!Shell::Get()
             ->resolution_notification_controller()
             ->PrepareNotificationAndSetDisplayMode(
                 id, current_mode, display_mode, source, base::BindOnce([]() {
                   Shell::Get()->display_prefs()->MaybeStoreDisplayPrefs();
                 }))) {
      return DisplayConfigResult::kSetDisplayModeError;
    }
  }

  return DisplayConfigResult::kSuccess;
}

}  // namespace

DisplayLayoutInfo::DisplayLayoutInfo() = default;
DisplayLayoutInfo::DisplayLayoutInfo(const DisplayLayoutInfo& other) = default;
DisplayLayoutInfo::DisplayLayoutInfo(DisplayLayoutInfo&& other) noexcept =
    default;
DisplayLayoutInfo& DisplayLayoutInfo::operator=(
    const DisplayLayoutInfo& other) = default;
DisplayLayoutInfo& DisplayLayoutInfo::operator=(
    DisplayLayoutInfo&& other) noexcept = default;
DisplayLayoutInfo::~DisplayLayoutInfo() = default;

DisplayConfigProperties::DisplayConfigProperties() = default;
DisplayConfigProperties::DisplayConfigProperties(
    const DisplayConfigProperties& other) = default;
DisplayConfigProperties::DisplayConfigProperties(
    DisplayConfigProperties&& other) noexcept = default;
DisplayConfigProperties& DisplayConfigProperties::operator=(
    const DisplayConfigProperties& other) = default;
DisplayConfigProperties& DisplayConfigProperties::operator=(
    DisplayConfigProperties&& other) noexcept = default;
DisplayConfigProperties::~DisplayConfigProperties() = default;

DisplayUnitInfo::DisplayUnitInfo() = default;
DisplayUnitInfo::DisplayUnitInfo(const DisplayUnitInfo& other) = default;
DisplayUnitInfo::DisplayUnitInfo(DisplayUnitInfo&& other) noexcept = default;
DisplayUnitInfo& DisplayUnitInfo::operator=(const DisplayUnitInfo& other) =
    default;
DisplayUnitInfo& DisplayUnitInfo::operator=(DisplayUnitInfo&& other) noexcept =
    default;
DisplayUnitInfo::~DisplayUnitInfo() = default;

// -----------------------------------------------------------------------------
// CrosDisplayConfigImpl::ObserverImpl:

// Observes display and tablet mode events, and notifies each
// CrosDisplayConfig::Observer with OnDisplayConfigChanged() in response to
// those events.
class CrosDisplayConfigImpl::ObserverImpl
    : public display::DisplayObserver,
      public TabletModeObserver,
      public ScreenOrientationController::Observer {
 public:
  ObserverImpl() {
    Shell::Get()->tablet_mode_controller()->AddObserver(this);
    Shell::Get()->screen_orientation_controller()->AddObserver(this);
  }

  ObserverImpl(const ObserverImpl&) = delete;
  ObserverImpl& operator=(const ObserverImpl&) = delete;

  ~ObserverImpl() override {
    Shell::Get()->screen_orientation_controller()->RemoveObserver(this);
    Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  }

  void AddObserver(CrosDisplayConfig::Observer* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(CrosDisplayConfig::Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override {
    NotifyObserversDisplayConfigChanged();
  }

  void OnDisplaysRemoved(const display::Displays& removed_displays) override {
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
    observers_.Notify(&CrosDisplayConfig::Observer::OnDisplayConfigChanged);
  }

  base::ObserverList<CrosDisplayConfig::Observer> observers_;
  display::ScopedDisplayObserver display_observer_{this};
};

// -----------------------------------------------------------------------------
// CrosDisplayConfigImpl:

CrosDisplayConfigImpl::CrosDisplayConfigImpl()
    : observer_impl_(std::make_unique<ObserverImpl>()) {}

CrosDisplayConfigImpl::~CrosDisplayConfigImpl() = default;

void CrosDisplayConfigImpl::AddObserver(CrosDisplayConfig::Observer* observer) {
  observer_impl_->AddObserver(observer);
}

void CrosDisplayConfigImpl::RemoveObserver(
    CrosDisplayConfig::Observer* observer) {
  observer_impl_->RemoveObserver(observer);
}

DisplayLayoutInfo CrosDisplayConfigImpl::GetDisplayLayoutInfo() {
  display::DisplayManager* display_manager = GetDisplayManager();
  DisplayLayoutInfo info;
  if (display_manager->IsInUnifiedMode()) {
    info.layout_mode = DisplayLayoutMode::kUnified;
  } else if (display_manager->IsInMirrorMode()) {
    info.layout_mode = DisplayLayoutMode::kMirrored;
    info.mirror_source_id = display_manager->mirroring_source_id();
    info.mirror_destination_ids.emplace();
    for (int64_t id : display_manager->GetMirroringDestinationDisplayIdList()) {
      info.mirror_destination_ids->push_back(id);
    }
  } else {
    info.layout_mode = DisplayLayoutMode::kNormal;
  }
  if (display_manager->IsInUnifiedMode()) {
    info.layouts = GetDisplayUnifiedLayouts();
  } else if (display_manager->num_connected_displays() > 1) {
    info.layouts = GetDisplayLayouts();
  }
  return info;
}

DisplayConfigResult SetDisplayLayouts(
    const std::vector<display::DisplayPlacement>& layouts) {
  display::DisplayManager* display_manager = GetDisplayManager();
  display::DisplayLayoutBuilder builder(
      display_manager->GetCurrentResolvedDisplayLayout());
  int64_t root_id = display::kInvalidDisplayId;
  std::set<int64_t> layout_ids;
  builder.ClearPlacements();
  for (const auto& layout : layouts) {
    display::Display display = GetDisplay(layout.display_id);
    if (display.id() == display::kInvalidDisplayId) {
      DISPLAY_LOG(ERROR) << "Display layout has invalid id: "
                         << layout.display_id;
      return DisplayConfigResult::kInvalidDisplayIdError;
    }
    display::Display parent = GetDisplay(layout.parent_display_id);
    if (parent.id() == display::kInvalidDisplayId) {
      if (root_id != display::kInvalidDisplayId) {
        DISPLAY_LOG(ERROR) << "Display layout has invalid parent: "
                           << layout.parent_display_id;
        return DisplayConfigResult::kInvalidDisplayLayoutError;
      }
      root_id = display.id();
      continue;  // No placement for root (primary) display.
    }
    layout_ids.insert(display.id());
    builder.AddDisplayPlacement(display.id(), parent.id(), layout.position,
                                layout.offset);
  }

  const display::DisplayIdList display_ids =
      display_manager->GetConnectedDisplayIdList();
  std::unique_ptr<display::DisplayLayout> layout = builder.Build();
  if (display_manager->IsInUnifiedMode()) {
    if (root_id == display::kInvalidDisplayId) {
      // Look for a display with no layout info to use as the root.
      for (int64_t id : display_ids) {
        if (!layout_ids.contains(id)) {
          root_id = id;
          break;
        }
      }
      if (root_id == display::kInvalidDisplayId) {
        DISPLAY_LOG(ERROR) << "Invalid unified layout: No root display id";
        return DisplayConfigResult::kInvalidDisplayLayoutError;
      }
    }
    layout->primary_id = root_id;
    display::UnifiedDesktopLayoutMatrix matrix;
    if (!display::BuildUnifiedDesktopMatrix(display_ids, *layout, &matrix)) {
      DISPLAY_LOG(ERROR)
          << "Invalid unified layout: No proper conversion to a matrix";
      return DisplayConfigResult::kInvalidDisplayLayoutError;
    }
    Shell::Get()
        ->display_configuration_controller()
        ->SetUnifiedDesktopLayoutMatrix(matrix);
  } else {
    if (!display::DisplayLayout::Validate(display_ids, *layout)) {
      // No need to log an error since `Validate` already logged what's wrong.
      return DisplayConfigResult::kInvalidDisplayLayoutError;
    }
    Shell::Get()->display_configuration_controller()->SetDisplayLayout(
        std::move(layout));
  }
  return DisplayConfigResult::kSuccess;
}

DisplayConfigResult CrosDisplayConfigImpl::SetDisplayLayoutInfo(
    const DisplayLayoutInfo& info) {
  DisplayConfigResult result = SetDisplayLayoutMode(info);
  if (result != DisplayConfigResult::kSuccess) {
    return result;
  }
  if (info.layouts) {
    result = SetDisplayLayouts(*info.layouts);
    if (result != DisplayConfigResult::kSuccess) {
      return result;
    }
  }
  return DisplayConfigResult::kSuccess;
}

std::vector<DisplayUnitInfo> CrosDisplayConfigImpl::GetDisplayUnitInfoList(
    bool single_unified) {
  std::vector<DisplayUnitInfo> info_list;
  display::DisplayManager* display_manager = GetDisplayManager();

  std::vector<display::Display> displays;
  int64_t primary_id;
  if (!display_manager->IsInUnifiedMode()) {
    displays = display::Screen::Get()->GetAllDisplays();
    primary_id = display::Screen::Get()->GetPrimaryDisplay().id();
  } else if (single_unified) {
    for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
      displays.push_back(display_manager->GetDisplayAt(i));
    }
    primary_id = display::Screen::Get()->GetPrimaryDisplay().id();
  } else {
    displays = display_manager->software_mirroring_display_list();
    primary_id = Shell::Get()
                     ->display_configuration_controller()
                     ->GetPrimaryMirroringDisplayForUnifiedDesktop()
                     .id();
  }

  for (const display::Display& display : displays) {
    info_list.emplace_back(GetDisplayUnitInfo(display, primary_id));
  }
  return info_list;
}

DisplayConfigResult CrosDisplayConfigImpl::SetDisplayProperties(
    const std::string& id,
    const DisplayConfigProperties& properties,
    crosapi::mojom::DisplayConfigSource source) {
  const display::Display display = GetDisplay(id);
  DisplayConfigResult result = ValidateDisplayProperties(properties, display);
  if (result != DisplayConfigResult::kSuccess) {
    return result;
  }

  display::DisplayManager* display_manager = GetDisplayManager();
  DisplayConfigurationController* display_configuration_controller =
      Shell::Get()->display_configuration_controller();
  const display::Display& primary = display::Screen::Get()->GetPrimaryDisplay();

  if (properties.set_primary && display.id() != primary.id()) {
    display_configuration_controller->SetPrimaryDisplayId(
        display.id(), false /* don't throttle */);
  }

  if (properties.overscan.has_value()) {
    display_manager->SetOverscanInsets(display.id(), *properties.overscan);
  }

  if (properties.rotation.has_value()) {
    const crosapi::mojom::DisplayRotationOptions rotation_options =
        *properties.rotation;
    auto* screen_orientation_controller =
        Shell::Get()->screen_orientation_controller();
    const bool is_auto_rotation_allowed =
        screen_orientation_controller->IsAutoRotationAllowed();
    const bool auto_rotate_requested =
        rotation_options == crosapi::mojom::DisplayRotationOptions::kAutoRotate;
    display::Display::Rotation rotation =
        DisplayRotationFromRotationOptions(rotation_options);

    if (is_auto_rotation_allowed && display.IsInternal()) {
      if (auto_rotate_requested) {
        if (screen_orientation_controller->user_rotation_locked()) {
          screen_orientation_controller->ToggleUserRotationLock();
        }
      } else {
        screen_orientation_controller->SetLockToRotation(rotation);
      }
    } else {
      display_configuration_controller->SetDisplayRotation(
          display.id(), rotation, display::Display::RotationSource::USER);
    }
  }

  if (properties.bounds_origin.has_value() &&
      *properties.bounds_origin != display.bounds().origin()) {
    gfx::Rect display_bounds = display.bounds();
    display_bounds.Offset(properties.bounds_origin->x() - display.bounds().x(),
                          properties.bounds_origin->y() - display.bounds().y());
    SetDisplayLayoutFromBounds(primary.bounds(), primary.id(), display_bounds,
                               display.id());
  }

  if (properties.display_zoom_factor > 0) {
    display_manager->UpdateZoomFactor(display.id(),
                                      properties.display_zoom_factor);
  }

  // Set the display mode. Note: if this returns an error, other properties
  // will have already been applied. TODO(stevenjb): Validate the display mode
  // before applying any properties.
  if (properties.display_mode.has_value()) {
    result = SetDisplayMode(display.id(), *properties.display_mode, source);
    if (result != DisplayConfigResult::kSuccess) {
      return result;
    }
  }

  return DisplayConfigResult::kSuccess;
}

void CrosDisplayConfigImpl::SetUnifiedDesktopEnabled(bool enabled) {
  GetDisplayManager()->SetUnifiedDesktopEnabled(enabled);
}

DisplayConfigResult CrosDisplayConfigImpl::OverscanCalibration(
    const std::string& display_id,
    crosapi::mojom::DisplayConfigOperation op,
    const std::optional<gfx::Insets>& delta) {
  display::Display display = GetDisplay(display_id);
  if (display.id() == display::kInvalidDisplayId) {
    return DisplayConfigResult::kInvalidDisplayIdError;
  }

  OverscanCalibrator* calibrator = GetOverscanCalibrator(display_id);
  if (!calibrator && op != crosapi::mojom::DisplayConfigOperation::kStart) {
    DISPLAY_LOG(ERROR) << "Calibrator does not exist for op=" << op;
    return DisplayConfigResult::kCalibrationNotAvailableError;
  }
  switch (op) {
    case crosapi::mojom::DisplayConfigOperation::kStart: {
      DVLOG(1) << "OverscanCalibrationStart: " << display_id;
      gfx::Insets insets =
          Shell::Get()->display_manager()->GetOverscanInsets(display.id());
      if (calibrator) {
        DVLOG(1) << "Replacing existing calibrator for id: " << display_id;
      }
      overscan_calibrators_[display_id] =
          std::make_unique<OverscanCalibrator>(display, insets);
      break;
    }
    case crosapi::mojom::DisplayConfigOperation::kAdjust:
      DVLOG(1) << "OverscanCalibrationAdjust: " << display_id;
      if (!delta) {
        DISPLAY_LOG(ERROR) << "Delta not provided for for adjust: "
                           << display_id;
        return DisplayConfigResult::kCalibrationFailedError;
      }
      calibrator->UpdateInsets(calibrator->insets() + *delta);
      break;
    case crosapi::mojom::DisplayConfigOperation::kReset:
      DVLOG(1) << "OverscanCalibrationReset: " << display_id;
      calibrator->Reset();
      break;
    case crosapi::mojom::DisplayConfigOperation::kComplete:
      DVLOG(1) << "OverscanCalibrationComplete: " << display_id;
      calibrator->Commit();
      overscan_calibrators_[display_id].reset();
      break;
    case crosapi::mojom::DisplayConfigOperation::kShowNative:
      DISPLAY_LOG(ERROR) << "Operation not supported: " << op;
      return DisplayConfigResult::kInvalidOperationError;
    case crosapi::mojom::DisplayConfigOperation::kShowNativeMappingDisplays:
      DISPLAY_LOG(ERROR) << "Operation not supported: " << op;
      return DisplayConfigResult::kInvalidOperationError;
  }
  return DisplayConfigResult::kSuccess;
}

void CrosDisplayConfigImpl::TouchCalibration(
    const std::string& display_id,
    crosapi::mojom::DisplayConfigOperation op,
    base::optional_ref<const display::TouchCalibrationData> calibration,
    TouchCalibrationCallback callback) {
  // For native touch display mapping.
  if (op ==
      crosapi::mojom::DisplayConfigOperation::kShowNativeMappingDisplays) {
    if (touch_calibrator_ && touch_calibrator_->IsCalibrating()) {
      DISPLAY_LOG(ERROR) << "Touch calibration already active.";
      std::move(callback).Run(DisplayConfigResult::kCalibrationInProgressError);
      return;
    }
    if (!touch_calibrator_) {
      touch_calibrator_ = std::make_unique<TouchCalibratorController>();
    }
    // For native calibration, |callback| is not run until calibration
    // completes.
    touch_calibrator_->StartNativeTouchscreenMappingExperience(base::BindOnce(
        [](TouchCalibrationCallback callback, bool result) {
          std::move(callback).Run(
              result ? DisplayConfigResult::kSuccess
                     : DisplayConfigResult::kCalibrationFailedError);
        },
        std::move(callback)));
    return;
  }

  display::Display display = GetDisplay(display_id);
  if (display.id() == display::kInvalidDisplayId) {
    std::move(callback).Run(DisplayConfigResult::kInvalidDisplayIdError);
    return;
  }
  if (display.IsInternal()) {
    DISPLAY_LOG(ERROR) << "Internal display cannot be calibrated for touch: "
                       << display_id;
    std::move(callback).Run(DisplayConfigResult::kCalibrationNotAvailableError);
    return;
  }
  if (!display::HasExternalTouchscreenDevice()) {
    DISPLAY_LOG(ERROR)
        << "Touch calibration called with no external touch screen device.";
    std::move(callback).Run(DisplayConfigResult::kCalibrationNotAvailableError);
    return;
  }

  if (op == crosapi::mojom::DisplayConfigOperation::kStart ||
      op == crosapi::mojom::DisplayConfigOperation::kShowNative) {
    if (touch_calibrator_ && touch_calibrator_->IsCalibrating()) {
      DISPLAY_LOG(ERROR) << "Touch calibration already active.";
      std::move(callback).Run(DisplayConfigResult::kCalibrationInProgressError);
      return;
    }
    if (!touch_calibrator_) {
      touch_calibrator_ = std::make_unique<TouchCalibratorController>();
    }
    if (op == crosapi::mojom::DisplayConfigOperation::kShowNative) {
      // For native calibration, |callback| is not run until calibration
      // completes.
      touch_calibrator_->StartCalibration(
          display, /*is_custom_calibration=*/false,
          base::BindOnce(
              [](TouchCalibrationCallback callback, bool result) {
                std::move(callback).Run(
                    result ? DisplayConfigResult::kSuccess
                           : DisplayConfigResult::kCalibrationFailedError);
              },
              std::move(callback)));
      return;
    }
    // For custom calibration, start calibration and run |callback| now.
    touch_calibrator_->StartCalibration(display, /*is_custom_calibration=*/true,
                                        base::OnceCallback<void(bool)>());
    std::move(callback).Run(DisplayConfigResult::kSuccess);
    return;
  }

  if (op == crosapi::mojom::DisplayConfigOperation::kReset) {
    Shell::Get()->display_manager()->ClearTouchCalibrationData(display.id(),
                                                               std::nullopt);
    std::move(callback).Run(DisplayConfigResult::kSuccess);
    return;
  }

  if (op != crosapi::mojom::DisplayConfigOperation::kComplete) {
    DISPLAY_LOG(ERROR) << "Unknown operation: " << op;
    std::move(callback).Run(DisplayConfigResult::kCalibrationNotStartedError);
    return;
  }

  if (!touch_calibrator_) {
    DISPLAY_LOG(ERROR) << "Touch calibration not active.";
    std::move(callback).Run(DisplayConfigResult::kCalibrationNotStartedError);
    return;
  }

  if (!calibration.has_value()) {
    DISPLAY_LOG(ERROR) << "Touch calibration requires calibration data.";
    std::move(callback).Run(DisplayConfigResult::kCalibrationInvalidDataError);
    return;
  }

  Shell::Get()->touch_transformer_controller()->SetForCalibration(false);

  const gfx::Size bounds = calibration->bounds;
  for (auto& calibration_point : calibration->point_pairs) {
    // Coordinates for display and touch point cannot be negative.
    if (calibration_point.first.x() < 0 || calibration_point.first.y() < 0 ||
        calibration_point.second.x() < 0 || calibration_point.second.y() < 0) {
      DISPLAY_LOG(ERROR)
          << "Display points and touch points cannot have negative coordinates";
      touch_calibrator_->StopCalibrationAndResetParams();
      std::move(callback).Run(
          DisplayConfigResult::kCalibrationInvalidDataError);
      return;
    }
    // Coordinates for display points cannot be greater than the screen
    // bounds.
    if (calibration_point.first.x() > bounds.width() ||
        calibration_point.first.y() > bounds.height()) {
      DISPLAY_LOG(ERROR)
          << "Display point coordinates cannot be more than size of the "
             "display.";
      touch_calibrator_->StopCalibrationAndResetParams();
      std::move(callback).Run(
          DisplayConfigResult::kCalibrationInvalidDataError);
      return;
    }
  }

  touch_calibrator_->CompleteCalibration(calibration->point_pairs, bounds);
  std::move(callback).Run(DisplayConfigResult::kSuccess);
}

OverscanCalibrator* CrosDisplayConfigImpl::GetOverscanCalibrator(
    const std::string& id) {
  auto iter = overscan_calibrators_.find(id);
  return iter == overscan_calibrators_.end() ? nullptr : iter->second.get();
}

void CrosDisplayConfigImpl::HighlightDisplay(int64_t display_id) {
  Shell::Get()->display_highlight_controller()->SetHighlightedDisplay(
      display_id);
}

void CrosDisplayConfigImpl::DragDisplayDelta(int64_t display_id,
                                             int32_t delta_x,
                                             int32_t delta_y) {
  DCHECK(features::IsDisplayAlignmentAssistanceEnabled());
  Shell::Get()->display_alignment_controller()->DisplayDragged(
      display_id, delta_x, delta_y);
}

bool CrosDisplayConfigImpl::IsCalibrating() const {
  return touch_calibrator_ && touch_calibrator_->IsCalibrating();
}

}  // namespace ash
