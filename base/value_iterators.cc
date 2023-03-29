// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/value_iterators.h"

#include "base/values.h"

namespace base {

namespace detail {

// ----------------------------------------------------------------------------
// dict_iterator.

dict_iterator::pointer::pointer(const reference& ref) : ref_(ref) {}

dict_iterator::pointer::pointer(const pointer& ptr) = default;

dict_iterator::dict_iterator(DictStorage::iterator dict_iter)
    : dict_iter_(dict_iter) {}

dict_iterator::dict_iterator(const dict_iterator& dict_iter) = default;

dict_iterator& dict_iterator::operator=(const dict_iterator& dict_iter) =
    default;

dict_iterator::~dict_iterator() = default;

dict_iterator::reference dict_iterator::operator*() {
  return {dict_iter_->first, *dict_iter_->second};
}

dict_iterator::pointer dict_iterator::operator->() {
  return pointer(operator*());
}

dict_iterator& dict_iterator::operator++() {
  ++dict_iter_;
  return *this;
}

dict_iterator dict_iterator::operator++(int) {
  dict_iterator tmp(*this);
  ++dict_iter_;
  return tmp;
}

dict_iterator& dict_iterator::operator--() {
  --dict_iter_;
  return *this;
}

dict_iterator dict_iterator::operator--(int) {
  dict_iterator tmp(*this);
  --dict_iter_;
  return tmp;
}

bool operator==(const dict_iterator& lhs, const dict_iterator& rhs) {
  return lhs.dict_iter_ == rhs.dict_iter_;
}

bool operator!=(const dict_iterator& lhs, const dict_iterator& rhs) {
  return !(lhs == rhs);
}

// ----------------------------------------------------------------------------
// const_dict_iterator.

const_dict_iterator::pointer::pointer(const reference& ref) : ref_(ref) {}

const_dict_iterator::pointer::pointer(const pointer& ptr) = default;

const_dict_iterator::const_dict_iterator(DictStorage::const_iterator dict_iter)
    : dict_iter_(dict_iter) {}

const_dict_iterator::const_dict_iterator(const const_dict_iterator& dict_iter) =
    default;

const_dict_iterator& const_dict_iterator::operator=(
    const const_dict_iterator& dict_iter) = default;

const_dict_iterator::~const_dict_iterator() = default;

const_dict_iterator::reference const_dict_iterator::operator*() const {
  return {dict_iter_->first, *dict_iter_->second};
}

const_dict_iterator::pointer const_dict_iterator::operator->() const {
  return pointer(operator*());
}

const_dict_iterator& const_dict_iterator::operator++() {
  ++dict_iter_;
  return *this;
}

const_dict_iterator const_dict_iterator::operator++(int) {
  const_dict_iterator tmp(*this);
  ++dict_iter_;
  return tmp;
}

const_dict_iterator& const_dict_iterator::operator--() {
  --dict_iter_;
  return *this;
}

const_dict_iterator const_dict_iterator::operator--(int) {
  const_dict_iterator tmp(*this);
  --dict_iter_;
  return tmp;
}

bool operator==(const const_dict_iterator& lhs,
                const const_dict_iterator& rhs) {
  return lhs.dict_iter_ == rhs.dict_iter_;
}

bool operator!=(const const_dict_iterator& lhs,
                const const_dict_iterator& rhs) {
  return !(lhs == rhs);
}

}  // namespace detail

}  // namespace base
