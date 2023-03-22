// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_TOUCHPAD_PREF_HANDLER_IMPL_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_TOUCHPAD_PREF_HANDLER_IMPL_H_

#include "ash/ash_export.h"
#include "ash/system/input_device_settings/pref_handlers/touchpad_pref_handler.h"
#include "base/values.h"

class PrefService;

namespace ash {

class ASH_EXPORT TouchpadPrefHandlerImpl : public TouchpadPrefHandler {
 public:
  TouchpadPrefHandlerImpl();
  TouchpadPrefHandlerImpl(const TouchpadPrefHandlerImpl&) = delete;
  TouchpadPrefHandlerImpl& operator=(const TouchpadPrefHandlerImpl&) = delete;
  ~TouchpadPrefHandlerImpl() override;

  // TouchpadPrefHandler:
  void InitializeTouchpadSettings(PrefService* pref_service,
                                  mojom::Touchpad* touchpad) override;
  void UpdateTouchpadSettings(PrefService* pref_service,
                              const mojom::Touchpad& touchpad) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_TOUCHPAD_PREF_HANDLER_IMPL_H_
