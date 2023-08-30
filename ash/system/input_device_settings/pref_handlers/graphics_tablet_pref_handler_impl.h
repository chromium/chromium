// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_GRAPHICS_TABLET_PREF_HANDLER_IMPL_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_GRAPHICS_TABLET_PREF_HANDLER_IMPL_H_

#include "ash/ash_export.h"
#include "ash/system/input_device_settings/pref_handlers/graphics_tablet_pref_handler.h"

class PrefService;

namespace ash {

class ASH_EXPORT GraphicsTabletPrefHandlerImpl
    : public GraphicsTabletPrefHandler {
 public:
  GraphicsTabletPrefHandlerImpl();
  GraphicsTabletPrefHandlerImpl(const GraphicsTabletPrefHandlerImpl&) = delete;
  GraphicsTabletPrefHandlerImpl& operator=(
      const GraphicsTabletPrefHandlerImpl&) = delete;
  ~GraphicsTabletPrefHandlerImpl() override;

  // GraphicsTabletPrefHandler:
  void InitializeGraphicsTabletSettings(
      PrefService* pref_service,
      mojom::GraphicsTablet* graphics_tablet) override;

  void InitializeLoginScreenGraphicsTabletSettings(
      PrefService* local_state,
      const AccountId& account_id,
      mojom::GraphicsTablet* graphics_tablet) override;

  void UpdateGraphicsTabletSettings(
      PrefService* pref_service,
      const mojom::GraphicsTablet& graphics_tablet) override;

  void UpdateLoginScreenGraphicsTabletSettings(
      PrefService* local_state,
      const AccountId& account_id,
      const mojom::GraphicsTablet& graphics_tablet) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_GRAPHICS_TABLET_PREF_HANDLER_IMPL_H_
