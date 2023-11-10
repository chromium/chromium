// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/prerewritten_event_forwarder.h"

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "ui/base/ui_base_features.h"

namespace ash {

PrerewrittenEventForwarder::PrerewrittenEventForwarder() = default;
PrerewrittenEventForwarder::~PrerewrittenEventForwarder() = default;

ui::EventDispatchDetails PrerewrittenEventForwarder::RewriteEvent(
    const ui::Event& event,
    const Continuation continuation) {
  DCHECK(::features::IsShortcutCustomizationEnabled() ||
         features::IsPeripheralCustomizationEnabled());
  return SendEvent(continuation, &event);
}

}  // namespace ash
