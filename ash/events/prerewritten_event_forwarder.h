// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_EVENTS_PREREWRITTEN_EVENT_FORWARDER_H_
#define ASH_EVENTS_PREREWRITTEN_EVENT_FORWARDER_H_

#include "ash/ash_export.h"
#include "base/observer_list.h"
#include "ui/events/event.h"
#include "ui/events/event_rewriter.h"

namespace ash {

// PrerewrittenEventForwarder listens to input events and supplies the raw event
// to clients that want events before any subsequent event rewrites.
class ASH_EXPORT PrerewrittenEventForwarder : public ui::EventRewriter {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called whenever a key event is pressed/released.
    virtual void OnPrerewriteKeyInputEvent(const ui::KeyEvent& event) = 0;
  };

  PrerewrittenEventForwarder();
  PrerewrittenEventForwarder(const PrerewrittenEventForwarder&) = delete;
  PrerewrittenEventForwarder& operator=(const PrerewrittenEventForwarder&) =
      delete;
  ~PrerewrittenEventForwarder() override;

  // ui::EventRewriter:
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // ASH_EVENTS_PREREWRITTEN_EVENT_FORWARDER_H_
