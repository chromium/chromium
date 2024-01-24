// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_configuration.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "chrome/browser/ash/login/configuration_keys.h"
#include "chromeos/ash/components/dbus/oobe_config/oobe_configuration_client.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/input_method_util.h"

namespace ash {

// static
OobeConfiguration* OobeConfiguration::instance = nullptr;
bool OobeConfiguration::skip_check_for_testing_ = false;

OobeConfiguration::OobeConfiguration() : check_completed_(false) {
  DCHECK(!OobeConfiguration::Get());
  OobeConfiguration::instance = this;
}

OobeConfiguration::~OobeConfiguration() {
  DCHECK_EQ(instance, this);
  OobeConfiguration::instance = nullptr;
}

// static
OobeConfiguration* OobeConfiguration::Get() {
  return OobeConfiguration::instance;
}

void OobeConfiguration::AddAndFireObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
  if (check_completed_) {
    observer->OnOobeConfigurationChanged();
  }
}

void OobeConfiguration::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool OobeConfiguration::CheckCompleted() const {
  return check_completed_;
}

void OobeConfiguration::ResetConfiguration() {
  configuration_ = base::Value::Dict();
  if (check_completed_) {
    NotifyObservers();
  }
}

void OobeConfiguration::CheckConfiguration() {
  if (skip_check_for_testing_)
    return;
  OobeConfigurationClient::Get()->CheckForOobeConfiguration(base::BindOnce(
      &OobeConfiguration::OnConfigurationCheck, weak_factory_.GetWeakPtr()));
}

void OobeConfiguration::OnConfigurationCheck(bool has_configuration,
                                             const std::string& configuration) {
  check_completed_ = true;
  if (!has_configuration) {
    NotifyObservers();
    return;
  }

  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      configuration,
      base::JSON_PARSE_CHROMIUM_EXTENSIONS | base::JSON_ALLOW_TRAILING_COMMAS);
  if (!parsed_json.has_value()) {
    LOG(ERROR) << "Error parsing OOBE configuration: "
               << parsed_json.error().message;
  } else if (!parsed_json->is_dict()) {
    LOG(ERROR) << "Configuration should be a dictionary";
  } else if (!configuration::ValidateConfiguration(parsed_json->GetDict())) {
    LOG(ERROR) << "Invalid OOBE configuration";
  } else {
    configuration_ = std::move(*parsed_json).TakeDict();
    UpdateConfigurationValues();
  }
  NotifyObservers();
}

void OobeConfiguration::UpdateConfigurationValues() {
  auto* ime_value = configuration_.FindString(configuration::kInputMethod);
  if (ime_value) {
    auto* imm = input_method::InputMethodManager::Get();
    configuration_.Set(
        configuration::kInputMethod,
        imm->GetInputMethodUtil()->GetMigratedInputMethod(*ime_value));
  }
}

void OobeConfiguration::NotifyObservers() {
  for (auto& observer : observer_list_)
    observer.OnOobeConfigurationChanged();
}

}  // namespace ash
