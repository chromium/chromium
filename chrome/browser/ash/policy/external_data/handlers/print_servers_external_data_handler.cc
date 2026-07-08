// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/external_data/handlers/print_servers_external_data_handler.h"

#include <utility>

#include "base/check_deref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/printing/enterprise/print_servers_provider.h"
#include "chrome/browser/ash/printing/enterprise/print_servers_provider_factory.h"

namespace policy {

namespace {

base::WeakPtr<ash::PrintServersProvider> GetPrintServersProvider(
    PrefService& local_state,
    const std::string& user_id) {
  return ash::PrintServersProviderFactory::Get()->GetForAccountId(
      CloudExternalDataPolicyObserver::GetAccountId(local_state, user_id));
}

}  // namespace

PrintServersExternalDataHandler::PrintServersExternalDataHandler(
    PrefService* local_state)
    : local_state_(CHECK_DEREF(local_state)) {}

PrintServersExternalDataHandler::~PrintServersExternalDataHandler() = default;

void PrintServersExternalDataHandler::OnExternalDataSet(
    const std::string& policy,
    const std::string& user_id) {
  GetPrintServersProvider(local_state_.get(), user_id)->ClearData();
}

void PrintServersExternalDataHandler::OnExternalDataCleared(
    const std::string& policy,
    const std::string& user_id) {
  GetPrintServersProvider(local_state_.get(), user_id)->ClearData();
}

void PrintServersExternalDataHandler::OnExternalDataFetched(
    const std::string& policy,
    const std::string& user_id,
    std::unique_ptr<std::string> data,
    const base::FilePath& file_path) {
  GetPrintServersProvider(local_state_.get(), user_id)
      ->SetData(std::move(data));
}

void PrintServersExternalDataHandler::RemoveForAccountId(
    const AccountId& account_id) {
  ash::PrintServersProviderFactory::Get()->RemoveForAccountId(account_id);
}

}  // namespace policy
