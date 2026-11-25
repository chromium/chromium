// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_metrics.h"

#include <string>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"

namespace default_browser {

namespace {

std::string GetSetterHistogramName(DefaultBrowserSetterType setter_type,
                                   std::string_view metric_type) {
  return base::JoinString(
      {"DefaultBrowser", SetterTypeToString(setter_type), metric_type}, ".");
}

}  // namespace

std::string SetterTypeToString(DefaultBrowserSetterType setter_type) {
  switch (setter_type) {
    case DefaultBrowserSetterType::kShellIntegration:
      return "ShellIntegration";
    case DefaultBrowserSetterType::kVisualGuide:
      return "VisualGuide";
    case DefaultBrowserSetterType::kAnnotatedShellIntegration:
      return "AnnotatedShellIntegration";
  }
  NOTREACHED();
}

std::string UiEntrypointTypeToString(
    DefaultBrowserEntrypointType ui_entrypoint) {
  switch (ui_entrypoint) {
    case DefaultBrowserEntrypointType::kSettingsPage:
      return "SettingsPage";
    case DefaultBrowserEntrypointType::kStartupInfobar:
      return "InfoBar";
    case DefaultBrowserEntrypointType::kChangeDetectedNotification:
      return "ChangeDetectedNotification";
    case DefaultBrowserEntrypointType::kBubbleDialog:
      return "BubbleDialog";
    case DefaultBrowserEntrypointType::kModalDialogWithSettingsIllustration:
      return "ModalDialogWithSettingsIllustration";
    case DefaultBrowserEntrypointType::kModalDialogWithoutSettingsIllustration:
      return "ModalDialogWithoutSettingsIllustration";
    case DefaultBrowserEntrypointType::
        kStickyModalDialogWithSettingsIllustration:
      return "StickyModalDialogWithSettingsIllustration";
    case DefaultBrowserEntrypointType::
        kStickyModalDialogWithoutSettingsIllustration:
      return "StickyModalDialogWithoutSettingsIllustration";
  }
  NOTREACHED();
}

std::string GetEntrypointHistogramName(
    DefaultBrowserEntrypointType entrypoint_type,
    DefaultBrowserSetterType setter_type,
    std::string_view metric_type) {
  return base::JoinString(
      {"DefaultBrowser", UiEntrypointTypeToString(entrypoint_type),
       SetterTypeToString(setter_type), metric_type},
      ".");
}

void RecordSetterResult(DefaultBrowserEntrypointType ui_entrypoint,
                        DefaultBrowserSetterType setter_type,
                        bool success,
                        base::TimeDelta duration) {
  base::UmaHistogramBoolean(GetSetterHistogramName(setter_type, "Result"),
                            success);
  base::UmaHistogramBoolean(
      GetEntrypointHistogramName(ui_entrypoint, setter_type, "OutcomeV2"),
      success);
  if (success) {
    base::UmaHistogramLongTimes(
        GetSetterHistogramName(setter_type, "SuccessDuration"), duration);
  }
}

}  // namespace default_browser
