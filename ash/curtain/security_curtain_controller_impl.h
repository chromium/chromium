// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CURTAIN_SECURITY_CURTAIN_CONTROLLER_IMPL_H_
#define ASH_CURTAIN_SECURITY_CURTAIN_CONTROLLER_IMPL_H_

#include "ash/curtain/security_curtain_controller.h"

#include <memory>

#include "ash/ash_export.h"
#include "ash/curtain/session.h"
#include "base/memory/raw_ref.h"

namespace ash {
class Shell;
}  // namespace ash

namespace ash::curtain {

class ASH_EXPORT SecurityCurtainControllerImpl
    : public SecurityCurtainController {
 public:
  explicit SecurityCurtainControllerImpl(ash::Shell* shell);
  SecurityCurtainControllerImpl(const SecurityCurtainControllerImpl&) = delete;
  SecurityCurtainControllerImpl& operator=(
      const SecurityCurtainControllerImpl&) = delete;
  ~SecurityCurtainControllerImpl() override;

  // SecurityCurtainController implementation:
  void Enable(InitParams params) override;
  void Disable() override;
  bool IsEnabled() const override;

 private:
  // Only present while the security curtain is enabled.
  std::unique_ptr<Session> session_;

  raw_ref<Shell> shell_;
};

}  // namespace ash::curtain

#endif  // ASH_CURTAIN_SECURITY_CURTAIN_CONTROLLER_IMPL_H_
