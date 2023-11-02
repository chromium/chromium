// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERLAY_EVENT_FILTER_H_
#define ASH_WM_OVERLAY_EVENT_FILTER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/compiler_specific.h"
#include "ui/aura/window.h"
#include "ui/events/event_handler.h"

namespace ash {

// EventFilter for the "overlay window", which intercepts events before they are
// processed by the usual path (e.g. the partial screenshot UI, the keyboard
// overlay).  It does nothing the first time, but works when |Activate()| is
// called.  The main task of this event filter is just to stop propagation
// of any key events during activation, and also signal cancellation when keys
// for canceling are pressed.
class ASH_EXPORT OverlayEventFilter : public ui::EventHandler,
                                      public SessionObserver {
 public:
  // Windows that need to receive events from OverlayEventFilter implement this.
  class ASH_EXPORT Delegate {
   public:
    // Invoked when OverlayEventFilter needs to stop handling events.
    virtual void Cancel() = 0;

    // Returns true if the overlay should be canceled in response to |event|.
    virtual bool IsCancelingKeyEvent(ui::KeyEvent* event) = 0;

    // Returns the window that needs to receive events. NULL if no window needs
    // to receive key events from OverlayEventFilter.
    virtual aura::Window* GetWindow() = 0;
  };

  OverlayEventFilter();

  OverlayEventFilter(const OverlayEventFilter&) = delete;
  OverlayEventFilter& operator=(const OverlayEventFilter&) = delete;

  ~OverlayEventFilter() override;

  // Starts the filtering of events.  It also notifies the specified
  // |delegate| when a key event means cancel (like Esc).  It holds the
  // pointer to the specified |delegate| until Deactivate() is called, but
  // does not take ownership.
  void Activate(Delegate* delegate);

  // Ends the filtering of events.
  void Deactivate(Delegate* delegate);

  // Cancels the partial screenshot UI.  Do nothing if it's not activated.
  void Cancel();

  // Returns true if it's currently active.
  bool IsActive();

  // ui::EventHandler overrides:
  void OnKeyEvent(ui::KeyEvent* event) override;

  // SessionObserver overrides:
  void OnLoginStatusChanged(LoginStatus status) override;
  void OnChromeTerminating() override;
  void OnLockStateChanged(bool locked) override;

 private:
  Delegate* delegate_ = nullptr;
  ScopedSessionObserver scoped_session_observer_;
};

}  // namespace ash

#endif  // ASH_WM_OVERLAY_EVENT_FILTER_H_
