// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_detachable_base_model.h"

#include "ash/detachable_base/detachable_base_handler.h"
#include "ash/detachable_base/detachable_base_observer.h"
#include "ash/detachable_base/detachable_base_pairing_status.h"
#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/shell.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"

namespace ash {

namespace {

class LoginDetachableBaseModelImpl : public LoginDetachableBaseModel,
                                     public DetachableBaseObserver {
 public:
  explicit LoginDetachableBaseModelImpl(
      DetachableBaseHandler* detachable_base_handler)
      : detachable_base_handler_(detachable_base_handler) {
    detachable_base_observation_.Observe(detachable_base_handler);
  }

  LoginDetachableBaseModelImpl(const LoginDetachableBaseModelImpl&) = delete;
  LoginDetachableBaseModelImpl& operator=(const LoginDetachableBaseModelImpl&) =
      delete;

  ~LoginDetachableBaseModelImpl() override = default;

  // LoginDetachableBaseModel:
  DetachableBasePairingStatus GetPairingStatus() override {
    return detachable_base_handler_->GetPairingStatus();
  }
  bool PairedBaseMatchesLastUsedByUser(const UserInfo& user_info) override {
    return detachable_base_handler_->PairedBaseMatchesLastUsedByUser(user_info);
  }
  bool SetPairedBaseAsLastUsedByUser(const UserInfo& user_info) override {
    return detachable_base_handler_->SetPairedBaseAsLastUsedByUser(user_info);
  }

  // DetachableBaseObserver:
  void OnDetachableBasePairingStatusChanged(
      DetachableBasePairingStatus pairing_status) override {
    Shell::Get()
        ->login_screen_controller()
        ->data_dispatcher()
        ->SetDetachableBasePairingStatus(pairing_status);
  }
  void OnDetachableBaseRequiresUpdateChanged(bool requires_update) override {}

 private:
  raw_ptr<DetachableBaseHandler, LeakedDanglingUntriaged>
      detachable_base_handler_;
  base::ScopedObservation<DetachableBaseHandler, DetachableBaseObserver>
      detachable_base_observation_{this};
};

}  // namespace

// static
std::unique_ptr<LoginDetachableBaseModel> LoginDetachableBaseModel::Create(
    DetachableBaseHandler* detachable_base_handler) {
  return std::make_unique<LoginDetachableBaseModelImpl>(
      detachable_base_handler);
}

}  // namespace ash
