// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_KEYBOARD_BRIGHTNESS_KEYBOARD_BACKLIGHT_COLOR_CONTROLLER_H_
#define ASH_SYSTEM_KEYBOARD_BRIGHTNESS_KEYBOARD_BACKLIGHT_COLOR_CONTROLLER_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/rgb_keyboard/rgb_keyboard_manager.h"
#include "ash/rgb_keyboard/rgb_keyboard_manager_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "base/scoped_observation.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "third_party/skia/include/core/SkColor.h"

class PrefRegistrySimple;

namespace ash {

class KeyboardBacklightColorNudgeController;

// Controller to manage keyboard backlight colors.
class ASH_EXPORT KeyboardBacklightColorController
    : public RgbKeyboardManagerObserver,
      public SessionObserver,
      public WallpaperControllerObserver {
 public:
  // Used to indicate which display type is being set on the keyboard.
  // TODO(b/266588717): Add a new value for rainbow color option.
  enum class DisplayType {
    kStatic = 0,
    kMultiZone = 1,
    kMaxValue = kMultiZone,
  };

  // Default brightness to be set by the `KeyboardBacklightColorController` when
  // the backlight is off and the user configures a new color.
  static constexpr double kDefaultBacklightBrightness = 40.0;

  explicit KeyboardBacklightColorController(PrefService* local_state);

  KeyboardBacklightColorController(const KeyboardBacklightColorController&) =
      delete;
  KeyboardBacklightColorController& operator=(
      const KeyboardBacklightColorController&) = delete;

  ~KeyboardBacklightColorController() override;

  // Register the pref to store keyboard color in the given registry.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Sets the keyboard backlight color for the user with |account_id|. This call
  // also invokes |UpdateAllBacklightZoneColors| to populate the color for all
  // the zones so that when a zone is customized, we still have the info of what
  // the correct colors for other zones should be.
  void SetBacklightColor(
      personalization_app::mojom::BacklightColor backlight_color,
      const AccountId& account_id);

  // Returns the currently set backlight color for user with |account_id|.
  personalization_app::mojom::BacklightColor GetBacklightColor(
      const AccountId& account_id);

  // Sets the color of the |zone| for the user with |account_id|.
  void SetBacklightZoneColor(
      int zone,
      personalization_app::mojom::BacklightColor backlight_color,
      const AccountId& account_id);

  // Returns all the zone colors. The order of the colors corresponds to the
  // order of the zones. The size of the vector is guaranteed to be the same as
  // |RgbKeyboardManager::GetZoneCount()|.
  std::vector<personalization_app::mojom::BacklightColor>
  GetBacklightZoneColors(const AccountId& account_id);

  // Returns the current keyboard backlight color display type for user with
  // |account_id|.
  DisplayType GetDisplayType(const AccountId& account_id);

  // RgbKeyboardManagerObserver:
  void OnRgbKeyboardSupportedChanged(bool supported) override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // WallpaperControllerObserver:
  void OnWallpaperColorsChanged() override;

  // Callback function for PrefChangeRegistrar, when policy value populates the
  // local state during the sign-in screen, display the keyboard backlight
  // color.
  void OnKeyboardBacklightColorLocalStateChanged();

  KeyboardBacklightColorNudgeController*
  keyboard_backlight_color_nudge_controller() {
    return keyboard_backlight_color_nudge_controller_.get();
  }

 private:
  friend class KeyboardBacklightColorControllerTest;

  // Displays the |backlight_color| on the keyboard.
  void DisplayBacklightColor(
      personalization_app::mojom::BacklightColor backlight_color);

  // Displays the |backlight_color| at the specific |zone| on the keyboard.
  void DisplayBacklightZoneColor(
      int zone,
      personalization_app::mojom::BacklightColor backlight_color);

  // Sets the keyboard backlight color pref for user with |account_id|.
  void SetBacklightColorPref(
      personalization_app::mojom::BacklightColor backlight_color,
      const AccountId& account_id);

  // Updates all the zone colors to be |backlight_color| in pref for the user
  // with |account_id|.
  void UpdateAllBacklightZoneColors(
      personalization_app::mojom::BacklightColor backlight_color,
      const AccountId& account_id);

  // Updates the keyboard backlight color zone pref at given |zone| for the user
  // with |account_id|.
  void UpdateBacklightZoneColorPref(
      int zone,
      personalization_app::mojom::BacklightColor backlight_color,
      const AccountId& account_id);

  // Used inside |SetBacklightColor()| and |SetBacklightZoneColors()| to set
  // the keyboard backlight color display type for user with |account_id|.
  void SetDisplayType(DisplayType type, const AccountId& account_id);

  // Toggles on the keyboard brightness at 40% if the backlight is off.
  void MaybeToggleOnKeyboardBrightness();

  // Callbacks:
  void KeyboardBrightnessPercentReceived(std::optional<double> percentage);

  // Returns the current wallpaper extracted color.
  SkColor GetCurrentWallpaperColor();

  SkColor displayed_color_for_testing_ = SK_ColorTRANSPARENT;

  base::ScopedObservation<SessionControllerImpl, SessionObserver>
      session_observer_{this};

  base::ScopedObservation<WallpaperController, WallpaperControllerObserver>
      wallpaper_controller_observation_{this};

  std::unique_ptr<KeyboardBacklightColorNudgeController>
      keyboard_backlight_color_nudge_controller_;

  const raw_ptr<PrefService> local_state_ = nullptr;

  PrefChangeRegistrar pref_change_registrar_local_;

  base::WeakPtrFactory<KeyboardBacklightColorController> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_KEYBOARD_BRIGHTNESS_KEYBOARD_BACKLIGHT_COLOR_CONTROLLER_H_
