// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/account_manager/get_account_information_helper.h"

#include <memory>
#include <string>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/lacros/account_manager/account_profile_mapper.h"
#include "chrome/browser/lacros/identity_manager_lacros.h"
#include "ui/gfx/image/image.h"

GetAccountInformationHelper::GetAccountInformationHelper()
    : identity_manager_lacros_(std::make_unique<IdentityManagerLacros>()) {}

GetAccountInformationHelper::GetAccountInformationHelper(
    std::unique_ptr<IdentityManagerLacros> identity_manager_lacros)
    : identity_manager_lacros_(std::move(identity_manager_lacros)) {}

GetAccountInformationHelper::~GetAccountInformationHelper() = default;

GetAccountInformationHelper::GetAccountInformationResult::
    GetAccountInformationResult() {}

GetAccountInformationHelper::GetAccountInformationResult::
    GetAccountInformationResult(const GetAccountInformationResult& other) =
        default;

GetAccountInformationHelper::GetAccountInformationResult::
    ~GetAccountInformationResult() = default;

void GetAccountInformationHelper::Start(
    const std::vector<std::string>& gaia_ids,
    GetAccountInformationCallback callback) {
  DCHECK(!callback_) << "Start() must be called only once.";
  callback_ = std::move(callback);
  DCHECK(callback_);

  if (gaia_ids.empty()) {
    std::move(callback_).Run({});
    // `this` may be deleted.
    return;
  }

  for (auto const& gaia_id : gaia_ids) {
    GetAccountInformationResult uai;
    uai.gaia = gaia_id;
    account_information_[gaia_id] = std::move(uai);
  }

  // We are scheduling 3 callbacks per account (FullName, Email, Image).
  int missing_information_pieces = gaia_ids.size() * 3;

  maybe_trigger_callback_ = base::BarrierClosure(
      missing_information_pieces,
      base::BindOnce(&GetAccountInformationHelper::TriggerCallback,
                     base::Unretained(this)));

  for (auto const& gaia_id : gaia_ids) {
    // Schedule callbacks
    identity_manager_lacros_->GetAccountFullName(
        gaia_id, base::BindOnce(&GetAccountInformationHelper::OnFullName,
                                base::Unretained(this), gaia_id));
    identity_manager_lacros_->GetAccountEmail(
        gaia_id, base::BindOnce(&GetAccountInformationHelper::OnEmail,
                                base::Unretained(this), gaia_id));
    identity_manager_lacros_->GetAccountImage(
        gaia_id, base::BindOnce(&GetAccountInformationHelper::OnAccountImage,
                                base::Unretained(this), gaia_id));
  }
}

void GetAccountInformationHelper::OnFullName(const std::string& gaia_id,
                                             const std::string& full_name) {
  account_information_[gaia_id].full_name = full_name;
  maybe_trigger_callback_.Run();
}

void GetAccountInformationHelper::OnEmail(const std::string& gaia_id,
                                          const std::string& email) {
  account_information_[gaia_id].email = email;
  maybe_trigger_callback_.Run();
}

void GetAccountInformationHelper::OnAccountImage(const std::string& gaia_id,
                                                 const gfx::ImageSkia& image) {
  account_information_[gaia_id].account_image = gfx::Image(image);
  maybe_trigger_callback_.Run();
}

void GetAccountInformationHelper::TriggerCallback() {
  std::vector<GetAccountInformationResult> accounts;
  for (auto account_pair : account_information_) {
    accounts.emplace_back(std::move(account_pair.second));
  }
  std::move(callback_).Run(std::move(accounts));
  // `this` may be deleted.
}
