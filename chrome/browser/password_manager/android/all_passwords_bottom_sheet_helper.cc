// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/all_passwords_bottom_sheet_helper.h"

#include <functional>

#include "base/ranges/algorithm.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"

AllPasswordsBottomSheetHelper::AllPasswordsBottomSheetHelper(
    password_manager::PasswordStoreInterface* profile_store,
    password_manager::PasswordStoreInterface* account_store) {
  if (profile_store) {
    profile_store->GetAllLoginsWithAffiliationAndBrandingInformation(
        weak_ptr_factory_.GetWeakPtr());
  }
  if (account_store) {
    account_store->GetAllLoginsWithAffiliationAndBrandingInformation(
        weak_ptr_factory_.GetWeakPtr());
  }
}

AllPasswordsBottomSheetHelper::~AllPasswordsBottomSheetHelper() = default;

void AllPasswordsBottomSheetHelper::SetLastFocusedFieldType(
    autofill::mojom::FocusedFieldType focused_field_type) {
  last_focused_field_type_ = focused_field_type;
}

void AllPasswordsBottomSheetHelper::SetUpdateCallback(
    base::OnceClosure update_callback) {
  DCHECK(update_callback);
  update_callback_ = std::move(update_callback);
}

void AllPasswordsBottomSheetHelper::ClearUpdateCallback() {
  update_callback_.Reset();
}

void AllPasswordsBottomSheetHelper::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<password_manager::PasswordForm>> results) {
  int results_count = base::ranges::count_if(
      results, std::not_fn(&password_manager::PasswordForm::blocked_by_user));
  available_credentials_ = available_credentials_.value_or(0) + results_count;
  if (available_credentials_.value() == 0) {
    return;  // Don't update if sheet still wouldn't be available.
  }
  if (update_callback_.is_null()) {
    return;  // No update if cannot be triggered right now.
  }
  if (last_focused_field_type_ == autofill::mojom::FocusedFieldType::kUnknown) {
    return;  // Don't update if no valid field was focused.
  }
  std::move(update_callback_).Run();
}
