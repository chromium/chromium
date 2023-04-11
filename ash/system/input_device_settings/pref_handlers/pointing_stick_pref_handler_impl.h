// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_POINTING_STICK_PREF_HANDLER_IMPL_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_POINTING_STICK_PREF_HANDLER_IMPL_H_

#include "ash/ash_export.h"
#include "ash/system/input_device_settings/pref_handlers/pointing_stick_pref_handler.h"
#include "base/values.h"

class PrefService;

namespace ash {

class ASH_EXPORT PointingStickPrefHandlerImpl
    : public PointingStickPrefHandler {
 public:
  PointingStickPrefHandlerImpl();
  PointingStickPrefHandlerImpl(const PointingStickPrefHandlerImpl&) = delete;
  PointingStickPrefHandlerImpl& operator=(const PointingStickPrefHandlerImpl&) =
      delete;
  ~PointingStickPrefHandlerImpl() override;

  // PointingStickPrefHandler:
  void InitializePointingStickSettings(
      PrefService* pref_service,
      mojom::PointingStick* pointing_stick) override;

  void InitializeLoginScreenPointingStickSettings(
      PrefService* local_state,
      const AccountId& account_id,
      mojom::PointingStick* pointing_stick) override;

  void UpdatePointingStickSettings(
      PrefService* pref_service,
      const mojom::PointingStick& pointing_stick) override;

  void UpdateLoginScreenPointingStickSettings(
      PrefService* local_state,
      const AccountId& account_id,
      const mojom::PointingStick& pointing_stick) override;

  void InitializeWithDefaultPointingStickSettings(
      mojom::PointingStick* pointing_stick) override;
};
}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_POINTING_STICK_PREF_HANDLER_IMPL_H_
