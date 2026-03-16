// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/display/display_settings_handler.h"

#include <utility>

#include "ash/display/cros_display_config.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace policy {

DisplaySettingsHandler::DisplaySettingsHandler()
    : cros_display_config_(ash::Shell::Get()->cros_display_config()) {
  CHECK(cros_display_config_);
}

DisplaySettingsHandler::~DisplaySettingsHandler() = default;

void DisplaySettingsHandler::OnDisplayConfigChanged() {
  // TODO(crbug.com/489591497): Redesign how policy enforces display properties.
  // Currently we need to post a task here to avoid an ordering issue with
  // reentrant observers (see the bug for details).
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&DisplaySettingsHandler::RequestDisplaysAndApplyChanges,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DisplaySettingsHandler::RegisterHandler(
    std::unique_ptr<DisplaySettingsPolicyHandler> handler) {
  if (!started_)
    handlers_.push_back(std::move(handler));
}

void DisplaySettingsHandler::Start() {
  if (started_)
    return;
  started_ = true;

  // Register observers for all settings
  for (const auto& handler : handlers_) {
    settings_subscriptions_.push_back(
        ash::CrosSettings::Get()->AddSettingsObserver(
            handler->SettingName(),
            base::BindRepeating(&DisplaySettingsHandler::OnSettingUpdate,
                                base::Unretained(this),
                                base::Unretained(handler.get()))));
  }

  // Make the initial display unit info request.
  std::vector<crosapi::mojom::DisplayUnitInfoPtr> info_list =
      cros_display_config_->GetDisplayUnitInfoList(false /* single_unified */);
  // (We only care about changes that occur after we apply any changes below).
  cros_display_config_observation_.Observe(cros_display_config_);
  ApplyChanges(std::move(info_list));
}

void DisplaySettingsHandler::RequestDisplaysAndApplyChanges() {
  std::vector<crosapi::mojom::DisplayUnitInfoPtr> info_list =
      cros_display_config_->GetDisplayUnitInfoList(/*single_unified=*/false);
  ApplyChanges(std::move(info_list));
}

void DisplaySettingsHandler::ApplyChanges(
    std::vector<crosapi::mojom::DisplayUnitInfoPtr> info_list) {
  for (std::unique_ptr<DisplaySettingsPolicyHandler>& handler : handlers_)
    UpdateSettingAndApplyChanges(handler.get(), info_list);
}

void DisplaySettingsHandler::OnSettingUpdate(
    DisplaySettingsPolicyHandler* handler) {
  std::vector<crosapi::mojom::DisplayUnitInfoPtr> info_list =
      cros_display_config_->GetDisplayUnitInfoList(
          /*single_unified=*/false);
  UpdateSettingAndApplyChanges(handler, info_list);
}

void DisplaySettingsHandler::UpdateSettingAndApplyChanges(
    DisplaySettingsPolicyHandler* handler,
    const std::vector<crosapi::mojom::DisplayUnitInfoPtr>& info_list) {
  handler->OnSettingUpdate();
  handler->ApplyChanges(*cros_display_config_, info_list);
}

}  // namespace policy
