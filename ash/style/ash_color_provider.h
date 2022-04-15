// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_ASH_COLOR_PROVIDER_H_
#define ASH_STYLE_ASH_COLOR_PROVIDER_H_

#include "ash/ash_export.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/style/color_provider.h"
#include "base/callback_helpers.h"
#include "base/observer_list.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"

class AccountId;
class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;

namespace ash {
class ColorModeObserver;

// The color provider for system UI. It provides colors for Shield layer, Base
// layer, Controls layer and Content layer. Shield layer is a combination of
// color, opacity and blur which may change depending on the context, it is
// usually a fullscreen layer. e.g, PowerButtoneMenuScreenView for power button
// menu. Base layer is the bottom layer of any UI displayed on top of all other
// UIs. e.g, the ShelfView that contains all the shelf items. Controls layer is
// where components such as icons and inkdrops lay on, it may also indicate the
// state of an interactive element (active/inactive states). Content layer means
// the UI elements, e.g., separator, text, icon. The color of an element in
// system UI will be the combination of the colors of the four layers.
class ASH_EXPORT AshColorProvider : public SessionObserver,
                                    public ColorProvider,
                                    public LoginDataDispatcher::Observer {
 public:
  AshColorProvider();
  AshColorProvider(const AshColorProvider& other) = delete;
  AshColorProvider operator=(const AshColorProvider& other) = delete;
  ~AshColorProvider() override;

  static AshColorProvider* Get();

  // Gets the disabled color on |enabled_color|. It can be disabled background,
  // an disabled icon, etc.
  static SkColor GetDisabledColor(SkColor enabled_color);

  // Gets the color of second tone on the given |color_of_first_tone|. e.g,
  // power status icon inside status area is a dual tone icon.
  static SkColor GetSecondToneColor(SkColor color_of_first_tone);

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override;
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // ColorProvider:
  SkColor GetShieldLayerColor(ShieldLayerType type) const override;
  SkColor GetBaseLayerColor(BaseLayerType type) const override;
  SkColor GetControlsLayerColor(ControlsLayerType type) const override;
  SkColor GetContentLayerColor(ContentLayerType type) const override;
  SkColor GetActiveDialogTitleBarColor() const override;
  SkColor GetInactiveDialogTitleBarColor() const override;
  std::pair<SkColor, float> GetInkDropBaseColorAndOpacity(
      SkColor background_color = gfx::kPlaceholderColor) const override;
  std::pair<SkColor, float> GetInvertedInkDropBaseColorAndOpacity(
      SkColor background_color = gfx::kPlaceholderColor) const override;
  void AddObserver(ColorModeObserver* observer) override;
  void RemoveObserver(ColorModeObserver* observer) override;
  // TODO(minch): Rename to ShouldUseDarkColors.
  bool IsDarkModeEnabled() const override;
  void SetDarkModeEnabledForTest(bool enabled) override;

  // LoginDataDispatcher::Observer:
  void OnOobeDialogStateChanged(OobeDialogState state) override;
  void OnFocusPod(const AccountId& account_id) override;

  // Gets the color of |type| of the corresponding layer based on the current
  // inverted color mode. For views that need LIGHT colors while DARK mode is
  // active, and vice versa.
  SkColor GetInvertedShieldLayerColor(ShieldLayerType type) const;
  SkColor GetInvertedBaseLayerColor(BaseLayerType type) const;
  SkColor GetInvertedControlsLayerColor(ControlsLayerType type) const;
  SkColor GetInvertedContentLayerColor(ContentLayerType type) const;

  // Gets the background color that can be applied on any layer. The returned
  // color will be different based on color mode and color theme (see
  // |is_themed_|).
  SkColor GetBackgroundColor() const;
  // Same as above, but returns the color based on the current inverted color
  // mode and color theme.
  SkColor GetInvertedBackgroundColor() const;
  // Gets the background color in the desired color mode dark/light.
  SkColor GetBackgroundColorInMode(bool use_dark_color) const;

  // Whether the system color mode is themed, by default is true. If true, the
  // background color will be calculated based on extracted wallpaper color.
  bool IsThemed() const;

  // Toggles pref |kDarkModeEnabled|.
  void ToggleColorMode();

  // Updates pref |kColorModeThemed| to |is_themed|.
  void UpdateColorModeThemed(bool is_themed);

 private:
  friend class ScopedLightModeAsDefault;
  friend class ScopedAssistantLightModeAsDefault;

  // Gets the color of |type| of the corresponding layer. Returns color based on
  // the current inverted color mode if |inverted| is true.
  SkColor GetShieldLayerColorImpl(ShieldLayerType type, bool inverted) const;
  SkColor GetBaseLayerColorImpl(BaseLayerType type, bool inverted) const;
  // Gets the color of |type| of the corresponding layer. Returns the color on
  // dark mode if |use_dark_color| is true.
  SkColor GetControlsLayerColorImpl(ControlsLayerType type,
                                    bool use_dark_color) const;
  SkColor GetContentLayerColorImpl(ContentLayerType type,
                                   bool use_dark_color) const;

  // Gets the background default color based on the current color mode.
  SkColor GetBackgroundDefaultColor() const;
  // Gets the background default color based on the current inverted color mode.
  SkColor GetInvertedBackgroundDefaultColor() const;

  // Gets the background themed color based on the current color mode.
  SkColor GetBackgroundThemedColor() const;
  // Gets the background themed color based on the current inverted color mode.
  SkColor GetInvertedBackgroundThemedColor() const;
  // Gets the background themed color that's calculated based on the color
  // extracted from wallpaper. For dark mode, it will be dark muted wallpaper
  // prominent color + SK_ColorBLACK 50%. For light mode, it will be light
  // muted wallpaper prominent color + SK_ColorWHITE 75%. Extracts the color on
  // dark mode if |use_dark_color| is true.
  SkColor GetBackgroundThemedColorImpl(SkColor default_color,
                                       bool use_dark_color) const;

  // Notifies all the observers on |kDarkModeEnabled|'s change.
  void NotifyDarkModeEnabledPrefChange();

  // Notifies all the observers on |kColorModeThemed|'s change.
  void NotifyColorModeThemedPrefChange();

  // Returns a closure which calls `NotifyIfDarkModeChanged` if the dark mode
  // changed between creation and getting out of scope.
  base::ScopedClosureRunner GetNotifyOnDarkModeChangeClosure();
  void NotifyIfDarkModeChanged(bool old_is_dark_mode_enabled);

  // The default color is DARK when the DarkLightMode feature is disabled. But
  // we can also override it to LIGHT through ScopedLightModeAsDefault. This is
  // done to help keeping some of the UI elements as LIGHT by default before
  // launching the DarkLightMode feature. Overriding only if the DarkLightMode
  // feature is disabled. This variable will be removed once fully launched the
  // DarkLightMode feature.
  bool override_light_mode_as_default_ = false;

  // Temporary field for testing purposes while OOBE WebUI is being migrated.
  absl::optional<bool> is_dark_mode_enabled_in_oobe_for_testing_;

  // True if we're in the OOBE state, or OOBE WebUI dialog is open (e.g. for the
  // "Add person" flow), except for the last two screens. In those two screens
  // the theme is based on user's preferences.
  bool force_oobe_light_mode_ = false;

  // absl::nullopt in case no user pod is focused.
  absl::optional<bool> is_dark_mode_enabled_for_focused_pod_;

  base::ObserverList<ColorModeObserver> observers_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  PrefService* active_user_pref_service_ = nullptr;  // Not owned.
};

}  // namespace ash

#endif  // ASH_STYLE_ASH_COLOR_PROVIDER_H_
