// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_TRANSIENT_DESCENDANT_ITERATOR_H_
#define ASH_WM_WINDOW_TRANSIENT_DESCENDANT_ITERATOR_H_

#include "ash/ash_export.h"
#include "base/bind_helpers.h"
#include "base/callback.h"

namespace aura {
class Window;
}

namespace ash {

using TransientTreeIgnorePredicate =
    base::RepeatingCallback<bool(aura::Window*)>;

// An iterator class that traverses an aura::Window and all of its transient
// descendants.
class ASH_EXPORT WindowTransientDescendantIterator {
 public:
  // Creates an empty iterator.
  WindowTransientDescendantIterator();

  ~WindowTransientDescendantIterator();

  // Copy constructor required for iterator purposes.
  WindowTransientDescendantIterator(
      const WindowTransientDescendantIterator& other);

  // Iterates over |root_window| and all of its transient descendants.
  explicit WindowTransientDescendantIterator(aura::Window* root_window);

  WindowTransientDescendantIterator(
      aura::Window* root_window,
      TransientTreeIgnorePredicate hide_predicate);

  // Prefix increment operator.  This assumes there are more items (i.e.
  // *this != TransientDescendantIterator()).
  const WindowTransientDescendantIterator& operator++();

  // Comparison for STL-based loops.
  bool operator!=(const WindowTransientDescendantIterator& other) const;

  // Dereference operator for STL-compatible iterators.
  aura::Window* operator*() const;

 private:
  // Explicit assignment operator defined because an explicit copy constructor
  // is needed and therefore the DISALLOW_COPY_AND_ASSIGN macro cannot be used.
  WindowTransientDescendantIterator& operator=(
      const WindowTransientDescendantIterator& other) = default;

  // The current window that |this| refers to. A null |current_window_| denotes
  // an empty iterator and is used as the last possible value in the traversal.
  aura::Window* current_window_;

  // Windows that satisfy this predicate will not be shown.
  TransientTreeIgnorePredicate hide_predicate_ = base::NullCallback();
};

// Provides a virtual container implementing begin() and end() for a sequence of
// WindowTransientDescendantIterators. This can be used in range-based for
// loops.
class WindowTransientDescendantIteratorRange {
 public:
  explicit WindowTransientDescendantIteratorRange(
      const WindowTransientDescendantIterator& begin);

  // Copy constructor required for iterator purposes.
  WindowTransientDescendantIteratorRange(
      const WindowTransientDescendantIteratorRange& other) = default;

  const WindowTransientDescendantIterator& begin() const { return begin_; }
  const WindowTransientDescendantIterator& end() const { return end_; }

 private:
  // Because the explicit copy constructor is needed, explicitly delete the
  // assignment operator rather than using DISALLOW_COPY_AND_ASSIGN.
  WindowTransientDescendantIteratorRange& operator=(
      const WindowTransientDescendantIteratorRange& other) = delete;

  WindowTransientDescendantIterator begin_;
  WindowTransientDescendantIterator end_;
};

// Returns the range to iterate over the entire transient-window hierarchy which
// |window| belongs to. If |hide_predicate| is given, windows that satisfy that
// condition will be skipped.
ASH_EXPORT WindowTransientDescendantIteratorRange
GetTransientTreeIterator(aura::Window* window);
ASH_EXPORT WindowTransientDescendantIteratorRange
GetTransientTreeIterator(aura::Window* window,
                         TransientTreeIgnorePredicate hide_predicate);

}  // namespace ash

#endif  // ASH_WM_WINDOW_TRANSIENT_DESCENDANT_ITERATOR_H_
