// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_TRANSIENT_DESCENDANT_ITERATOR_H_
#define ASH_WM_WINDOW_TRANSIENT_DESCENDANT_ITERATOR_H_

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"

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

  // Iterates over |root_window| and all of its transient descendants.
  explicit WindowTransientDescendantIterator(aura::Window* root_window);

  WindowTransientDescendantIterator(
      aura::Window* root_window,
      TransientTreeIgnorePredicate hide_predicate);

  // Copy constructor required for iterator purposes.
  WindowTransientDescendantIterator(
      const WindowTransientDescendantIterator& other);

  ~WindowTransientDescendantIterator();

  // Prefix increment operator.  This assumes there are more items (i.e.
  // *this != TransientDescendantIterator()).
  const WindowTransientDescendantIterator& operator++();

  // Comparison for STL-based loops.
  bool operator!=(const WindowTransientDescendantIterator& other) const;

  // Dereference operator for STL-compatible iterators.
  aura::Window* operator*() const;

 private:
  // Explicit assignment operator defined because an explicit copy constructor
  // is needed.
  WindowTransientDescendantIterator& operator=(
      const WindowTransientDescendantIterator& other);

  // The current window that |this| refers to. A null |current_window_| denotes
  // an empty iterator and is used as the last possible value in the traversal.
  raw_ptr<aura::Window> current_window_;

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

  WindowTransientDescendantIteratorRange(
      const WindowTransientDescendantIteratorRange&) = delete;
  WindowTransientDescendantIteratorRange& operator=(
      const WindowTransientDescendantIteratorRange&) = delete;

  const WindowTransientDescendantIterator& begin() const { return begin_; }
  const WindowTransientDescendantIterator& end() const { return end_; }

 private:
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
