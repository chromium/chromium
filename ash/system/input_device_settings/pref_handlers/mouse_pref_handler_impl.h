// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_MOUSE_PREF_HANDLER_IMPL_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_MOUSE_PREF_HANDLER_IMPL_H_

#include "ash/ash_export.h"
#include "ash/system/input_device_settings/pref_handlers/mouse_pref_handler.h"
#include "base/values.h"

class PrefService;

namespace ash {

class ASH_EXPORT MousePrefHandlerImpl : public MousePrefHandler {
 public:
  MousePrefHandlerImpl();
  MousePrefHandlerImpl(const MousePrefHandlerImpl&) = delete;
  MousePrefHandlerImpl& operator=(const MousePrefHandlerImpl&) = delete;
  ~MousePrefHandlerImpl() override;

  // MousePrefHandler:
  void InitializeMouseSettings(PrefService* pref_service,
                               const mojom::MousePolicies& mouse_policies,
                               mojom::Mouse* mouse) override;

  void InitializeLoginScreenMouseSettings(
      PrefService* local_state,
      const AccountId& account_id,
      const mojom::MousePolicies& mouse_policies,
      mojom::Mouse* mouse) override;

  void InitializeWithDefaultMouseSettings(
      const mojom::MousePolicies& mouse_policies,
      mojom::Mouse* mouse) override;

  void UpdateMouseSettings(PrefService* pref_service,
                           const mojom::MousePolicies& mouse_policies,
                           const mojom::Mouse& mouse) override;

  void UpdateLoginScreenMouseSettings(
      PrefService* local_state,
      const AccountId& account_id,
      const mojom::MousePolicies& mouse_policies,
      const mojom::Mouse& mouse) override;

  void UpdateDefaultMouseSettings(PrefService* pref_service,
                                  const mojom::MousePolicies& mouse_policies,
                                  const mojom::Mouse& mouse) override;

  void ForceInitializeWithDefaultSettings(
      PrefService* pref_service,
      const mojom::MousePolicies& mouse_policies,
      mojom::Mouse* mouse) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_MOUSE_PREF_HANDLER_IMPL_H_
