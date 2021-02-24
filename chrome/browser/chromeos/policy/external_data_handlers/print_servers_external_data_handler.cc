// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/external_data_handlers/print_servers_external_data_handler.h"

#include <utility>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/printing/print_servers_provider.h"
#include "chrome/browser/chromeos/printing/print_servers_provider_factory.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "components/policy/policy_constants.h"

namespace policy {

namespace {

base::WeakPtr<chromeos::PrintServersProvider> GetPrintServersProvider(
    const std::string& user_id) {
  return chromeos::PrintServersProviderFactory::Get()->GetForAccountId(
      CloudExternalDataPolicyHandler::GetAccountId(user_id));
}

}  // namespace

PrintServersExternalDataHandler::PrintServersExternalDataHandler(
    chromeos::CrosSettings* cros_settings,
    DeviceLocalAccountPolicyService* policy_service)
    : print_servers_observer_(cros_settings,
                              policy_service,
                              key::kExternalPrintServers,
                              this) {
  print_servers_observer_.Init();
}

PrintServersExternalDataHandler::~PrintServersExternalDataHandler() = default;

void PrintServersExternalDataHandler::OnExternalDataSet(
    const std::string& policy,
    const std::string& user_id) {
  GetPrintServersProvider(user_id)->ClearData();
}

void PrintServersExternalDataHandler::OnExternalDataCleared(
    const std::string& policy,
    const std::string& user_id) {
  GetPrintServersProvider(user_id)->ClearData();
}

void PrintServersExternalDataHandler::OnExternalDataFetched(
    const std::string& policy,
    const std::string& user_id,
    std::unique_ptr<std::string> data,
    const base::FilePath& file_path) {
  GetPrintServersProvider(user_id)->SetData(std::move(data));
}

void PrintServersExternalDataHandler::RemoveForAccountId(
    const AccountId& account_id) {
  chromeos::PrintServersProviderFactory::Get()->RemoveForAccountId(account_id);
}

}  // namespace policy
