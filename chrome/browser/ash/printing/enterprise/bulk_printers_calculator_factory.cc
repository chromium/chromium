// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/enterprise/bulk_printers_calculator_factory.h"

#include "chrome/browser/ash/printing/enterprise/bulk_printers_calculator.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"

namespace ash {

namespace {

// This class is owned by `ChromeBrowserMainPartsAsh`.
static BulkPrintersCalculatorFactory* g_bulk_printers_factory = nullptr;

}  // namespace

// static
BulkPrintersCalculatorFactory* BulkPrintersCalculatorFactory::Get() {
  return g_bulk_printers_factory;
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
  if (shutdown_) {
    return nullptr;
  }

  if (!device_printers_)
    device_printers_ = BulkPrintersCalculator::Create();
  return device_printers_->AsWeakPtr();
}

void BulkPrintersCalculatorFactory::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!shutdown_);
  shutdown_ = true;
  printers_by_user_.clear();
  device_printers_.reset();
}

BulkPrintersCalculatorFactory::BulkPrintersCalculatorFactory() {
  // Only one factory should exist.
  DCHECK(!g_bulk_printers_factory);
  g_bulk_printers_factory = this;
}

BulkPrintersCalculatorFactory::~BulkPrintersCalculatorFactory() {
  // Ensure that an instance was created sometime in the past.
  DCHECK(g_bulk_printers_factory);
  g_bulk_printers_factory = nullptr;
}

}  // namespace ash
