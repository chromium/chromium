// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_MOVE_ONLY_INT_H_
#define BASE_TEST_MOVE_ONLY_INT_H_

namespace base {

// A move-only class that holds an integer. This is designed for testing
// containers. See also CopyOnlyInt.
class MoveOnlyInt {
 public:
  explicit MoveOnlyInt(int data = 1) : data_(data) {}
  MoveOnlyInt(MoveOnlyInt&& other) : data_(other.data_) { other.data_ = 0; }

  MoveOnlyInt(const MoveOnlyInt&) = delete;
  MoveOnlyInt& operator=(const MoveOnlyInt&) = delete;

  ~MoveOnlyInt() { data_ = 0; }

  MoveOnlyInt& operator=(MoveOnlyInt&& other) {
    data_ = other.data_;
    other.data_ = 0;
    return *this;
  }

  friend bool operator==(const MoveOnlyInt& lhs, const MoveOnlyInt& rhs) {
    return lhs.data_ == rhs.data_;
  }

  friend bool operator!=(const MoveOnlyInt& lhs, const MoveOnlyInt& rhs) {
    return !operator==(lhs, rhs);
  }

  friend bool operator<(const MoveOnlyInt& lhs, int rhs) {
    return lhs.data_ < rhs;
  }

  friend bool operator<(int lhs, const MoveOnlyInt& rhs) {
    return lhs < rhs.data_;
  }

  friend bool operator<(const MoveOnlyInt& lhs, const MoveOnlyInt& rhs) {
    return lhs.data_ < rhs.data_;
  }

  friend bool operator>(const MoveOnlyInt& lhs, const MoveOnlyInt& rhs) {
    return rhs < lhs;
  }

  friend bool operator<=(const MoveOnlyInt& lhs, const MoveOnlyInt& rhs) {
    return !(rhs < lhs);
  }

  friend bool operator>=(const MoveOnlyInt& lhs, const MoveOnlyInt& rhs) {
    return !(lhs < rhs);
  }

  int data() const { return data_; }

 private:
  volatile int data_;
};

}  // namespace base

#endif  // BASE_TEST_MOVE_ONLY_INT_H_
