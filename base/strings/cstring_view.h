// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_CSTRING_VIEW_H_
#define BASE_STRINGS_CSTRING_VIEW_H_

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/checked_iterators.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/is_basic_cstring_view.h"
#include "base/types/to_address.h"
#include "build/build_config.h"

namespace base {

// A CString is a NUL-terminated character array, which is the C programming
// language representation of a string. This class (and its aliases below)
// provides a non-owning and bounds-safe view of a CString, and can replace all
// use of native pointers (such as `const char*`) for this purpose in C++ code.
//
// The basic_cstring_view class is followed by aliases for the various char
// types:
// * cstring_view provides a view of a `const char*`.
// * u16cstring_view provides a view of a `const char16_t*`.
// * u32cstring_view provides a view of a `const char32_t*`.
// * wcstring_view provides a view of a `const wchar_t*`.
template <class Char>
class basic_cstring_view {
  static_assert(!std::is_const_v<Char>);
  static_assert(!std::is_reference_v<Char>);

 public:
  using value_type = Char;
  using pointer = Char*;
  using const_pointer = const Char*;
  using reference = Char&;
  using const_reference = const Char&;
  using iterator = CheckedContiguousIterator<const Char>;
  using const_iterator = CheckedContiguousIterator<const Char>;
  // TODO:
  // using reverse_iterator = std::reverse_iterator<iterator>;
  // using const_reverse_iterator = std::reverse_iterator<iterator>;
  using size_type = size_t;
  using difference_type = ptrdiff_t;

  // Constructs an empty cstring view, which points to an empty string with a
  // terminating NUL.
  constexpr basic_cstring_view() noexcept : ptr_(kEmpty), len_(0u) {}

  // cstring views are trivially copyable, moveable, and destructible.

  // Constructs a cstring view that points at the contents of a string literal.
  //
  // Example:
  // ```
  // const char kLiteral[] = "hello world";
  // auto s = base::cstring_view(kLiteral);
  // CHECK(s == "hello world");
  // auto s2 = base::cstring_view("this works too");
  // CHECK(s == "this works too");
  // ```
  template <int&..., size_t M>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr basic_cstring_view(const Char (&lit LIFETIME_BOUND)[M]) noexcept
      ENABLE_IF_ATTR(lit[M - 1u] == Char{0}, "requires string literal as input")
      : ptr_(lit), len_(M - 1u) {
    // For non-clang compilers. On clang, the function is not even callable
    // without this being known to pass at compile time.
    //
    // SAFETY: lit is an array of size M, so M-1 is in bounds.
    DCHECK_EQ(UNSAFE_BUFFERS(lit[M - 1u]), Char{0});
  }

  // Unsafe construction from a pointer and length. Prefer to construct cstring
  // view from a string literal, std::string, or another cstring view.
  //
  // # Safety
  // The `ptr` and `len` pair indicate a valid NUL-terminated string:
  // * The `ptr` must not be null, and must point to a NUL-terminated string.
  // * The `len` must be valid such that `ptr + len` gives a pointer to the
  //   terminating NUL and is in the same allocation as `ptr`.
  UNSAFE_BUFFER_USAGE explicit constexpr basic_cstring_view(const Char* ptr
                                                                LIFETIME_BOUND,
                                                            size_t len)
      : ptr_(ptr), len_(len) {
    // This method is marked UNSAFE_BUFFER_USAGE so we are trusting the caller
    // to do things right, and expecting strong scrutiny at the call site, but
    // we perform a debug check to help catch mistakes regardless.
    //
    // SAFETY: `ptr` points to `len` many chars and then a NUL, according to the
    // caller of this method. So then `len` index will be in bounds and return
    // the NUL.
    DCHECK_EQ(UNSAFE_BUFFERS(ptr[len]), Char{0});
  }

  // Returns a pointer to the NUL-terminated string, for passing to C-style APIs
  // that require `const char*` (or whatever the `Char` type is).
  //
  // This is never null.
  PURE_FUNCTION constexpr const Char* c_str() const noexcept { return ptr_; }

  // Returns a pointer to underlying buffer. To get a string pointer, use
  // `c_str()`.
  //
  // Pair with `size()` to construct a bounded non-NUL-terminated view, such as
  // by `base::span`. This is never null.
  PURE_FUNCTION constexpr const Char* data() const noexcept { return ptr_; }

  // Returns the number of characters in the string, not including the
  // terminating NUL.
  PURE_FUNCTION constexpr size_t size() const noexcept { return len_; }

  // Returns the number of bytes in the string, not including the terminating
  // NUL. To include the NUL, add `sizeof(Char)` where `Char` is the character
  // type of the cstring view (accessible as the `value_type` alias).
  PURE_FUNCTION constexpr size_t size_bytes() const noexcept {
    return len_ * sizeof(Char);
  }

  // Produces an iterator over the cstring view, excluding the terminating NUL.
  PURE_FUNCTION constexpr const iterator begin() const noexcept {
    // SAFETY: `ptr_ + len_` for a cstring view always gives a pointer in
    // the same allocation as `ptr_` based on the precondition of
    // the type.
    return UNSAFE_BUFFERS(iterator(ptr_, ptr_ + len_));
  }
  // Produces an iterator over the cstring view, excluding the terminating NUL.
  PURE_FUNCTION constexpr const iterator end() const noexcept {
    // SAFETY: `ptr_ + len_` for a cstring view always gives a pointer in
    // the same allocation as `ptr_` based on the precondition of
    // the type.
    return UNSAFE_BUFFERS(iterator(ptr_, ptr_ + len_, ptr_ + len_));
  }
  // Produces an iterator over the cstring view, excluding the terminating NUL.
  PURE_FUNCTION constexpr const const_iterator cbegin() const noexcept {
    return begin();
  }
  // Produces an iterator over the cstring view, excluding the terminating NUL.
  PURE_FUNCTION constexpr const const_iterator cend() const noexcept {
    return end();
  }

  // Returns the character at offset `idx`.
  //
  // This can be used to access any character in the ctring, as well as the NUL
  // terminator.
  //
  // # Checks
  // The function CHECKs that the `idx` is inside the cstring (including at its
  // NUL terminator) and will terminate otherwise.
  PURE_FUNCTION constexpr const Char& operator[](size_t idx) const noexcept {
    CHECK_LE(idx, len_);
    // SAFETY: `ptr_` points `len_` many elements plus a NUL terminator, and
    // `idx <= len_`, so `idx` is in range for `ptr_`.
    return UNSAFE_BUFFERS(ptr_[idx]);
  }

  // Compare two cstring views for equality, comparing the string contents.
  friend constexpr bool operator==(basic_cstring_view l, basic_cstring_view r) {
    return std::ranges::equal(l, r);
  }

  // Return an ordering between two cstring views, comparing the string
  // contents.
  //
  // cstring views are weakly ordered, since string views pointing into
  // different strings can compare as equal.
  friend constexpr std::weak_ordering operator<=>(basic_cstring_view l,
                                                  basic_cstring_view r) {
    return std::lexicographical_compare_three_way(l.begin(), l.end(), r.begin(),
                                                  r.end());
  }

 private:
  // An empty string literal for the `Char` type.
  static constexpr Char kEmpty[] = {Char{0}};

  // An always-valid pointer (never null) to a NUL-terminated string.
  RAW_PTR_EXCLUSION const Char* ptr_;
  // The number of characters between `ptr_` and the NUL terminator.
  //
  // SAFETY: `ptr_ + len_` is always valid since `len_` must not exceed the
  // number of characters in the allocation, or it would no longer indicate the
  // position of the NUL terminator in the string allocation.
  size_t len_;
};

namespace internal {
// Internal type matcher for cstring views based on basic_cstring_view.
template <class T>
struct IsBasicCStringView;
}  // namespace internal

// cstring_view provides a view of a NUL-terminated string. It is a replacement
// for all use of `const char*`, in order to provide bounds checks and prevent
// unsafe pointer usage (otherwise prevented by `-Wunsafe-buffer-usage`).
//
// See basic_cstring_view for more.
using cstring_view = basic_cstring_view<char>;

// u16cstring_view provides a view of a NUL-terminated string. It is a
// replacement for all use of `const char16_t*`, in order to provide bounds
// checks and prevent unsafe pointer usage (otherwise prevented by
// `-Wunsafe-buffer-usage`).
//
// See basic_cstring_view for more.
using u16cstring_view = basic_cstring_view<char16_t>;

// u32cstring_view provides a view of a NUL-terminated string. It is a
// replacement for all use of `const char32_t*`, in order to provide bounds
// checks and prevent unsafe pointer usage (otherwise prevented by
// `-Wunsafe-buffer-usage`).
//
// See basic_cstring_view for more.
using u32cstring_view = basic_cstring_view<char32_t>;

#if BUILDFLAG(IS_WIN)
// wcstring_view provides a view of a NUL-terminated string. It is a
// replacement for all use of `const wchar_t*`, in order to provide bounds
// checks and prevent unsafe pointer usage (otherwise prevented by
// `-Wunsafe-buffer-usage`).
//
// See basic_cstring_view for more.
using wcstring_view = basic_cstring_view<wchar_t>;
#endif

}  // namespace base

template <class T>
  requires(base::internal::IsBasicCStringView<T>::value)
inline constexpr bool std::ranges::enable_borrowed_range<T> = true;

template <class T>
  requires(base::internal::IsBasicCStringView<T>::value)
inline constexpr bool std::ranges::enable_view<T> = true;

#endif  // BASE_STRINGS_CSTRING_VIEW_H_
