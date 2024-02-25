// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_ui_settings.h"

#include <utility>

#include "ash/ambient/util/ambient_util.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace {

using ash::personalization_app::mojom::AmbientTheme;

// Validity is always checked for both |AmbientTheme| and |AmbientVideo| in case
// pref storage is corrupted. Both use the integer representation of their enum
// values.
template <class T>
bool EnumInRange(T val) {
  int val_as_int = static_cast<int>(val);
  return val_as_int >= 0 && val_as_int <= static_cast<int>(T::kMaxValue);
}

}  // namespace

// static
std::optional<AmbientUiSettings> AmbientUiSettings::CreateFromDict(
    const base::Value::Dict& dict) {
  std::optional<int> theme_as_int =
      dict.FindInt(ambient::prefs::kAmbientUiSettingsFieldTheme);
  if (!theme_as_int) {
    return std::nullopt;
  }
  AmbientUiSettings settings;
  settings.theme_ = static_cast<AmbientTheme>(*theme_as_int);
  std::optional<int> video_as_int =
      dict.FindInt(ambient::prefs::kAmbientUiSettingsFieldVideo);
  if (video_as_int) {
    settings.video_ = static_cast<AmbientVideo>(*video_as_int);
  }
  if (settings.IsValid()) {
    return settings;
  } else {
    return std::nullopt;
  }
}

// static
AmbientUiSettings AmbientUiSettings::ReadFromPrefService(
    PrefService& pref_service) {
  const base::Value::Dict& settings_dict =
      pref_service.GetDict(ambient::prefs::kAmbientUiSettings);
  std::optional<AmbientUiSettings> settings_loaded =
      CreateFromDict(settings_dict);
  if (settings_loaded) {
    return *settings_loaded;
  } else {
    if (!settings_dict.empty()) {
      // This should only happen if pref storage was corrupted on disc.
      LOG(ERROR)
          << "Loaded invalid AmbientUiSettings from pref. Using default.";
      pref_service.ClearPref(ambient::prefs::kAmbientUiSettings);
    } else if (features::IsTimeOfDayScreenSaverEnabled()) {
      return AmbientUiSettings(AmbientTheme::kVideo, kDefaultAmbientVideo);
    }
    return AmbientUiSettings();
  }
}

AmbientUiSettings::AmbientUiSettings() = default;

AmbientUiSettings::AmbientUiSettings(AmbientTheme theme,
                                     std::optional<AmbientVideo> video)
    : theme_(theme), video_(std::move(video)) {
  CHECK(IsValid());
}

AmbientUiSettings::AmbientUiSettings(const AmbientUiSettings&) = default;

AmbientUiSettings& AmbientUiSettings::operator=(const AmbientUiSettings&) =
    default;

AmbientUiSettings::~AmbientUiSettings() = default;

bool AmbientUiSettings::operator==(const AmbientUiSettings& other) const {
  return theme_ == other.theme_ && video_ == other.video_;
}

bool AmbientUiSettings::operator!=(const AmbientUiSettings& other) const {
  return !(*this == other);
}

void AmbientUiSettings::WriteToPrefService(PrefService& pref_service) const {
  base::Value::Dict dict;
  dict.Set(ambient::prefs::kAmbientUiSettingsFieldTheme,
           static_cast<int>(theme_));
  if (video_) {
    dict.Set(ambient::prefs::kAmbientUiSettingsFieldVideo,
             static_cast<int>(*video_));
  }
  pref_service.SetDict(ambient::prefs::kAmbientUiSettings, std::move(dict));
}

std::string AmbientUiSettings::ToString() const {
  std::string output(ambient::util::AmbientThemeToString(theme_));
  if (theme_ == AmbientTheme::kVideo) {
    CHECK(video_);
    base::StrAppend(&output, {".", ash::ToString(*video_)});
  }
  return output;
}

bool AmbientUiSettings::IsValid() const {
  if (!EnumInRange(theme_)) {
    return false;
  }
  switch (theme_) {
    case AmbientTheme::kSlideshow:
    case AmbientTheme::kFeelTheBreeze:
    case AmbientTheme::kFloatOnBy:
      // If the "video" field is set, that's OK. It will just be ignored.
      return true;
    case AmbientTheme::kVideo:
      return video_.has_value() && EnumInRange(*video_);
  }
}

}  // namespace ash
