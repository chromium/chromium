// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/display_settings_handler.h"

#include <utility>
#include "ash/public/mojom/constants.mojom.h"
#include "ash/public/mojom/cros_display_config.mojom.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/settings/cros_settings_names.h"
#include "content/public/browser/system_connector.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "services/service_manager/public/cpp/connector.h"

namespace policy {

DisplaySettingsHandler::DisplaySettingsHandler() {
  content::GetSystemConnector()->Connect(
      ash::mojom::kServiceName,
      cros_display_config_.BindNewPipeAndPassReceiver());
}

DisplaySettingsHandler::~DisplaySettingsHandler() = default;

void DisplaySettingsHandler::OnDisplayConfigChanged() {
  RequestDisplaysAndApplyChanges();
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
    settings_observers_.push_back(
        chromeos::CrosSettings::Get()->AddSettingsObserver(
            handler->SettingName(),
            base::BindRepeating(&DisplaySettingsHandler::OnSettingUpdate,
                                base::Unretained(this),
                                base::Unretained(handler.get()))));
  }

  // Make the initial display unit info request. This will be queued until the
  // Ash service is ready.
  cros_display_config_->GetDisplayUnitInfoList(
      false /* single_unified */,
      base::BindOnce(&DisplaySettingsHandler::OnGetInitialDisplayInfo,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DisplaySettingsHandler::OnGetInitialDisplayInfo(
    std::vector<ash::mojom::DisplayUnitInfoPtr> info_list) {
  // Add this as an observer to the mojo service now that it is ready.
  // (We only care about changes that occur after we apply any changes below).
  mojo::PendingAssociatedRemote<ash::mojom::CrosDisplayConfigObserver> observer;
  cros_display_config_observer_receiver_.Bind(
      observer.InitWithNewEndpointAndPassReceiver());
  cros_display_config_->AddObserver(std::move(observer));

  ApplyChanges(std::move(info_list));
}

void DisplaySettingsHandler::RequestDisplaysAndApplyChanges() {
  cros_display_config_->GetDisplayUnitInfoList(
      false /* single_unified */,
      base::BindOnce(&DisplaySettingsHandler::ApplyChanges,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DisplaySettingsHandler::ApplyChanges(
    std::vector<ash::mojom::DisplayUnitInfoPtr> info_list) {
  for (std::unique_ptr<DisplaySettingsPolicyHandler>& handler : handlers_)
    UpdateSettingAndApplyChanges(handler.get(), info_list);
}

void DisplaySettingsHandler::OnSettingUpdate(
    DisplaySettingsPolicyHandler* handler) {
  cros_display_config_->GetDisplayUnitInfoList(
      false /* single_unified */,
      base::BindOnce(&DisplaySettingsHandler::OnConfigurationChangeForHandler,
                     weak_ptr_factory_.GetWeakPtr(), handler));
}

void DisplaySettingsHandler::UpdateSettingAndApplyChanges(
    DisplaySettingsPolicyHandler* handler,
    const std::vector<ash::mojom::DisplayUnitInfoPtr>& info_list) {
  handler->OnSettingUpdate();
  handler->ApplyChanges(cros_display_config_.get(), info_list);
}

void DisplaySettingsHandler::OnConfigurationChangeForHandler(
    DisplaySettingsPolicyHandler* handler,
    std::vector<ash::mojom::DisplayUnitInfoPtr> info_list) {
  UpdateSettingAndApplyChanges(handler, info_list);
}

}  // namespace policy
