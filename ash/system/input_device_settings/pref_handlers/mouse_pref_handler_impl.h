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
                               mojom::Mouse* mouse) override;
  void UpdateMouseSettings(PrefService* pref_service,
                           const mojom::Mouse& mouse) override;

 private:
  mojom::MouseSettingsPtr GetNewMouseSettings(PrefService* prefs,
                                              const mojom::Mouse& Mouse);
  mojom::MouseSettingsPtr RetreiveMouseSettings(
      PrefService* prefs,
      const mojom::Mouse& mouse,
      const base::Value::Dict& settings_dict);
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_MOUSE_PREF_HANDLER_IMPL_H_
