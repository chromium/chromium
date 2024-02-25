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
  if (event.IsKeyEvent()) {
    const ui::KeyEvent& key_event = *event.AsKeyEvent();
    if (!key_event.is_repeat()) {
      for (auto& observer : observers_) {
        observer.OnPrerewriteKeyInputEvent(key_event);
      }
    }
  }

  return SendEvent(continuation, &event);
}

void PrerewrittenEventForwarder::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PrerewrittenEventForwarder::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ash
