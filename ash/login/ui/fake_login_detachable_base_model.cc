// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/fake_login_detachable_base_model.h"

#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/public/cpp/session/user_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

FakeLoginDetachableBaseModel::FakeLoginDetachableBaseModel(
    LoginDataDispatcher* data_dispatcher)
    : data_dispatcher_(data_dispatcher) {}

FakeLoginDetachableBaseModel::~FakeLoginDetachableBaseModel() = default;

void FakeLoginDetachableBaseModel::InitLastUsedBases(
    const std::map<AccountId, std::string>& last_used_bases) {
  ASSERT_TRUE(last_used_bases_.empty());
  last_used_bases_ = last_used_bases;
}

std::string FakeLoginDetachableBaseModel::GetLastUsedBase(
    const AccountId& account_id) {
  auto it = last_used_bases_.find(account_id);
  if (it == last_used_bases_.end()) {
    return "";
  }
  return it->second;
}

void FakeLoginDetachableBaseModel::SetPairingStatus(
    DetachableBasePairingStatus pairing_status,
    const std::string& base_id) {
  ASSERT_EQ(pairing_status == DetachableBasePairingStatus::kAuthenticated,
            !base_id.empty());

  current_authenticated_base_ = base_id;
  pairing_status_ = pairing_status;
  data_dispatcher_->SetDetachableBasePairingStatus(pairing_status);
}

DetachableBasePairingStatus FakeLoginDetachableBaseModel::GetPairingStatus() {
  return pairing_status_;
}

bool FakeLoginDetachableBaseModel::PairedBaseMatchesLastUsedByUser(
    const UserInfo& user_info) {
  EXPECT_FALSE(current_authenticated_base_.empty());

  std::string last_used = GetLastUsedBase(user_info.account_id);
  return last_used.empty() || last_used == current_authenticated_base_;
}

bool FakeLoginDetachableBaseModel::SetPairedBaseAsLastUsedByUser(
    const UserInfo& user_info) {
  if (current_authenticated_base_.empty()) {
    return false;
  }

  last_used_bases_[user_info.account_id] = current_authenticated_base_;
  return true;
}

}  // namespace ash
