// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_transient_descendant_iterator.h"

#include <algorithm>

#include "ash/wm/window_util.h"
#include "ui/aura/window.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

// Helper that returns the next window in the preorder traversal.
aura::Window* GetNextWindow(aura::Window* current_window) {
  const aura::Window::Windows transient_children =
      ::wm::GetTransientChildren(current_window);

  if (!transient_children.empty()) {
    current_window = transient_children.front();
  } else {
    while (current_window) {
      aura::Window* parent = ::wm::GetTransientParent(current_window);
      if (!parent) {
        current_window = nullptr;
        break;
      }
      const aura::Window::Windows transient_siblings =
          ::wm::GetTransientChildren(parent);
      auto iter = std::find(transient_siblings.begin(),
                            transient_siblings.end(), current_window);
      ++iter;
      if (iter != transient_siblings.end()) {
        current_window = *iter;
        break;
      }
      current_window = ::wm::GetTransientParent(current_window);
    }
  }
  return current_window;
}

}  // namespace

WindowTransientDescendantIterator::WindowTransientDescendantIterator()
    : current_window_(nullptr) {}

WindowTransientDescendantIterator::~WindowTransientDescendantIterator() =
    default;

WindowTransientDescendantIterator::WindowTransientDescendantIterator(
    const WindowTransientDescendantIterator& other) = default;

WindowTransientDescendantIterator::WindowTransientDescendantIterator(
    aura::Window* root_window)
    : current_window_(root_window) {}

WindowTransientDescendantIterator::WindowTransientDescendantIterator(
    aura::Window* root_window,
    TransientTreeIgnorePredicate hide_predicate)
    : current_window_(root_window), hide_predicate_(hide_predicate) {}

// Performs a pre-order traversal of the transient descendants.
const WindowTransientDescendantIterator& WindowTransientDescendantIterator::
operator++() {
  DCHECK(current_window_);

  aura::Window* next_window = GetNextWindow(current_window_);
  // Find the next preorder window if |hide_predicate_| is satisfied.
  if (!hide_predicate_.is_null()) {
    while (next_window && hide_predicate_.Run(next_window))
      next_window = GetNextWindow(next_window);
  }
  current_window_ = next_window;

  return *this;
}

bool WindowTransientDescendantIterator::operator!=(
    const WindowTransientDescendantIterator& other) const {
  return current_window_ != other.current_window_;
}

aura::Window* WindowTransientDescendantIterator::operator*() const {
  return current_window_;
}

WindowTransientDescendantIteratorRange::WindowTransientDescendantIteratorRange(
    const WindowTransientDescendantIterator& begin)
    : begin_(begin) {}

WindowTransientDescendantIteratorRange GetTransientTreeIterator(
    aura::Window* window) {
  return WindowTransientDescendantIteratorRange(
      WindowTransientDescendantIterator(::wm::GetTransientRoot(window)));
}

WindowTransientDescendantIteratorRange GetTransientTreeIterator(
    aura::Window* window,
    TransientTreeIgnorePredicate hide_predicate) {
  return WindowTransientDescendantIteratorRange(
      WindowTransientDescendantIterator(::wm::GetTransientRoot(window),
                                        hide_predicate));
}

}  // namespace ash
