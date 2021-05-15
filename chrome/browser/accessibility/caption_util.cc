// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/caption_util.h"

#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/common/pref_names_util.h"
#include "components/prefs/pref_service.h"
#include "ui/base/ui_base_switches.h"
#include "ui/native_theme/native_theme.h"

namespace {

// Returns whether the style is default or not. If the user has changed any of
// the captions settings from the default value, that is an interesting metric
// to observe.
bool IsDefaultStyle(absl::optional<ui::CaptionStyle> style) {
  return (style.has_value() && style->text_size.empty() &&
          style->font_family.empty() && style->text_color.empty() &&
          style->background_color.empty() && style->text_shadow.empty());
}

}  // namespace

namespace captions {

absl::optional<ui::CaptionStyle> GetCaptionStyleFromUserSettings(
    PrefService* prefs,
    bool record_metrics) {
  // Apply native CaptionStyle parameters.
  absl::optional<ui::CaptionStyle> style;

  // Apply native CaptionStyle parameters.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kForceCaptionStyle)) {
    style = ui::CaptionStyle::FromSpec(
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kForceCaptionStyle));
  }

  // Apply system caption style.
  if (!style) {
    ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForWeb();
    style = native_theme->GetSystemCaptionStyle();
    if (record_metrics && style.has_value()) {
      base::UmaHistogramBoolean(
          "Accessibility.CaptionSettingsLoadedFromSystemSettings",
          !IsDefaultStyle(style));
    }
  }

  // Apply caption style from preferences if system caption style is undefined.
  if (!style) {
    style = pref_names_util::GetCaptionStyleFromPrefs(prefs);
    if (record_metrics && style.has_value()) {
      base::UmaHistogramBoolean("Accessibility.CaptionSettingsLoadedFromPrefs",
                                !IsDefaultStyle(style));
    }
  }

  return style;
}

}  // namespace captions
