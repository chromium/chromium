// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_DETACHABLE_BASE_MODEL_H_
#define ASH_LOGIN_UI_LOGIN_DETACHABLE_BASE_MODEL_H_

#include <memory>

#include "ash/ash_export.h"

namespace ash {

class DetachableBaseHandler;
enum class DetachableBasePairingStatus;
struct UserInfo;

// Wrapper around ash::DetachableBaseHandler used by login UI. Exposed as an
// interface to ease faking the detachable base state in login UI tests, and in
// debug login view.
//
// It observes the detachable base pairing status, and informs login data
// dispatcher of pairing status changes.
// It provides methods for comparing the current base to the last based used by
// a user, and setting the last base used by the user.
class ASH_EXPORT LoginDetachableBaseModel {
 public:
  virtual ~LoginDetachableBaseModel() = default;

  static std::unique_ptr<LoginDetachableBaseModel> Create(
      DetachableBaseHandler* detachable_base_handler);

  // Returns the current detachable base pairing status.
  virtual DetachableBasePairingStatus GetPairingStatus() = 0;

  // Checks if the currently paired base is different than the last base used by
  // the user.
  virtual bool PairedBaseMatchesLastUsedByUser(const UserInfo& user_info) = 0;

  // Sets the currently paired base as the last base used by the user.
  virtual bool SetPairedBaseAsLastUsedByUser(const UserInfo& user_info) = 0;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_DETACHABLE_BASE_MODEL_H_
