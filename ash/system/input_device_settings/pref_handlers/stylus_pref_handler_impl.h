// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_STYLUS_PREF_HANDLER_IMPL_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_STYLUS_PREF_HANDLER_IMPL_H_

#include "ash/ash_export.h"
#include "ash/system/input_device_settings/pref_handlers/stylus_pref_handler.h"

class PrefService;

namespace ash {

class ASH_EXPORT StylusPrefHandlerImpl : public StylusPrefHandler {
 public:
  StylusPrefHandlerImpl();
  StylusPrefHandlerImpl(const StylusPrefHandlerImpl&) = delete;
  StylusPrefHandlerImpl& operator=(const StylusPrefHandlerImpl&) = delete;
  ~StylusPrefHandlerImpl() override;

  // StylusPrefHandler:
  void InitializeStylusSettings(PrefService* pref_service,
                                mojom::Stylus* stylus) override;
  void UpdateStylusSettings(PrefService* pref_service,
                            const mojom::Stylus& stylus) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_PREF_HANDLERS_STYLUS_PREF_HANDLER_IMPL_H_
