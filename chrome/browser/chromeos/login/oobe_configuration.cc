// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/oobe_configuration.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/login/configuration_keys.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/oobe_configuration_client.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/chromeos/input_method_util.h"

namespace chromeos {

// static
OobeConfiguration* OobeConfiguration::instance = nullptr;
bool OobeConfiguration::skip_check_for_testing_ = false;

OobeConfiguration::OobeConfiguration()
    : check_completed_(false),
      configuration_(
          std::make_unique<base::Value>(base::Value::Type::DICTIONARY)) {
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

const base::Value& OobeConfiguration::GetConfiguration() const {
  return *configuration_.get();
}

bool OobeConfiguration::CheckCompleted() const {
  return check_completed_;
}

void OobeConfiguration::ResetConfiguration() {
  configuration_ = std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  if (check_completed_) {
    NotifyObservers();
  }
}

void OobeConfiguration::CheckConfiguration() {
  if (skip_check_for_testing_)
    return;
  DBusThreadManager::Get()
      ->GetOobeConfigurationClient()
      ->CheckForOobeConfiguration(
          base::BindOnce(&OobeConfiguration::OnConfigurationCheck,
                         weak_factory_.GetWeakPtr()));
}

void OobeConfiguration::OnConfigurationCheck(bool has_configuration,
                                             const std::string& configuration) {
  check_completed_ = true;
  if (!has_configuration) {
    NotifyObservers();
    return;
  }

  int error_code, row, col;
  std::string error_message;
  auto value = base::JSONReader::ReadAndReturnErrorDeprecated(
      configuration, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS,
      &error_code, &error_message, &row, &col);
  if (!value) {
    LOG(ERROR) << "Error parsing OOBE configuration: " << error_message;
  } else if (!chromeos::configuration::ValidateConfiguration(*value)) {
    LOG(ERROR) << "Invalid OOBE configuration";
  } else {
    configuration_ = std::move(value);
    UpdateConfigurationValues();
  }
  NotifyObservers();
}

void OobeConfiguration::UpdateConfigurationValues() {
  auto* ime_value = configuration_->FindKeyOfType(configuration::kInputMethod,
                                                  base::Value::Type::STRING);
  if (ime_value) {
    chromeos::input_method::InputMethodManager* imm =
        chromeos::input_method::InputMethodManager::Get();
    configuration_->SetKey(
        configuration::kInputMethod,
        base::Value(imm->GetInputMethodUtil()->MigrateInputMethod(
            ime_value->GetString())));
  }
}

void OobeConfiguration::NotifyObservers() {
  for (auto& observer : observer_list_)
    observer.OnOobeConfigurationChanged();
}

}  // namespace chromeos
