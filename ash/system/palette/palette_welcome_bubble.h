// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PALETTE_PALETTE_WELCOME_BUBBLE_H_
#define ASH_SYSTEM_PALETTE_PALETTE_WELCOME_BUBBLE_H_

#include "ash/ash_export.h"
#include "ash/session/session_observer.h"
#include "base/macros.h"
#include "ui/events/event_handler.h"
#include "ui/views/widget/widget_observer.h"

class PrefRegistrySimple;
class PrefService;

namespace views {
class View;
}

namespace ash {
class PaletteTray;

// The PaletteWelcomeBubble handles displaying a warm welcome bubble letting
// users know about the PaletteTray the first time a stylus is ejected, or if an
// external stylus is detected. PaletteTray controls the visibility of the
// bubble.
class ASH_EXPORT PaletteWelcomeBubble : public SessionObserver,
                                        public views::WidgetObserver,
                                        public ui::EventHandler {
 public:
  explicit PaletteWelcomeBubble(PaletteTray* tray);
  ~PaletteWelcomeBubble() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Show the welcome bubble iff it has not been shown before.
  void ShowIfNeeded();

  // Get the pref which stores whether the bubble has been shown before.
  bool HasBeenShown() const;

  // Set the pref which stores whether the bubble has been shown before as true.
  // The bubble will not be shown after this is called.
  void MarkAsShown();

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;

  // Returns the bubble view for tests, or null when the bubble is not showing.
  views::View* GetBubbleViewForTesting();

 private:
  friend class PaletteWelcomeBubbleTest;
  class WelcomeBubbleView;

  // Shows or hides the welcome bubble.
  void Show();
  void Hide();

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

  // The PaletteTray this bubble is associated with. Serves as the anchor for
  // the bubble. Not owned.
  PaletteTray* tray_ = nullptr;

  PrefService* active_user_pref_service_ = nullptr;  // Not owned.

  WelcomeBubbleView* bubble_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PaletteWelcomeBubble);
};

}  // namespace ash

#endif  // ASH_SYSTEM_PALETTE_PALETTE_WELCOME_BUBBLE_H_
