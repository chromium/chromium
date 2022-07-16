// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_AUTO_RESET_H_
#define BASE_AUTO_RESET_H_

#include <utility>

// base::AutoReset<> is useful for setting a variable to a new value only within
// a particular scope. An base::AutoReset<> object resets a variable to its
// original value upon destruction, making it an alternative to writing
// "var = false;" or "var = old_val;" at all of a block's exit points.
// base::AutoReset<> 对于仅在特定范围内将变量设置为新值很有用。 base::AutoReset<> 对象
// 在销毁时将变量重置为其原始值，使其成为编写“var = false;”的替代方法 或“var = old_val;”
// 在一个block的所有出口点。
//
// This should be obvious, but note that an base::AutoReset<> instance should
// have a shorter lifetime than its scoped_variable, to prevent invalid memory
// writes when the base::AutoReset<> object is destroyed.
// 这应该很明显，但请注意 base::AutoReset<> 实例的生命周期应该比它的 scoped_variable
// 短，以防止在销毁 base::AutoReset<> 对象时进行无效的内存写入。

namespace base {

/**
 * @brief 这个类真是把 RAII 机制运用到极致了
 */
template <typename T>
class AutoReset {
 public:
  template <typename U>
  AutoReset(T* scoped_variable, U&& new_value)
      : scoped_variable_(scoped_variable),
        original_value_(
            std::exchange(*scoped_variable_, std::forward<U>(new_value))) {}

  AutoReset(AutoReset&& other)
      : scoped_variable_(std::exchange(other.scoped_variable_, nullptr)),
        original_value_(std::move(other.original_value_)) {}

  AutoReset& operator=(AutoReset&& rhs) {
    scoped_variable_ = std::exchange(rhs.scoped_variable_, nullptr);
    original_value_ = std::move(rhs.original_value_);
    return *this;
  }

  ~AutoReset() {
    if (scoped_variable_)
      *scoped_variable_ = std::move(original_value_);
  }

 private:
  T* scoped_variable_;
  T original_value_;
};

}  // namespace base

#endif  // BASE_AUTO_RESET_H_
