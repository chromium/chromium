// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/bulk_printers_calculator_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/chromeos/printing/bulk_printers_calculator.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"

namespace chromeos {

// static
BulkPrintersCalculatorFactory* BulkPrintersCalculatorFactory::Get() {
  static base::NoDestructor<BulkPrintersCalculatorFactory> instance;
  return instance.get();
}

base::WeakPtr<BulkPrintersCalculator>
BulkPrintersCalculatorFactory::GetForAccountId(const AccountId& account_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = printers_by_user_.find(account_id);
  if (it != printers_by_user_.end())
    return it->second->AsWeakPtr();
  printers_by_user_.emplace(account_id, BulkPrintersCalculator::Create());
  return printers_by_user_[account_id]->AsWeakPtr();
}

void BulkPrintersCalculatorFactory::RemoveForUserId(
    const AccountId& account_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  printers_by_user_.erase(account_id);
}

base::WeakPtr<BulkPrintersCalculator>
BulkPrintersCalculatorFactory::GetForDevice() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (device_printers_)
    return device_printers_->AsWeakPtr();
  device_printers_ = BulkPrintersCalculator::Create();
  return device_printers_->AsWeakPtr();
}

void BulkPrintersCalculatorFactory::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  printers_by_user_.clear();
  device_printers_.reset();
}

BulkPrintersCalculatorFactory::BulkPrintersCalculatorFactory() = default;
BulkPrintersCalculatorFactory::~BulkPrintersCalculatorFactory() = default;

}  // namespace chromeos
