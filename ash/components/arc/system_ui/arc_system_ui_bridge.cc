// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/system_ui/arc_system_ui_bridge.h"

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/session/connection_holder.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/style/mojom/color_scheme.mojom-shared.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chromeos/constants/chromeos_features.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace arc {

namespace {

// Singleton factory for ArcSystemUIBridge.
class ArcSystemUIBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcSystemUIBridge,
          ArcSystemUIBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcSystemUIBridgeFactory";

  static ArcSystemUIBridgeFactory* GetInstance() {
    return base::Singleton<ArcSystemUIBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcSystemUIBridgeFactory>;
  ArcSystemUIBridgeFactory() = default;
  ~ArcSystemUIBridgeFactory() override = default;
};

// Converts a `ash::style::mojom::ColorScheme` to the equivalent
// `mojom::ThemeStyleType`.
mojom::ThemeStyleType ToThemeStyle(ash::style::mojom::ColorScheme scheme) {
  switch (scheme) {
    // In ChromeOS, static is a color that's not from the wallpaper and palettes
    // are always tonal.
    case ash::style::mojom::ColorScheme::kStatic:
    case ash::style::mojom::ColorScheme::kTonalSpot:
      return mojom::ThemeStyleType::TONAL_SPOT;
    case ash::style::mojom::ColorScheme::kNeutral:
      return mojom::ThemeStyleType::SPRITZ;
    case ash::style::mojom::ColorScheme::kExpressive:
      return mojom::ThemeStyleType::EXPRESSIVE;
    case ash::style::mojom::ColorScheme::kVibrant:
      return mojom::ThemeStyleType::VIBRANT;
  }
}

}  // namespace

// static
ArcSystemUIBridge* ArcSystemUIBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcSystemUIBridgeFactory::GetForBrowserContext(context);
}

// static
ArcSystemUIBridge* ArcSystemUIBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcSystemUIBridgeFactory::GetForBrowserContextForTesting(context);
}

ArcSystemUIBridge::ArcSystemUIBridge(content::BrowserContext* context,
                                     ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->system_ui()->AddObserver(this);
}

ArcSystemUIBridge::~ArcSystemUIBridge() {
  DetachColorPaletteController();
  arc_bridge_service_->system_ui()->RemoveObserver(this);
}

void ArcSystemUIBridge::OnConnectionReady() {
  AttachColorPaletteController();

  const auto seed = color_palette_controller_->GetCurrentSeed();
  if (seed) {
    OnColorPaletteChanging(*seed);
  }
}

void ArcSystemUIBridge::OnColorPaletteChanging(
    const ash::ColorPaletteSeed& in_seed) {
  // Make a copy so we can modify the seed for backward compatibility.
  ash::ColorPaletteSeed seed = in_seed;

  if (previous_seed_ == seed) {
    // Skip sending identical events
    return;
  }

  // Save the previous value to detect changes.
  auto old_previous = std::move(previous_seed_);
  // Save the current value for future comparisons.
  previous_seed_.emplace(seed);

  if (!old_previous || seed.color_mode != old_previous->color_mode) {
    bool dark_theme = seed.color_mode == ui::ColorProviderKey::ColorMode::kDark;
    if (!SendDeviceDarkThemeState(dark_theme)) {
      LOG(ERROR) << "Failed to send theme status of: " << dark_theme;
      return;
    }
  }

  if (!old_previous || seed.seed_color != old_previous->seed_color ||
      seed.scheme != old_previous->scheme) {
    if (!SendOverlayColor(seed.seed_color, ToThemeStyle(seed.scheme))) {
      LOG(ERROR) << "Failed to send overlay color information for seed change.";
      return;
    }
  }
}

void ArcSystemUIBridge::OnShellDestroying() {
  // Detach from `color_palette_controller_` when Shell is destroyed as its
  // owned by Shell and we can outlive Shell. Otherwise, we try to remove
  // ourselves from a dead object in our destructor.
  DetachColorPaletteController();
}

void ArcSystemUIBridge::AttachColorPaletteController() {
  // Skip attaching if we haven't been detached first.
  if (color_palette_controller_) {
    return;
  }

  ash::Shell* const shell = ash::Shell::Get();
  color_palette_controller_ = shell->color_palette_controller();
  CHECK(color_palette_controller_);
  color_palette_controller_->AddObserver(this);
  shell_observation_.Observe(shell);
}

void ArcSystemUIBridge::DetachColorPaletteController() {
  // Only detach if we actually started listening.
  if (!color_palette_controller_) {
    return;
  }

  color_palette_controller_->RemoveObserver(this);
  color_palette_controller_ = nullptr;
  shell_observation_.Reset();
}

bool ArcSystemUIBridge::SendDeviceDarkThemeState(bool dark_theme_status) {
  mojom::SystemUiInstance* system_ui_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->system_ui(), SetDarkThemeStatus);

  if (!system_ui_instance)
    return false;
  system_ui_instance->SetDarkThemeStatus(dark_theme_status);
  return true;
}

bool ArcSystemUIBridge::SendOverlayColor(uint32_t source_color,
                                         mojom::ThemeStyleType theme_style) {
  mojom::SystemUiInstance* system_ui_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->system_ui(), SetOverlayColor);

  if (!system_ui_instance)
    return false;
  system_ui_instance->SetOverlayColor(source_color, theme_style);

  return true;
}

// static
void ArcSystemUIBridge::EnsureFactoryBuilt() {
  ArcSystemUIBridgeFactory::GetInstance();
}

}  // namespace arc
