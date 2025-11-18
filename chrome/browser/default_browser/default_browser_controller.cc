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
#include "chrome/browser/default_browser/default_browser_setter.h"

namespace default_browser {

namespace {

std::string GetHistogramName(const std::string& suffix,
                             const std::string& metric_type) {
  return base::JoinString({"DefaultBrowser", suffix, metric_type}, ".");
}

std::string SetterTypeToString(DefaultBrowserSetterType setter_type) {
  switch (setter_type) {
    case DefaultBrowserSetterType::kShellIntegration:
      return "ShellIntegration";
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
    default:
      NOTREACHED();
  }
}

}  // namespace

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
  RecordResultMetric(default_browser_state == DefaultBrowserState::IS_DEFAULT);

  std::move(completion_callback_).Run(default_browser_state);
}

void DefaultBrowserController::IncrementShownMetric() {
  base::UmaHistogramCounts100(
      GetHistogramName(UiEntrypointTypeToString(ui_entrypoint_), "Shown"), 1);
}

void DefaultBrowserController::RecordInteractionMetric(
    DefaultBrowserInteractionType interaction) {
  base::UmaHistogramEnumeration(
      GetHistogramName(UiEntrypointTypeToString(ui_entrypoint_), "Interaction"),
      interaction);
}

void DefaultBrowserController::RecordResultMetric(bool success) {
  base::UmaHistogramBoolean(
      GetHistogramName(SetterTypeToString(GetSetterType()), "Result"), success);
}

}  // namespace default_browser
