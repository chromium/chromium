// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/chrome_feature_flags/arc_chrome_feature_flags_bridge.h"

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/memory/singleton.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace arc {

namespace {

// Singleton factory for ArcChromeFeatureFlagsBridge.
class ArcChromeFeatureFlagsBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcChromeFeatureFlagsBridge,
          ArcChromeFeatureFlagsBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcChromeFeatureFlagsBridgeFactory";

  static ArcChromeFeatureFlagsBridgeFactory* GetInstance() {
    return base::Singleton<ArcChromeFeatureFlagsBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcChromeFeatureFlagsBridgeFactory>;
  ArcChromeFeatureFlagsBridgeFactory() = default;
  ~ArcChromeFeatureFlagsBridgeFactory() override = default;
};

const std::vector<const base::Feature*> kArcFeatureList = {
    &ash::features::kQsRevamp,
    &chromeos::features::kJelly,
    &kTouchscreenEmulation,
    &kTrackpadScrollTouchscreenEmulation,
    &arc::kRoundedWindowCompat,
    &chromeos::features::kRoundedWindows,
    &kXdgMode,
    &ash::features::kPipDoubleTapToResize};

const base::Feature* GetArcFeatureByName(const std::string& feature_name) {
  for (const auto* feature : kArcFeatureList) {
    if (feature->name == feature_name) {
      return feature;
    }
  }
  return nullptr;
}

absl::optional<int> GetFieldTrialIntIfAvailable(const base::Feature* feature,
                                                std::string param_name) {
  const std::string param =
      base::GetFieldTrialParamValueByFeature(*feature, param_name);
  int int_parameter = 0;
  return (param.empty() || !base::StringToInt(param, &int_parameter))
             ? absl::nullopt
             : absl::make_optional<int>(int_parameter);
}

absl::optional<double> GetFieldTrialDoubleIfAvailable(
    const base::Feature* feature,
    std::string param_name) {
  const std::string param =
      base::GetFieldTrialParamValueByFeature(*feature, param_name);
  double double_parameter = 0;
  return (param.empty() || !base::StringToDouble(param, &double_parameter))
             ? absl::nullopt
             : absl::make_optional<double>(double_parameter);
}

absl::optional<bool> GetFieldTrialBoolIfAvailable(const base::Feature* feature,
                                                  std::string param_name) {
  const std::string param =
      base::GetFieldTrialParamValueByFeature(*feature, param_name);
  if (param == "true") {
    return absl::make_optional<bool>(true);
  } else if (param == "false") {
    return absl::make_optional<bool>(false);
  }
  return absl::nullopt;
}

}  // namespace

// static
ArcChromeFeatureFlagsBridge* ArcChromeFeatureFlagsBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcChromeFeatureFlagsBridgeFactory::GetForBrowserContext(context);
}

// static
ArcChromeFeatureFlagsBridge*
ArcChromeFeatureFlagsBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcChromeFeatureFlagsBridgeFactory::GetForBrowserContextForTesting(
      context);
}

ArcChromeFeatureFlagsBridge::ArcChromeFeatureFlagsBridge(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->chrome_feature_flags()->AddObserver(this);
  arc_bridge_service_->chrome_feature_flags()->SetHost(this);
}

ArcChromeFeatureFlagsBridge::~ArcChromeFeatureFlagsBridge() {
  arc_bridge_service_->chrome_feature_flags()->RemoveObserver(this);
  arc_bridge_service_->chrome_feature_flags()->SetHost(nullptr);
}

void ArcChromeFeatureFlagsBridge::OnConnectionReady() {
  NotifyFeatureFlags();
}

void ArcChromeFeatureFlagsBridge::IsFeatureEnabled(
    const std::string& feature_name,
    IsFeatureEnabledCallback callback) {
  const base::Feature* feature = GetArcFeatureByName(feature_name);
  if (feature) {
    std::move(callback).Run(base::FeatureList::IsEnabled(*feature));
  } else {
    std::move(callback).Run(absl::nullopt);
  }
}

void ArcChromeFeatureFlagsBridge::GetIntParamByFeatureAndParamName(
    const std::string& feature_name,
    const std::string& param_name,
    GetIntParamByFeatureAndParamNameCallback callback) {
  const base::Feature* feature = GetArcFeatureByName(feature_name);
  if (!feature) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  std::move(callback).Run(GetFieldTrialIntIfAvailable(feature, param_name));
}

void ArcChromeFeatureFlagsBridge::GetDoubleParamByFeatureAndParamName(
    const std::string& feature_name,
    const std::string& param_name,
    GetDoubleParamByFeatureAndParamNameCallback callback) {
  const base::Feature* feature = GetArcFeatureByName(feature_name);
  if (!feature) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  std::move(callback).Run(GetFieldTrialDoubleIfAvailable(feature, param_name));
}

void ArcChromeFeatureFlagsBridge::GetBoolParamByFeatureAndParamName(
    const std::string& feature_name,
    const std::string& param_name,
    GetBoolParamByFeatureAndParamNameCallback callback) {
  const base::Feature* feature = GetArcFeatureByName(feature_name);
  if (!feature) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  std::move(callback).Run(GetFieldTrialBoolIfAvailable(feature, param_name));
}

void ArcChromeFeatureFlagsBridge::NotifyFeatureFlags() {
  mojom::ChromeFeatureFlagsInstance* chrome_feature_flags_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->chrome_feature_flags(),
                                  NotifyFeatureFlags);
  if (!chrome_feature_flags_instance) {
    return;
  }
  mojom::FeatureFlagsPtr flags = mojom::FeatureFlags::New();
  flags->qs_revamp = ash::features::IsQsRevampEnabled();
  flags->jelly_colors = chromeos::features::IsJellyEnabled();
  flags->touchscreen_emulation =
      base::FeatureList::IsEnabled(kTouchscreenEmulation);
  flags->trackpad_scroll_touchscreen_emulation =
      base::FeatureList::IsEnabled(kTrackpadScrollTouchscreenEmulation);
  flags->rounded_window_compat_strategy =
      base::FeatureList::IsEnabled(arc::kRoundedWindowCompat)
          ? static_cast<mojom::RoundedWindowCompatStrategy>(
                base::GetFieldTrialParamByFeatureAsInt(
                    kRoundedWindowCompat, kRoundedWindowCompatStrategy,
                    static_cast<int>(mojom::RoundedWindowCompatStrategy::
                                         kLeftRightBottomGesture)))
          : mojom::RoundedWindowCompatStrategy::kDisabled;
  flags->rounded_window_radius = chromeos::features::RoundedWindowsRadius();
  flags->xdg_mode = base::FeatureList::IsEnabled(kXdgMode);
  flags->enable_pip_double_tap = ash::features::IsPipDoubleTapToResizeEnabled();

  chrome_feature_flags_instance->NotifyFeatureFlags(std::move(flags));
}

// static
void ArcChromeFeatureFlagsBridge::EnsureFactoryBuilt() {
  ArcChromeFeatureFlagsBridgeFactory::GetInstance();
}

}  // namespace arc
