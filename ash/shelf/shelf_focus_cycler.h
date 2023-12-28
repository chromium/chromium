// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_FOCUS_CYCLER_H_
#define ASH_SHELF_SHELF_FOCUS_CYCLER_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"

namespace ash {
class Shelf;

// The containers that rely on ShelfFocusCycler to move focus outside of their
// view trees.
enum class SourceView {
  kShelfNavigationView = 0,
  kShelfView,
  kStatusAreaView,
  kDeskButton,
};

// ShelfFocusCycler handles the special focus transitions from the Login UI,
// Shelf, and Status Tray.
class ASH_EXPORT ShelfFocusCycler {
 public:
  explicit ShelfFocusCycler(Shelf* shelf);

  ShelfFocusCycler(const ShelfFocusCycler&) = delete;
  ShelfFocusCycler& operator=(const ShelfFocusCycler&) = delete;

  ~ShelfFocusCycler() = default;

  // Moves focus from one container to the next. |reverse| will move the focus
  // to the container to the left in LTR. RTL does not need to be accounted
  // for when calling this function.
  void FocusOut(bool reverse, SourceView source_view);

  // Focuses the navigation widget (back and home buttons).
  void FocusNavigation(bool last_element);

  // Focuses the desk button widget.
  void FocusDeskButton(bool last_element);

  // Focuses the shelf widget (app shortcuts).
  void FocusShelf(bool last_element);

  // Focuses the status area widget.
  void FocusStatusArea(bool last_element);

 private:
  // Owned by RootWindowController.
  raw_ptr<Shelf> shelf_;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_FOCUS_CYCLER_H_
