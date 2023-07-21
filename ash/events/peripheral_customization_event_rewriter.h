// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_EVENTS_PERIPHERAL_CUSTOMIZATION_EVENT_REWRITER_H_
#define ASH_EVENTS_PERIPHERAL_CUSTOMIZATION_EVENT_REWRITER_H_

#include "ash/ash_export.h"
#include "ui/events/event.h"
#include "ui/events/event_rewriter.h"

namespace ash {

// PeripheralCustomizationEventRewriter recognizes and rewrites events from mice
// and graphics tablets to arbitrary `ui::KeyEvent`s configured by the user via
// the Settings SWA.
class ASH_EXPORT PeripheralCustomizationEventRewriter
    : public ui::EventRewriter {
 public:
  PeripheralCustomizationEventRewriter();
  PeripheralCustomizationEventRewriter(
      const PeripheralCustomizationEventRewriter&) = delete;
  PeripheralCustomizationEventRewriter& operator=(
      const PeripheralCustomizationEventRewriter&) = delete;
  ~PeripheralCustomizationEventRewriter() override;

  // ui::EventRewriter:
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;
};

}  // namespace ash

#endif  // ASH_EVENTS_PERIPHERAL_CUSTOMIZATION_EVENT_REWRITER_H_
