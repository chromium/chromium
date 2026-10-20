// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_engagement/android_blue_bubble_configuration_provider.h"

#include <algorithm>
#include <optional>
#include <set>

#include "base/feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/feature_constants.h"

namespace feature_engagement {

namespace {

constexpr uint32_t kMaxLifetimeTriggers = 3;
constexpr uint32_t kLifetimeWindowDays = 360;
constexpr uint32_t kMinCooldownDays = 7;

// Returns whether `feature` is an Android Blue Bubble IPH onboarded to the
// Blue Bubble cooldown group.
// LINT.IfChange(BlueBubbleCooldownFeatures)
bool IsBlueBubbleFeature(const base::Feature& feature) {
  static constexpr const base::Feature* kBlueBubbleFeatures[] = {
      &kIPHBottomToolbarTipFeature,
      &kIPHCCTMinimized,
      &kIPHReadLaterAppMenuBookmarkThisPageFeature,
      &kIPHSiteControlsFeature,
      &kIPHTabSwitcherButtonFeature,
  };
  return std::ranges::contains(kBlueBubbleFeatures, &feature);
}
// LINT.ThenChange(//chrome/browser/user_education/java/src/org/chromium/chrome/browser/user_education/UserEducationHelper.java:BlueBubbleCooldownFeatures)

// Returns the maximum number of times a trigger event can occur while still
// satisfying `comparator` on the next check, or `std::nullopt` if unbounded
// from above.
std::optional<uint32_t> GetMaxAllowedTriggers(const Comparator& comparator) {
  switch (comparator.type) {
    case EQUAL:
      return comparator.value == 0 ? std::optional<uint32_t>(1) : std::nullopt;
    case LESS_THAN:
      return comparator.value;
    case LESS_THAN_OR_EQUAL:
      return comparator.value + 1;
    default:
      return std::nullopt;
  }
}

bool HasLifetimeCap(const EventConfig& event_config) {
  std::optional<uint32_t> max_allowed =
      GetMaxAllowedTriggers(event_config.comparator);
  return max_allowed.has_value() && *max_allowed <= kMaxLifetimeTriggers &&
         event_config.window >= kLifetimeWindowDays;
}

bool HasCooldownFloor(const EventConfig& event_config) {
  std::optional<uint32_t> max_allowed =
      GetMaxAllowedTriggers(event_config.comparator);
  return max_allowed.has_value() && *max_allowed <= 1 &&
         event_config.window >= kMinCooldownDays;
}

}  // namespace

AndroidBlueBubbleConfigurationProvider::
    AndroidBlueBubbleConfigurationProvider() = default;

AndroidBlueBubbleConfigurationProvider::
    ~AndroidBlueBubbleConfigurationProvider() = default;

bool AndroidBlueBubbleConfigurationProvider::MaybeProvideFeatureConfiguration(
    const base::Feature& feature,
    FeatureConfig& config,
    const FeatureVector& known_features,
    const GroupVector& known_groups) const {
  if (!base::FeatureList::IsEnabled(
          chrome::android::kBlueBubbleIphCooldownGroup) ||
      !config.valid || config.trigger.name.empty() ||
      !IsBlueBubbleFeature(feature)) {
    return false;
  }

  const std::string& trigger_name = config.trigger.name;
  bool has_lifetime_cap = HasLifetimeCap(config.trigger);
  bool has_cooldown_floor = HasCooldownFloor(config.trigger);

  for (const auto& ec : config.event_configs) {
    if (ec.name != trigger_name) {
      continue;
    }
    has_lifetime_cap |= HasLifetimeCap(ec);
    has_cooldown_floor |= HasCooldownFloor(ec);
  }

  bool modified = false;

  if (!has_lifetime_cap) {
    // Remove any looser lifetime caps on the trigger event from event_configs.
    std::erase_if(config.event_configs, [&trigger_name](const EventConfig& ec) {
      if (ec.name != trigger_name || ec.window < kLifetimeWindowDays) {
        return false;
      }
      std::optional<uint32_t> max_allowed =
          GetMaxAllowedTriggers(ec.comparator);
      return !max_allowed.has_value() || *max_allowed > kMaxLifetimeTriggers;
    });

    uint32_t storage = std::max(config.trigger.storage, kLifetimeWindowDays);
    if (config.trigger.window >= kLifetimeWindowDays) {
      config.trigger.comparator = Comparator(LESS_THAN, kMaxLifetimeTriggers);
      config.trigger.storage = storage;
    } else {
      config.trigger.storage = storage;
      config.event_configs.insert(
          EventConfig(trigger_name, Comparator(LESS_THAN, kMaxLifetimeTriggers),
                      /*window=*/kLifetimeWindowDays, /*storage=*/storage));
    }
    modified = true;
  }

  if (!has_cooldown_floor) {
    // Remove any shorter-window 1-trigger cooldowns on the trigger event from
    // event_configs before enforcing the 7-day floor.
    std::erase_if(config.event_configs, [&trigger_name](const EventConfig& ec) {
      if (ec.name != trigger_name || ec.window >= kMinCooldownDays) {
        return false;
      }
      std::optional<uint32_t> max_allowed =
          GetMaxAllowedTriggers(ec.comparator);
      return max_allowed.has_value() && *max_allowed <= 1;
    });

    uint32_t storage = std::max(config.trigger.storage, kLifetimeWindowDays);
    std::optional<uint32_t> trigger_max =
        GetMaxAllowedTriggers(config.trigger.comparator);
    if (trigger_max.has_value() && *trigger_max <= 1 &&
        config.trigger.window < kMinCooldownDays) {
      config.trigger.window = kMinCooldownDays;
      config.trigger.storage = storage;
    } else {
      config.trigger.storage = storage;
      config.event_configs.insert(
          EventConfig(trigger_name, Comparator(EQUAL, 0),
                      /*window=*/kMinCooldownDays, /*storage=*/storage));
    }
    modified = true;
  }

  return modified;
}

const char*
AndroidBlueBubbleConfigurationProvider::GetConfigurationSourceDescription()
    const {
  return "Android Blue Bubble frequency limits";
}

}  // namespace feature_engagement
