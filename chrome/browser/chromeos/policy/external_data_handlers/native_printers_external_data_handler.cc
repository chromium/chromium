// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/external_data_handlers/native_printers_external_data_handler.h"

#include <utility>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/printing/bulk_printers_calculator.h"
#include "chrome/browser/chromeos/printing/bulk_printers_calculator_factory.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "components/policy/policy_constants.h"

namespace policy {

namespace {

base::WeakPtr<chromeos::BulkPrintersCalculator> GetBulkPrintersCalculator(
    const std::string& user_id) {
  return chromeos::BulkPrintersCalculatorFactory::Get()->GetForAccountId(
      CloudExternalDataPolicyHandler::GetAccountId(user_id));
}

}  // namespace

NativePrintersExternalDataHandler::NativePrintersExternalDataHandler(
    chromeos::CrosSettings* cros_settings,
    DeviceLocalAccountPolicyService* policy_service)
    : native_printers_observer_(cros_settings,
                                policy_service,
                                key::kNativePrintersBulkConfiguration,
                                this) {
  native_printers_observer_.Init();
}

NativePrintersExternalDataHandler::~NativePrintersExternalDataHandler() =
    default;

void NativePrintersExternalDataHandler::OnExternalDataSet(
    const std::string& policy,
    const std::string& user_id) {
  GetBulkPrintersCalculator(user_id)->ClearData();
}

void NativePrintersExternalDataHandler::OnExternalDataCleared(
    const std::string& policy,
    const std::string& user_id) {
  GetBulkPrintersCalculator(user_id)->ClearData();
}

void NativePrintersExternalDataHandler::OnExternalDataFetched(
    const std::string& policy,
    const std::string& user_id,
    std::unique_ptr<std::string> data,
    const base::FilePath& file_path) {
  GetBulkPrintersCalculator(user_id)->SetData(std::move(data));
}

void NativePrintersExternalDataHandler::RemoveForAccountId(
    const AccountId& account_id) {
  chromeos::BulkPrintersCalculatorFactory::Get()->RemoveForUserId(account_id);
}

}  // namespace policy
