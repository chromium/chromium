// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/linked_list.h"

#include "base/check_op.h"

namespace base {

namespace internal {

LinkNodeBase::LinkNodeBase() = default;

LinkNodeBase::LinkNodeBase(LinkNodeBase* previous, LinkNodeBase* next)
    : previous_(previous), next_(next) {}

LinkNodeBase::LinkNodeBase(LinkNodeBase&& rhs) {
  next_ = rhs.next_;
  rhs.next_ = nullptr;
  previous_ = rhs.previous_;
  rhs.previous_ = nullptr;

  // If the node belongs to a list, next_ and previous_ are both non-null.
  // Otherwise, they are both null.
  if (next_) {
    next_->previous_ = this;
    previous_->next_ = this;
  }
}

void LinkNodeBase::RemoveFromList() {
  previous_->next_ = next_;
  next_->previous_ = previous_;
  // next() and previous() return non-null if and only this node is not in any
  // list.
  next_ = nullptr;
  previous_ = nullptr;
}

void LinkNodeBase::InsertBeforeBase(LinkNodeBase* e) {
  CHECK_EQ(previous_, nullptr);
  CHECK_EQ(next_, nullptr);
  next_ = e;
  previous_ = e->previous_;
  e->previous_->next_ = this;
  e->previous_ = this;
}

void LinkNodeBase::InsertAfterBase(LinkNodeBase* e) {
  CHECK_EQ(previous_, nullptr);
  CHECK_EQ(next_, nullptr);
  next_ = e->next_;
  previous_ = e;
  e->next_->previous_ = this;
  e->next_ = this;
}

}  // namespace internal

}  // namespace base
