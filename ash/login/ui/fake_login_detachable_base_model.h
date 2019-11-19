// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_FAKE_LOGIN_DETACHABLE_BASE_MODEL_H_
#define ASH_LOGIN_UI_FAKE_LOGIN_DETACHABLE_BASE_MODEL_H_

#include <map>
#include <string>

#include "ash/detachable_base/detachable_base_pairing_status.h"
#include "ash/login/ui/login_detachable_base_model.h"
#include "base/macros.h"
#include "components/account_id/account_id.h"

namespace ash {

class LoginDataDispatcher;

// Fake LoginDetachableBaseModel implementation. To be used in tests to hide
// dependency on DetachableBaseHandler.
class FakeLoginDetachableBaseModel : public LoginDetachableBaseModel {
 public:
  // |data_dispatcher| - the dispatcher to which the pairing status changes
  // should be forwarded.
  explicit FakeLoginDetachableBaseModel(LoginDataDispatcher* data_dispatcher);
  ~FakeLoginDetachableBaseModel() override;

  // Sets the initial mapping for user -> last used detachable base.
  // It will assert if called while |last_used_bases_| is not empty.
  void InitLastUsedBases(
      const std::map<AccountId, std::string>& last_used_bases);

  // Gets the last recorded base for the user with the provided account id.
  // Returns empty string if the user does not have a recorded detcachable base
  // usage.
  std::string GetLastUsedBase(const AccountId& account_id);

  // Changes current detachable base pairing status.
  // |pairing_status| - the new pairing status.
  // |base_id| - the authenticated base ID. It is expect to be set if and only
  //             if pairing_status is kAuthenticated.
  void SetPairingStatus(DetachableBasePairingStatus pairing_status,
                        const std::string& base_id);

  // LoginDetachableBaseModel:
  DetachableBasePairingStatus GetPairingStatus() override;
  bool PairedBaseMatchesLastUsedByUser(const UserInfo& user_info) override;
  bool SetPairedBaseAsLastUsedByUser(const UserInfo& user_info) override;

 private:
  LoginDataDispatcher* data_dispatcher_;

  // Current pairing status.
  DetachableBasePairingStatus pairing_status_ =
      DetachableBasePairingStatus::kNone;

  // The ID if the currently authenticated detachable base.
  std::string current_authenticated_base_;

  // Maps user account Id to the ID of the last used detachable base.
  std::map<AccountId, std::string> last_used_bases_;

  DISALLOW_COPY_AND_ASSIGN(FakeLoginDetachableBaseModel);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_FAKE_LOGIN_DETACHABLE_BASE_MODEL_H_
