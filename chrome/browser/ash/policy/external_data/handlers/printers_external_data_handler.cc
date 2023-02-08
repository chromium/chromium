// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/external_data/handlers/printers_external_data_handler.h"

#include <utility>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/printing/bulk_printers_calculator.h"
#include "chrome/browser/ash/printing/bulk_printers_calculator_factory.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "components/policy/policy_constants.h"

namespace policy {

namespace {

base::WeakPtr<ash::BulkPrintersCalculator> GetBulkPrintersCalculator(
    const std::string& user_id) {
  auto* factory = ash::BulkPrintersCalculatorFactory::Get();
  if (!factory) {
    return nullptr;
  }
  return factory->GetForAccountId(
      CloudExternalDataPolicyHandler::GetAccountId(user_id));
}

}  // namespace

PrintersExternalDataHandler::PrintersExternalDataHandler(
    ash::CrosSettings* cros_settings,
    DeviceLocalAccountPolicyService* policy_service)
    : printers_observer_(cros_settings,
                         policy_service,
                         key::kPrintersBulkConfiguration,
                         this) {
  printers_observer_.Init();
}

PrintersExternalDataHandler::~PrintersExternalDataHandler() = default;

void PrintersExternalDataHandler::OnExternalDataSet(
    const std::string& policy,
    const std::string& user_id) {
  auto calculator = GetBulkPrintersCalculator(user_id);
  if (calculator) {
    calculator->ClearData();
  }
}

void PrintersExternalDataHandler::OnExternalDataCleared(
    const std::string& policy,
    const std::string& user_id) {
  auto calculator = GetBulkPrintersCalculator(user_id);
  if (calculator) {
    calculator->ClearData();
  }
}

void PrintersExternalDataHandler::OnExternalDataFetched(
    const std::string& policy,
    const std::string& user_id,
    std::unique_ptr<std::string> data,
    const base::FilePath& file_path) {
  auto calculator = GetBulkPrintersCalculator(user_id);
  if (calculator) {
    calculator->SetData(std::move(data));
  }
}

void PrintersExternalDataHandler::RemoveForAccountId(
    const AccountId& account_id,
    base::OnceClosure on_removed) {
  auto* factory = ash::BulkPrintersCalculatorFactory::Get();
  if (factory) {
    factory->RemoveForUserId(account_id);
  }
  std::move(on_removed).Run();
}

}  // namespace policy
