// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/peripheral_customization_event_rewriter.h"

#include "ash/constants/ash_features.h"
#include "base/check.h"

namespace ash {

PeripheralCustomizationEventRewriter::PeripheralCustomizationEventRewriter() =
    default;
PeripheralCustomizationEventRewriter::~PeripheralCustomizationEventRewriter() =
    default;

ui::EventDispatchDetails PeripheralCustomizationEventRewriter::RewriteEvent(
    const ui::Event& event,
    const Continuation continuation) {
  DCHECK(features::IsPeripheralCustomizationEnabled());
  return SendEvent(continuation, &event);
}

}  // namespace ash
