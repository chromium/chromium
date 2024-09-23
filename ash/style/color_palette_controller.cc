// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/color_palette_controller.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/login/login_screen_controller.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/style/color_mode_observer.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/color_util.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/style/mojom/color_scheme.mojom-shared.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/barrier_callback.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/dynamic_color/palette.h"
#include "ui/color/dynamic_color/palette_factory.h"
#include "ui/gfx/color_palette.h"

namespace ash {

namespace {

class ColorPaletteControllerImpl;

using ColorMode = ui::ColorProviderKey::ColorMode;

const SkColor kDefaultWallpaperColor = gfx::kGoogleBlue400;

PrefService* GetUserPrefService(const AccountId& account_id) {
  if (!account_id.is_valid()) {
    CHECK_IS_TEST();
    return nullptr;
  }
  return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
      account_id);
}

// Returns the currently active user session (at index 0).
const UserSession* GetActiveUserSession() {
  return Shell::Get()->session_controller()->GetUserSession(/*index=*/0);
}

const AccountId& AccountFromSession(const UserSession* session) {
  CHECK(session);
  return session->user_info.account_id;
}

using SchemeVariant = ui::ColorProviderKey::SchemeVariant;

SchemeVariant ToVariant(style::mojom::ColorScheme scheme) {
  switch (scheme) {
    case style::mojom::ColorScheme::kStatic:
    case style::mojom::ColorScheme::kNeutral:
      return SchemeVariant::kNeutral;
    case style::mojom::ColorScheme::kTonalSpot:
      return SchemeVariant::kTonalSpot;
    case style::mojom::ColorScheme::kExpressive:
      return SchemeVariant::kExpressive;
    case style::mojom::ColorScheme::kVibrant:
      return SchemeVariant::kVibrant;
  }
}

SampleColorScheme GenerateSampleColorScheme(bool dark,
                                            SkColor seed_color,
                                            style::mojom::ColorScheme scheme) {
  DCHECK_NE(scheme, style::mojom::ColorScheme::kStatic)
      << "Requesting a static scheme doesn't make sense since there is no "
         "seed color";

  std::unique_ptr<ui::Palette> palette =
      ui::GeneratePalette(seed_color, ToVariant(scheme));
  SampleColorScheme sample;
  sample.scheme = scheme;
  // Tertiary is cros.ref.teratiary-70 for all color schemes.
  sample.tertiary = palette->tertiary().get(70.f);  // tertiary 70

  if (scheme == style::mojom::ColorScheme::kVibrant) {
    // Vibrant uses cros.ref.primary-70 and cros.ref.primary-50.
    sample.primary = palette->primary().get(70.f);    // primary 70
    sample.secondary = palette->primary().get(50.f);  // primary 50
  } else {
    // All other schemes use cros.ref.primary-80 and cros.ref.primary-60.
    sample.primary = palette->primary().get(80.f);    // primary 80
    sample.secondary = palette->primary().get(60.f);  // primary 60
  }

  return sample;
}

void SortSampleColorSchemes(
    ColorPaletteController::SampleColorSchemeCallback callback,
    base::span<const style::mojom::ColorScheme> color_scheme_buttons,
    std::vector<SampleColorScheme> sample_color_schemes) {
  std::vector<SampleColorScheme> sorted_sample_color_schemes;
  for (const auto scheme : color_scheme_buttons) {
    auto color_scheme_sample =
        std::find_if(sample_color_schemes.begin(), sample_color_schemes.end(),
                     [&scheme](SampleColorScheme sample) {
                       return scheme == sample.scheme;
                     });
    sorted_sample_color_schemes.push_back(*color_scheme_sample);
  }
  std::move(callback).Run(sorted_sample_color_schemes);
}

// Refresh colors of the system on the current color mode. Not only the SysUI,
// but also all the other components like WebUI. This will trigger
// View::OnThemeChanged to live update the colors. The colors live update can
// happen when color mode changes or wallpaper changes. It is needed when
// wallpaper changes as the background color is calculated from current
// wallpaper.
void RefreshNativeTheme(const ColorPaletteSeed& seed) {
  const SkColor themed_color = seed.seed_color;
  bool is_dark_mode_enabled = seed.color_mode == ColorMode::kDark;
  auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  native_theme->set_use_dark_colors(is_dark_mode_enabled);
  native_theme->set_user_color(themed_color);
  native_theme->set_scheme_variant(ToVariant(seed.scheme));
  native_theme->NotifyOnNativeThemeUpdated();

  auto* native_theme_web = ui::NativeTheme::GetInstanceForWeb();
  if (!native_theme_web->IsForcedDarkMode()) {
    native_theme_web->set_use_dark_colors(is_dark_mode_enabled);
    native_theme_web->set_preferred_color_scheme(
        is_dark_mode_enabled ? ui::NativeTheme::PreferredColorScheme::kDark
                             : ui::NativeTheme::PreferredColorScheme::kLight);
  }
  native_theme_web->set_scheme_variant(ToVariant(seed.scheme));
  native_theme_web->set_user_color(themed_color);
  native_theme_web->NotifyOnNativeThemeUpdated();
}

class ColorPaletteControllerImpl : public ColorPaletteController,
                                   public WallpaperControllerObserver,
                                   public ColorModeObserver {
 public:
  ColorPaletteControllerImpl(
      DarkLightModeController* dark_light_mode_controller,
      WallpaperControllerImpl* wallpaper_controller,
      PrefService* local_state)
      : wallpaper_controller_(wallpaper_controller),
        dark_light_mode_controller_(dark_light_mode_controller),
        local_state_(local_state) {
    dark_light_observation_.Observe(dark_light_mode_controller);
    wallpaper_observation_.Observe(wallpaper_controller);
    Shell::Get()->login_screen_controller()->data_dispatcher()->AddObserver(
        this);
    if (!local_state) {
      // The local state should only be null in tests.
      CHECK_IS_TEST();
    }
  }
  ~ColorPaletteControllerImpl() override {
    DCHECK_EQ(0, notification_pauser_count_);
  }

  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  void SetColorScheme(style::mojom::ColorScheme scheme,
                      const AccountId& account_id,
                      base::OnceClosure on_complete) override {
    DVLOG(1) << "Setting color scheme to: " << (int)scheme;
    PrefService* pref_service = GetUserPrefService(account_id);
    if (!pref_service) {
      DVLOG(1) << "No user pref service available.";
      return;
    }
    pref_service->SetInteger(prefs::kDynamicColorColorScheme,
                             static_cast<int>(scheme));
    NotifyObservers(GetColorPaletteSeed(account_id));
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(on_complete));
  }

  void SetStaticColor(SkColor seed_color,
                      const AccountId& account_id,
                      base::OnceClosure on_complete) override {
    DVLOG(1) << "Static color scheme: " << (int)seed_color;
    PrefService* pref_service = GetUserPrefService(account_id);
    if (!pref_service) {
      DVLOG(1) << "No user pref service available.";
      return;
    }
    // Set the color scheme before the seed color because there is a check in
    // |GetStaticColor| to only return a color if the color scheme is kStatic.
    pref_service->SetInteger(
        prefs::kDynamicColorColorScheme,
        static_cast<int>(style::mojom::ColorScheme::kStatic));
    pref_service->SetUint64(prefs::kDynamicColorSeedColor, seed_color);
    NotifyObservers(GetColorPaletteSeed(account_id));
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(on_complete));
  }

  std::optional<ColorPaletteSeed> GetColorPaletteSeed(
      const AccountId& account_id) const override {
    ColorPaletteSeed seed;
    std::optional<SkColor> seed_color =
        UsesWallpaperSeedColor(account_id)
            ? GetWallpaperColorForUser(account_id)
            : GetStaticSeedColor(account_id);
    if (!seed_color) {
      return {};
    }

    seed.color_mode = dark_light_mode_controller_->IsDarkModeEnabled()
                          ? ui::ColorProviderKey::ColorMode::kDark
                          : ui::ColorProviderKey::ColorMode::kLight;
    seed.seed_color = *seed_color;
    seed.scheme = GetColorScheme(account_id);

    return seed;
  }

  std::optional<SkColor> GetWallpaperColorForUser(
      const AccountId& account_id) const {
    if (GetActiveUserSession()) {
      return CurrentWallpaperColor(
          dark_light_mode_controller_->IsDarkModeEnabled());
    }
    const bool should_use_k_means = ShouldUseKMeans(account_id);
    std::optional<SkColor> seed_color =
        wallpaper_controller_->GetCachedWallpaperColorForUser(
            account_id, should_use_k_means);
    if (seed_color.has_value()) {
      if (should_use_k_means) {
        bool dark = dark_light_mode_controller_->IsDarkModeEnabled();
        return ColorUtil::AdjustKMeansColor(seed_color.value(), dark);
      }
      return seed_color.value();
    }
    DVLOG(1)
        << "No wallpaper color for user. Returning default wallpaper color.";
    return kDefaultWallpaperColor;
  }

  std::optional<ColorPaletteSeed> GetCurrentSeed() const override {
    const auto* session = GetActiveUserSession();
    if (!session) {
      return {};
    }

    return GetColorPaletteSeed(AccountFromSession(session));
  }

  bool UsesWallpaperSeedColor(const AccountId& account_id) const override {
    // Scheme tracks if wallpaper color is used.
    return GetColorScheme(account_id) != style::mojom::ColorScheme::kStatic;
  }

  style::mojom::ColorScheme GetColorScheme(
      const AccountId& account_id) const override {
    PrefService* pref_service = GetUserPrefService(account_id);
    if (pref_service) {
      const PrefService::Preference* pref =
          pref_service->FindPreference(prefs::kDynamicColorColorScheme);
      if (!pref->IsDefaultValue()) {
        return static_cast<style::mojom::ColorScheme>(
            pref->GetValue()->GetInt());
      }
    } else {
      CHECK(local_state_);
      const auto scheme =
          user_manager::KnownUser(local_state_)
              .FindIntPath(account_id, prefs::kDynamicColorColorScheme);
      if (scheme.has_value()) {
        return static_cast<style::mojom::ColorScheme>(scheme.value());
      }
    }

    DVLOG(1) << "No user pref service or local pref service available. "
                "Returning default color scheme.";
    // The preferred default color scheme for the time of day wallpaper instead
    // of tonal spot.
    return features::IsTimeOfDayWallpaperEnabled() &&
                   wallpaper_controller_->IsTimeOfDayWallpaper()
               ? style::mojom::ColorScheme::kNeutral
               : style::mojom::ColorScheme::kTonalSpot;
  }

  std::optional<SkColor> GetStaticColor(
      const AccountId& account_id) const override {
    if (GetColorScheme(account_id) == style::mojom::ColorScheme::kStatic) {
      return GetStaticSeedColor(account_id);
    }

    return std::nullopt;
  }

  void GenerateSampleColorSchemes(
      base::span<const style::mojom::ColorScheme> color_scheme_buttons,
      SampleColorSchemeCallback callback) const override {
    bool dark = dark_light_mode_controller_->IsDarkModeEnabled();
    std::optional<SkColor> celebi_seed_color = GetCurrentCelebiColor();
    if (!celebi_seed_color) {
      LOG(WARNING) << "Using default color due to missing wallpaper sample";
      celebi_seed_color.emplace(kDefaultWallpaperColor);
    }
    CHECK(celebi_seed_color.has_value());
    auto* session = GetActiveUserSession();
    DCHECK(session);
    auto account_id = AccountFromSession(session);
    bool use_k_means = GetUseKMeansPref(account_id);
    const SkColor tonal_spot_color =
        use_k_means ? GetCurrentKMeanColor() : *celebi_seed_color;
    // Schemes need to be copied as the underlying memory for the span could go
    // out of scope.
    const std::vector<style::mojom::ColorScheme> schemes_copy(
        color_scheme_buttons.begin(), color_scheme_buttons.end());
    const auto barrier_callback = base::BarrierCallback<SampleColorScheme>(
        color_scheme_buttons.size(),
        base::BindOnce(&SortSampleColorSchemes,
                       base::BindPostTaskToCurrentDefault(std::move(callback)),
                       std::move(schemes_copy)));

    for (unsigned int i = 0; i < color_scheme_buttons.size(); i++) {
      SkColor seed_color =
          color_scheme_buttons[i] == style::mojom::ColorScheme::kTonalSpot
              ? tonal_spot_color
              : *celebi_seed_color;
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&GenerateSampleColorScheme, dark, seed_color,
                         color_scheme_buttons[i]),
          base::BindOnce(std::move(barrier_callback)));
    }
  }

  // LoginDataDispatcher::Observer overrides:
  void OnOobeDialogStateChanged(OobeDialogState state) override {
    oobe_state_ = state;
  }

  // WallpaperControllerObserver overrides:
  void OnWallpaperColorsChanged() override {
    NotifyObservers(BestEffortSeed(GetActiveUserSession()));
  }

  // WallpaperControllerObserver overrides:
  void OnUserSetWallpaper(const AccountId& account_id) override {
    if (oobe_state_ == OobeDialogState::HIDDEN) {
      UpdateUseKMeanColor(account_id, /* value= */ false);
    }
  }

  void SelectLocalAccount(const AccountId& account_id) override {
    NotifyObservers(GetColorPaletteSeed(account_id));
  }

  // ColorModeObserver overrides:
  void OnColorModeChanged(bool) override {
    NotifyObservers(BestEffortSeed(GetActiveUserSession()));
  }

  // SessionObserver overrides:
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override {
    ScopedNotificationPauser scoped_notification_pauser(this);

    MaybeSetUseKMeansPref(prefs);
    NotifyObservers(BestEffortSeed(GetActiveUserSession()));

    pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
    pref_change_registrar_->Init(prefs);
    OnColorSchemePrefChanged();
    OnSeedColorPrefChanged();
    OnUseKMeansPrefChanged();

    pref_change_registrar_->Add(
        prefs::kDynamicColorColorScheme,
        base::BindRepeating(
            &ColorPaletteControllerImpl::OnColorSchemePrefChanged,
            base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kDynamicColorSeedColor,
        base::BindRepeating(&ColorPaletteControllerImpl::OnSeedColorPrefChanged,
                            base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kDynamicColorUseKMeans,
        base::BindRepeating(&ColorPaletteControllerImpl::OnUseKMeansPrefChanged,
                            base::Unretained(this)));
  }

  // Sets the UseKMeans pref to false if the user is new, and
  // true if the user does not have a set pref. This will give users updating
  // the device the k means color for tonal spot, while new users will only see
  // celebi colors.
  void MaybeSetUseKMeansPref(PrefService* prefs) {
    DCHECK(prefs);
    auto* session = GetActiveUserSession();
    DCHECK(session);
    if (session->user_info.is_ephemeral) {
      return;
    }

    auto account_id = AccountFromSession(session);
    if (Shell::Get()->session_controller()->IsUserFirstLogin()) {
      // New users should use celebi color calculation.
      UpdateUseKMeanColor(account_id, /* value= */ false);
      return;
    }
    const PrefService::Preference* pref =
        prefs->FindPreference(prefs::kDynamicColorUseKMeans);
    if (pref->IsDefaultValue()) {
      // Any user without a set pref is an existing user logging in for the
      // first time after updating their device. They should use the kmeans
      // color calculation.
      UpdateUseKMeanColor(account_id, /* value= */ true);
    }
  }

  SkColor GetUserWallpaperColorOrDefault(SkColor default_color) const override {
    const auto& calculated_colors = wallpaper_controller_->calculated_colors();
    if (!calculated_colors) {
      DVLOG(1) << "Failed to get wallpaper color";
      const bool dark_mode = dark_light_mode_controller_->IsDarkModeEnabled();
      return ColorUtil::AdjustKMeansColor(default_color, dark_mode);
    }

    std::optional<AccountId> account_id;
    auto* session = GetActiveUserSession();
    if (session) {
      account_id = AccountFromSession(session);
    }

    // Return KMeans if the account is kept in legacy mode i.e. they haven't
    // changed their wallpaper since the new sampling algorithm was introduced.
    if (account_id.has_value() && ShouldUseKMeans(*account_id)) {
      return GetCurrentKMeanColor();
    }

    const auto celebi_color = GetCurrentCelebiColor();
    if (celebi_color.has_value()) {
      return *celebi_color;
    }
    DVLOG(1) << "Failed to get wallpaper color";
    return default_color;
  }

  bool GetUseKMeansPref(const AccountId& account_id) const override {
    PrefService* pref_service = GetUserPrefService(account_id);
    if (pref_service) {
      return pref_service->GetBoolean(prefs::kDynamicColorUseKMeans);
    }
    CHECK(local_state_);
    const base::Value* value =
        user_manager::KnownUser(local_state_)
            .FindPath(account_id, prefs::kDynamicColorUseKMeans);
    if (value && value->GetIfBool().has_value()) {
      return value->GetBool();
    }
    DVLOG(1) << "No user pref service or local pref service available. "
                "Returning UseKMeans pref as false.";
    return false;
  }

 private:
  // Helper class to pause observer notifications when there is one instance
  // is live. The last one of the skipped notifications is sent out when the
  // last instances of this class is going away.
  class ScopedNotificationPauser {
   public:
    explicit ScopedNotificationPauser(ColorPaletteControllerImpl* controller)
        : controller_(controller) {
      controller_->AddNotificationPauser();
    }
    ~ScopedNotificationPauser() { controller_->RemoveNotificationPauser(); }

   private:
    // `controller_` must out live this class.
    const raw_ptr<ColorPaletteControllerImpl> controller_;
  };

  // Gets the user's current wallpaper color.
  // TODO(b/289106519): Combine this function with |GetUserWallpaperColor|.
  std::optional<SkColor> CurrentWallpaperColor(bool dark) const {
    const std::optional<WallpaperCalculatedColors>& calculated_colors =
        wallpaper_controller_->calculated_colors();
    if (!calculated_colors) {
      return {};
    }
    auto* session = GetActiveUserSession();
    if (session) {
      auto account_id = AccountFromSession(session);
      if (ShouldUseKMeans(account_id)) {
        return GetCurrentKMeanColor();
      }
    }
    return GetCurrentCelebiColor();
  }

  std::optional<SkColor> GetCurrentCelebiColor() const {
    const std::optional<WallpaperCalculatedColors>& calculated_colors =
        wallpaper_controller_->calculated_colors();
    if (!calculated_colors) {
      return {};
    }
    return calculated_colors->celebi_color;
  }

  SkColor GetCurrentKMeanColor() const {
    bool dark = dark_light_mode_controller_->IsDarkModeEnabled();
    const SkColor default_color = dark ? gfx::kGoogleGrey900 : SK_ColorWHITE;
    SkColor k_mean_color = wallpaper_controller_->GetKMeanColor();
    SkColor color =
        k_mean_color == kInvalidWallpaperColor ? default_color : k_mean_color;
    return ColorUtil::AdjustKMeansColor(color, dark);
  }

  bool ShouldUseKMeans(const AccountId& account_id) const {
    return GetColorScheme(account_id) ==
               style::mojom::ColorScheme::kTonalSpot &&
           GetUseKMeansPref(account_id);
  }

  void UpdateUseKMeanColor(const AccountId& account_id, bool value) {
    PrefService* pref_service = GetUserPrefService(account_id);
    if (pref_service) {
      pref_service->SetBoolean(prefs::kDynamicColorUseKMeans, value);
    }
  }

  SkColor GetStaticSeedColor(const AccountId& account_id) const {
    PrefService* pref_service = GetUserPrefService(account_id);
    if (pref_service) {
      return static_cast<SkColor>(
          pref_service->GetUint64(prefs::kDynamicColorSeedColor));
    }
    CHECK(local_state_);
    const base::Value* value =
        user_manager::KnownUser(local_state_)
            .FindPath(account_id, prefs::kDynamicColorSeedColor);
    if (value) {
      const auto seed_color = base::ValueToInt64(value);
      if (seed_color.has_value()) {
        return static_cast<SkColor>(seed_color.value());
      }
    }
    DVLOG(1) << "No user pref service or local pref service available. "
                "Returning default color palette seed.";
    return kDefaultWallpaperColor;
  }

  // Returns the seed for `session` if it's present.  Otherwise, returns a seed
  // for backward compatibility with just dark/light and seed color filled.
  std::optional<ColorPaletteSeed> BestEffortSeed(const UserSession* session) {
    if (session) {
      return GetColorPaletteSeed(AccountFromSession(session));
    }

    session_manager::SessionState session_state =
        Shell::Get()->session_controller()->GetSessionState();
    const bool is_oobe =
        session_state == session_manager::SessionState::OOBE ||
        (session_state == session_manager::SessionState::LOGIN_PRIMARY &&
         oobe_state_ != OobeDialogState::HIDDEN);

    if (!is_oobe) {
      // This early return prevents overwriting colors. OOBE has a special
      // wallpaper and needs to sample the color. In any other case, like on the
      // login screen, the calculated colors may not have been updated and so
      // the colors should not be updated.
      return {};
    }

    // Generate a seed where we assume TonalSpot and ignore static colors.
    ColorPaletteSeed seed;
    bool dark = dark_light_mode_controller_->IsDarkModeEnabled();
    // The user is in OOBE and should see the celebi color.
    std::optional<SkColor> seed_color = GetCurrentCelebiColor();
    if (!seed_color) {
      // If `seed_color` is not available, we expect to have it shortly
      // the color computation is done and this will be called again.
      return {};
    }
    seed.color_mode = dark ? ui::ColorProviderKey::ColorMode::kDark
                           : ui::ColorProviderKey::ColorMode::kLight;
    seed.seed_color = *seed_color;
    seed.scheme = style::mojom::ColorScheme::kTonalSpot;

    return seed;
  }

  void NotifyObservers(const std::optional<ColorPaletteSeed>& seed) {
    if (notification_pauser_count_) {
      last_notification_seed_ = seed;
      return;
    }

    if (!seed) {
      // If the seed wasn't valid, skip notifications.
      return;
    }

    for (auto& observer : observers_) {
      observer.OnColorPaletteChanging(*seed);
    }

    RefreshNativeTheme(*seed);
  }

  void OnColorSchemePrefChanged() {
    if (!local_state_) {
      CHECK_IS_TEST();
      return;
    }
    auto account_id = AccountFromSession(GetActiveUserSession());
    auto color_scheme = GetColorScheme(account_id);
    user_manager::KnownUser(local_state_)
        .SetIntegerPref(account_id, prefs::kDynamicColorColorScheme,
                        static_cast<int>(color_scheme));
    NotifyObservers(BestEffortSeed(GetActiveUserSession()));
  }

  void OnSeedColorPrefChanged() {
    if (!local_state_) {
      CHECK_IS_TEST();
      return;
    }
    auto account_id = AccountFromSession(GetActiveUserSession());
    auto seed_color = GetStaticSeedColor(account_id);
    user_manager::KnownUser(local_state_)
        .SetPath(account_id, prefs::kDynamicColorSeedColor,
                 base::Int64ToValue(seed_color));
    NotifyObservers(BestEffortSeed(GetActiveUserSession()));
  }

  void OnUseKMeansPrefChanged() {
    if (!local_state_) {
      CHECK_IS_TEST();
      return;
    }
    auto account_id = AccountFromSession(GetActiveUserSession());
    auto use_k_means = GetUseKMeansPref(account_id);
    user_manager::KnownUser(local_state_)
        .SetBooleanPref(account_id, prefs::kDynamicColorUseKMeans, use_k_means);
    NotifyObservers(BestEffortSeed(GetActiveUserSession()));
  }

  void AddNotificationPauser() { ++notification_pauser_count_; }

  void RemoveNotificationPauser() {
    --notification_pauser_count_;

    if (notification_pauser_count_ == 0 && last_notification_seed_) {
      NotifyObservers(last_notification_seed_);
      last_notification_seed_.reset();
    }
  }

  base::ScopedObservation<DarkLightModeController, ColorModeObserver>
      dark_light_observation_{this};

  base::ScopedObservation<WallpaperController, WallpaperControllerObserver>
      wallpaper_observation_{this};

  ScopedSessionObserver scoped_session_observer_{this};

  raw_ptr<WallpaperControllerImpl> wallpaper_controller_;  // unowned

  raw_ptr<DarkLightModeController> dark_light_mode_controller_;  // unowned

  // May be null in tests.
  const raw_ptr<PrefService> local_state_;

  base::ObserverList<ColorPaletteController::Observer> observers_;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  OobeDialogState oobe_state_ = OobeDialogState::HIDDEN;

  // Number of live ScopedNotificationPausers.
  int notification_pauser_count_ = 0;
  std::optional<ColorPaletteSeed> last_notification_seed_;
};

}  // namespace

// static
std::unique_ptr<ColorPaletteController> ColorPaletteController::Create(
    DarkLightModeController* dark_light_mode_controller,
    WallpaperControllerImpl* wallpaper_controller,
    PrefService* local_state) {
  return std::make_unique<ColorPaletteControllerImpl>(
      dark_light_mode_controller, wallpaper_controller, local_state);
}

// static
void ColorPaletteController::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kDynamicColorColorScheme,
      static_cast<int>(style::mojom::ColorScheme::kTonalSpot),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterUint64Pref(
      prefs::kDynamicColorSeedColor, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kDynamicColorUseKMeans, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
}

// static
void ColorPaletteController::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kDynamicColorColorScheme,
      static_cast<int>(style::mojom::ColorScheme::kTonalSpot));
  registry->RegisterUint64Pref(prefs::kDynamicColorSeedColor, 0);
  registry->RegisterBooleanPref(prefs::kDynamicColorUseKMeans, false);
}

}  // namespace ash
