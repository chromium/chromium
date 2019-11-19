// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SWITCH_ACCESS_EVENT_HANDLER_DELEGATE_H_
#define ASH_PUBLIC_CPP_SWITCH_ACCESS_EVENT_HANDLER_DELEGATE_H_

#include <memory>

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

enum class SwitchAccessCommand;

// Allows a client to implement Switch Access.
class ASH_PUBLIC_EXPORT SwitchAccessEventHandlerDelegate {
 public:
  // Sends a command to Switch Access, based on what key was pressed and the
  // user's settings.
  virtual void SendSwitchAccessCommand(SwitchAccessCommand command) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SWITCH_ACCESS_EVENT_HANDLER_DELEGATE_H_
