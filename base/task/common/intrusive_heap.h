// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_COMMON_INTRUSIVE_HEAP_H_
#define BASE_TASK_COMMON_INTRUSIVE_HEAP_H_

#include <algorithm>
#include <vector>

#include "base/logging.h"

namespace base {
namespace internal {

template <typename T>
class IntrusiveHeap;

// Intended as an opaque wrapper around |index_|.
class HeapHandle {
 public:
  HeapHandle() : index_(0u) {}

  bool IsValid() const { return index_ != 0u; }

 private:
  template <typename T>
  friend class IntrusiveHeap;

  HeapHandle(size_t index) : index_(index) {}

  size_t index_;
};

// A standard min-heap with the following assumptions:
// 1. T has operator <=
// 2. T has method void SetHeapHandle(HeapHandle handle)
// 3. T has method void ClearHeapHandle()
// 4. T is moveable
// 5. T is default constructible
// 6. The heap size never gets terribly big so reclaiming memory on pop/erase
// isn't a priority.
//
// The reason IntrusiveHeap exists is to provide similar performance to
// std::priority_queue while allowing removal of arbitrary elements.
template <typename T>
class IntrusiveHeap {
 public:
  IntrusiveHeap() : nodes_(kMinimumHeapSize), size_(0) {}

  ~IntrusiveHeap() {
    for (size_t i = 1; i <= size_; i++) {
      MakeHole(i);
    }
  }

  bool empty() const { return size_ == 0; }

  size_t size() const { return size_; }

  void Clear() {
    for (size_t i = 1; i <= size_; i++) {
      MakeHole(i);
    }
    nodes_.resize(kMinimumHeapSize);
    size_ = 0;
  }

  const T& Min() const {
    DCHECK_GE(size_, 1u);
    return nodes_[1];
  }

  void Pop() {
    DCHECK_GE(size_, 1u);
    MakeHole(1u);
    size_t top_index = size_--;
    if (!empty())
      MoveHoleDownAndFillWithLeafElement(1u, std::move(nodes_[top_index]));
  }

  void insert(T&& element) {
    size_++;
    if (size_ >= nodes_.size())
      nodes_.resize(nodes_.size() * 2);
    // Notionally we have a hole in the tree at index |size_|, move this up
    // to find the right insertion point.
    MoveHoleUpAndFillWithElement(size_, std::move(element));
  }

  void erase(HeapHandle handle) {
    DCHECK_GT(handle.index_, 0u);
    DCHECK_LE(handle.index_, size_);
    MakeHole(handle.index_);
    size_t top_index = size_--;
    if (empty() || top_index == handle.index_)
      return;
    if (nodes_[handle.index_] <= nodes_[top_index]) {
      MoveHoleDownAndFillWithLeafElement(handle.index_,
                                         std::move(nodes_[top_index]));
    } else {
      MoveHoleUpAndFillWithElement(handle.index_, std::move(nodes_[top_index]));
    }
  }

  void ReplaceMin(T&& element) {
    // Note |element| might not be a leaf node so we can't use
    // MoveHoleDownAndFillWithLeafElement.
    MoveHoleDownAndFillWithElement(1u, std::move(element));
  }

  void ChangeKey(HeapHandle handle, T&& element) {
    if (nodes_[handle.index_] <= element) {
      MoveHoleDownAndFillWithLeafElement(handle.index_, std::move(element));
    } else {
      MoveHoleUpAndFillWithElement(handle.index_, std::move(element));
    }
  }

  const T& at(HeapHandle handle) const {
    DCHECK(handle.IsValid());
    return nodes_[handle.index_];
  }

  // Caution mutating the heap invalidates the iterators.
  const T* begin() const { return &nodes_[1u]; }
  const T* end() const { return begin() + size_; }

 private:
  enum {
    // The majority of sets in the scheduler have 0-3 items in them (a few will
    // have perhaps up to 100), so this means we usually only have to allocate
    // memory once.
    kMinimumHeapSize = 4u
  };

  friend class IntrusiveHeapTest;

  size_t MoveHole(size_t new_hole_pos, size_t old_hole_pos) {
    DCHECK_GT(new_hole_pos, 0u);
    DCHECK_LE(new_hole_pos, size_);
    DCHECK_GT(new_hole_pos, 0u);
    DCHECK_LE(new_hole_pos, size_);
    DCHECK_NE(old_hole_pos, new_hole_pos);
    nodes_[old_hole_pos] = std::move(nodes_[new_hole_pos]);
    nodes_[old_hole_pos].SetHeapHandle(HeapHandle(old_hole_pos));
    return new_hole_pos;
  }

  // Notionally creates a hole in the tree at |index|.
  void MakeHole(size_t index) {
    DCHECK_GT(index, 0u);
    DCHECK_LE(index, size_);
    nodes_[index].ClearHeapHandle();
  }

  void FillHole(size_t hole, T&& element) {
    DCHECK_GT(hole, 0u);
    DCHECK_LE(hole, size_);
    nodes_[hole] = std::move(element);
    nodes_[hole].SetHeapHandle(HeapHandle(hole));
    DCHECK(std::is_heap(begin(), end(), CompareNodes));
  }

  // is_heap requires a strict comparator.
  static bool CompareNodes(const T& a, const T& b) { return !(a <= b); }

  // Moves the |hole| up the tree and when the right position has been found
  // |element| is moved in.
  void MoveHoleUpAndFillWithElement(size_t hole, T&& element) {
    DCHECK_GT(hole, 0u);
    DCHECK_LE(hole, size_);
    while (hole >= 2u) {
      size_t parent_pos = hole / 2;
      if (nodes_[parent_pos] <= element)
        break;

      hole = MoveHole(parent_pos, hole);
    }
    FillHole(hole, std::move(element));
  }

  // Moves the |hole| down the tree and when the right position has been found
  // |element| is moved in.
  void MoveHoleDownAndFillWithElement(size_t hole, T&& element) {
    DCHECK_GT(hole, 0u);
    DCHECK_LE(hole, size_);
    size_t child_pos = hole * 2;
    while (child_pos < size_) {
      if (nodes_[child_pos + 1] <= nodes_[child_pos])
        child_pos++;

      if (element <= nodes_[child_pos])
        break;

      hole = MoveHole(child_pos, hole);
      child_pos *= 2;
    }
    if (child_pos == size_ && !(element <= nodes_[child_pos]))
      hole = MoveHole(child_pos, hole);
    FillHole(hole, std::move(element));
  }

  // Moves the |hole| down the tree and when the right position has been found
  // |leaf_element| is moved in.  Faster than MoveHoleDownAndFillWithElement
  // (it does one key comparison per level instead of two) but only valid for
  // leaf elements (i.e. one of the max values).
  void MoveHoleDownAndFillWithLeafElement(size_t hole, T&& leaf_element) {
    DCHECK_GT(hole, 0u);
    DCHECK_LE(hole, size_);
    size_t child_pos = hole * 2;
    while (child_pos < size_) {
      size_t second_child = child_pos + 1;
      if (nodes_[second_child] <= nodes_[child_pos])
        child_pos = second_child;

      hole = MoveHole(child_pos, hole);
      child_pos *= 2;
    }
    if (child_pos == size_)
      hole = MoveHole(child_pos, hole);
    MoveHoleUpAndFillWithElement(hole, std::move(leaf_element));
  }

  std::vector<T> nodes_;  // NOTE we use 1-based indexing
  size_t size_;
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_COMMON_INTRUSIVE_HEAP_H_
