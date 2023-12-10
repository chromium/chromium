// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_FINGERPRINT_AUTH_FACTOR_MODEL_H_
#define ASH_LOGIN_UI_FINGERPRINT_AUTH_FACTOR_MODEL_H_

#include "ash/login/ui/auth_factor_model.h"
#include "ash/public/cpp/login_types.h"

namespace ash {

class AuthIconView;

// Implements the logic necessary to show Fingerprint as an auth factor on the
// lock screen.
class ASH_EXPORT FingerprintAuthFactorModel : public AuthFactorModel {
 public:
  class Factory {
   public:
    Factory() = default;
    Factory(const Factory&) = delete;
    Factory& operator=(const Factory&) = delete;

    static std::unique_ptr<FingerprintAuthFactorModel> Create(
        FingerprintState state);

    static void SetFactoryForTesting(Factory* factory);

   protected:
    virtual ~Factory() = default;
    virtual std::unique_ptr<FingerprintAuthFactorModel> CreateInstance(
        FingerprintState state) = 0;

   private:
    static Factory* factory_instance_;
  };

  explicit FingerprintAuthFactorModel(FingerprintState state);
  FingerprintAuthFactorModel(FingerprintAuthFactorModel&) = delete;
  FingerprintAuthFactorModel& operator=(FingerprintAuthFactorModel&) = delete;
  ~FingerprintAuthFactorModel() override;

  void SetFingerprintState(FingerprintState state);
  void ResetUIState();
  void NotifyFingerprintAuthResult(bool result);

  // If |available| is false, forces |GetAuthFactorState()| to return
  // |kUnavailable|, otherwise has no effect. Used to hide Fingerprint auth
  // independently of |state_|.
  void set_available(bool available) { available_ = available; }

 private:
  // AuthFactorModel:
  AuthFactorState GetAuthFactorState() const override;
  AuthFactorType GetType() const override;
  int GetLabelId() const override;
  bool ShouldAnnounceLabel() const override;
  int GetAccessibleNameId() const override;
  void DoHandleTapOrClick() override;
  void DoHandleErrorTimeout() override;
  void UpdateIcon(AuthIconView* icon) override;

  FingerprintState state_;
  std::optional<bool> auth_result_;

  // TODO(b/216691052): Change the name of this to be more clear that this is
  // an override on top of |state_|.
  bool available_ = true;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_FINGERPRINT_AUTH_FACTOR_MODEL_H_
