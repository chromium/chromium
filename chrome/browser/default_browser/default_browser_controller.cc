// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_controller.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/default_browser/default_browser_manager.h"
#include "chrome/browser/default_browser/default_browser_setter.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "default_browser_setter.h"

namespace default_browser {

namespace {

std::string GetEntrypointHistogramName(
    DefaultBrowserEntrypointType entrypoint_type,
    DefaultBrowserSetterType setter_type,
    const std::string& metric_type) {
  return base::JoinString(
      {"DefaultBrowser", UiEntrypointTypeToString(entrypoint_type),
       SetterTypeToString(setter_type), metric_type},
      ".");
}

std::string GetSetterHistogramName(DefaultBrowserSetterType setter_type,
                                   const std::string& metric_type) {
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
    default:
      NOTREACHED();
  }
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
    default:
      NOTREACHED();
  }
}

DefaultBrowserController::DefaultBrowserController(
    std::unique_ptr<DefaultBrowserSetter> setter,
    DefaultBrowserEntrypointType ui_entrypoint)
    : setter_(std::move(setter)), ui_entrypoint_(ui_entrypoint) {}

DefaultBrowserController::~DefaultBrowserController() = default;

DefaultBrowserSetterType DefaultBrowserController::GetSetterType() const {
  return setter_->GetType();
}

void DefaultBrowserController::OnShown() {
  IncrementShownMetric();
}

void DefaultBrowserController::OnAccepted(
    DefaultBrowserControllerCompletionCallback completion_callback) {
  RecordInteractionMetric(DefaultBrowserInteractionType::kAccepted);

  setter_execution_start_time_ = base::TimeTicks::Now();

  completion_callback_ = std::move(completion_callback);
  setter_->Execute(
      base::BindOnce(&DefaultBrowserController::OnSetterExecutionComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DefaultBrowserController::OnIgnored() {
  RecordInteractionMetric(DefaultBrowserInteractionType::kIgnored);
}

void DefaultBrowserController::OnDismissed() {
  RecordInteractionMetric(DefaultBrowserInteractionType::kDismissed);
}

void DefaultBrowserController::OnSetterExecutionComplete(
    DefaultBrowserState default_browser_state) {
  bool success = default_browser_state == DefaultBrowserState::IS_DEFAULT;
  RecordResultMetric(success);

  if (success) {
    std::string duration_histogram_name =
        GetSetterHistogramName(GetSetterType(), "SuccessDuration");
    base::UmaHistogramLongTimes(
        duration_histogram_name,
        base::TimeTicks::Now() - setter_execution_start_time_);

    BrowserWindowInterface* browser =
        GlobalBrowserCollection::GetInstance()->GetLastActiveBrowser();
    if (browser) {
      ToastController* toast_controller =
          browser->GetFeatures().toast_controller();
      if (toast_controller) {
        toast_controller->MaybeShowToast(
            ToastParams(ToastId::kDefaultBrowserUpdateSuccess));
      }
    }
  } else if (auto* manager = DefaultBrowserManager::From(g_browser_process)) {
    manager->TrackTimeAfterSetterFailure(ui_entrypoint_, GetSetterType());
  }

  std::move(completion_callback_).Run(default_browser_state);
}

void DefaultBrowserController::IncrementShownMetric() {
  base::UmaHistogramCounts100(
      GetEntrypointHistogramName(ui_entrypoint_, GetSetterType(), "Shown"), 1);
}

void DefaultBrowserController::RecordInteractionMetric(
    DefaultBrowserInteractionType interaction) {
  base::UmaHistogramEnumeration(
      GetEntrypointHistogramName(ui_entrypoint_, GetSetterType(),
                                 "Interaction"),
      interaction);
}

void DefaultBrowserController::RecordResultMetric(bool success) {
  base::UmaHistogramBoolean(GetSetterHistogramName(GetSetterType(), "Result"),
                            success);
}

}  // namespace default_browser
