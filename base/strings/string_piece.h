// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A string-like object that points to a sized piece of memory.
//
// You can use StringPiece as a function or method parameter.  A StringPiece
// parameter can receive a double-quoted string literal argument, a "const
// char*" argument, a string argument, or a StringPiece argument with no data
// copying.  Systematic use of StringPiece for arguments reduces data
// copies and strlen() calls.
//
// Prefer passing StringPieces by value:
//   void MyFunction(StringPiece arg);
// If circumstances require, you may also pass by const reference:
//   void MyFunction(const StringPiece& arg);  // not preferred
// Both of these have the same lifetime semantics.  Passing by value
// generates slightly smaller code.  For more discussion, Googlers can see
// the thread go/stringpiecebyvalue on c-users.

#ifndef BASE_STRINGS_STRING_PIECE_H_
#define BASE_STRINGS_STRING_PIECE_H_

#include <stddef.h>

#include <iosfwd>
#include <limits>
#include <string>
#include <type_traits>

#include "base/base_export.h"
#include "base/check_op.h"
#include "base/strings/char_traits.h"
#include "base/strings/string_piece_forward.h"
#include "build/build_config.h"

namespace base {

// internal --------------------------------------------------------------------

// Many of the StringPiece functions use different implementations for the
// 8-bit and 16-bit versions, and we don't want lots of template expansions in
// this (very common) header that will slow down compilation.
//
// So here we define overloaded functions called by the StringPiece template.
// For those that share an implementation, the two versions will expand to a
// template internal to the .cc file.
namespace internal {

BASE_EXPORT size_t find(StringPiece self, StringPiece s, size_t pos);
BASE_EXPORT size_t find(StringPiece16 self, StringPiece16 s, size_t pos);

BASE_EXPORT size_t rfind(StringPiece self, StringPiece s, size_t pos);
BASE_EXPORT size_t rfind(StringPiece16 self, StringPiece16 s, size_t pos);

BASE_EXPORT size_t find_first_of(StringPiece self, StringPiece s, size_t pos);
BASE_EXPORT size_t find_first_of(StringPiece16 self,
                                 StringPiece16 s,
                                 size_t pos);

BASE_EXPORT size_t find_first_not_of(StringPiece self,
                                     StringPiece s,
                                     size_t pos);
BASE_EXPORT size_t find_first_not_of(StringPiece16 self,
                                     StringPiece16 s,
                                     size_t pos);

BASE_EXPORT size_t find_last_of(StringPiece self, StringPiece s, size_t pos);
BASE_EXPORT size_t find_last_of(StringPiece16 self,
                                StringPiece16 s,
                                size_t pos);

BASE_EXPORT size_t find_last_not_of(StringPiece self,
                                    StringPiece s,
                                    size_t pos);
BASE_EXPORT size_t find_last_not_of(StringPiece16 self,
                                    StringPiece16 s,
                                    size_t pos);

BASE_EXPORT size_t find(WStringPiece self, WStringPiece s, size_t pos);
BASE_EXPORT size_t rfind(WStringPiece self, WStringPiece s, size_t pos);
BASE_EXPORT size_t find_first_of(WStringPiece self, WStringPiece s, size_t pos);
BASE_EXPORT size_t find_first_not_of(WStringPiece self,
                                     WStringPiece s,
                                     size_t pos);
BASE_EXPORT size_t find_last_of(WStringPiece self, WStringPiece s, size_t pos);
BASE_EXPORT size_t find_last_not_of(WStringPiece self,
                                    WStringPiece s,
                                    size_t pos);

}  // namespace internal

// BasicStringPiece ------------------------------------------------------------

// Mirrors the C++17 version of std::basic_string_view<> as closely as possible,
// except where noted below.
template <typename CharT, typename Traits>
class BasicStringPiece {
 public:
  using traits_type = Traits;
  using value_type = CharT;
  using pointer = CharT*;
  using const_pointer = const CharT*;
  using reference = CharT&;
  using const_reference = const CharT&;
  using const_iterator = const CharT*;
  using iterator = const_iterator;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using reverse_iterator = const_reverse_iterator;
  using size_type = size_t;
  using difference_type = ptrdiff_t;

  constexpr BasicStringPiece() noexcept : ptr_(nullptr), length_(0) {}
  constexpr BasicStringPiece(const BasicStringPiece& other) noexcept = default;
  constexpr BasicStringPiece& operator=(const BasicStringPiece& view) noexcept =
      default;
  constexpr BasicStringPiece(const CharT* s, size_type count)
      : ptr_(s), length_(count) {}
  // Note: This doesn't just use traits_type::length(), since that
  // isn't constexpr until C++17.
  constexpr BasicStringPiece(const CharT* s)
      : ptr_(s), length_(s ? CharTraits<CharT>::length(s) : 0) {
    // Intentional STL deviation: Null-check instead of UB.
    CHECK(s);
  }
  // Explicitly disallow construction from nullptr. Note that this does not
  // catch construction from runtime strings that might be null.
  // Note: The following is just a more elaborate way of spelling
  // `BasicStringPiece(nullptr_t) = delete`, but unfortunately the terse form is
  // not supported by the PNaCl toolchain.
  template <class T, class = std::enable_if_t<std::is_null_pointer<T>::value>>
  BasicStringPiece(T) {
    static_assert(sizeof(T) == 0,  // Always false.
                  "StringPiece does not support construction from nullptr, use "
                  "the default constructor instead.");
  }

  // These are necessary because std::basic_string provides construction from
  // (an object convertible to) a std::basic_string_view, as well as an explicit
  // cast operator to a std::basic_string_view, but (obviously) not from/to a
  // BasicStringPiece.
  BasicStringPiece(const std::basic_string<CharT>& str)
      : ptr_(str.data()), length_(str.size()) {}
  explicit operator std::basic_string<CharT>() const {
    return std::basic_string<CharT>(data(), size());
  }

  constexpr const_iterator begin() const noexcept { return ptr_; }
  constexpr const_iterator cbegin() const noexcept { return ptr_; }
  constexpr const_iterator end() const noexcept { return ptr_ + length_; }
  constexpr const_iterator cend() const noexcept { return ptr_ + length_; }
  constexpr const_reverse_iterator rbegin() const noexcept {
    return const_reverse_iterator(ptr_ + length_);
  }
  constexpr const_reverse_iterator crbegin() const noexcept {
    return const_reverse_iterator(ptr_ + length_);
  }
  constexpr const_reverse_iterator rend() const noexcept {
    return const_reverse_iterator(ptr_);
  }
  constexpr const_reverse_iterator crend() const noexcept {
    return const_reverse_iterator(ptr_);
  }

  constexpr const_reference operator[](size_type pos) const {
    // Intentional STL deviation: Bounds-check instead of UB.
    return at(pos);
  }
  constexpr const_reference at(size_type pos) const {
    CHECK_LT(pos, size());
    return data()[pos];
  }

  constexpr const_reference front() const { return operator[](0); }

  constexpr const_reference back() const { return operator[](size() - 1); }

  constexpr const_pointer data() const noexcept { return ptr_; }

  constexpr size_type size() const noexcept { return length_; }
  constexpr size_type length() const noexcept { return length_; }

  constexpr size_type max_size() const {
    return std::numeric_limits<size_type>::max() / sizeof(CharT);
  }

  constexpr bool empty() const noexcept WARN_UNUSED_RESULT {
    return size() == 0;
  }

  constexpr void remove_prefix(size_type n) {
    // Intentional STL deviation: Bounds-check instead of UB.
    CHECK_LE(n, size());
    ptr_ += n;
    length_ -= n;
  }

  constexpr void remove_suffix(size_type n) {
    // Intentional STL deviation: Bounds-check instead of UB.
    CHECK_LE(n, size());
    length_ -= n;
  }

  constexpr void swap(BasicStringPiece& v) noexcept {
    // Note: Cannot use std::swap() since it is not constexpr until C++20.
    const const_pointer ptr = ptr_;
    ptr_ = v.ptr_;
    v.ptr_ = ptr;
    const size_type length = length_;
    length_ = v.length_;
    v.length_ = length;
  }

  constexpr size_type copy(CharT* dest,
                           size_type count,
                           size_type pos = 0) const {
    CHECK_LE(pos, size());
    const size_type rcount = std::min(count, size() - pos);
    traits_type::copy(dest, data() + pos, rcount);
    return rcount;
  }

  constexpr BasicStringPiece substr(size_type pos = 0,
                                    size_type count = npos) const {
    CHECK_LE(pos, size());
    const size_type rcount = std::min(count, size() - pos);
    return {data() + pos, rcount};
  }

  constexpr int compare(BasicStringPiece v) const noexcept {
    const size_type rlen = std::min(size(), v.size());
    const int result = CharTraits<CharT>::compare(data(), v.data(), rlen);
    if (result != 0)
      return result;
    if (size() == v.size())
      return 0;
    return size() < v.size() ? -1 : 1;
  }
  constexpr int compare(size_type pos1,
                        size_type count1,
                        BasicStringPiece v) const {
    return substr(pos1, count1).compare(v);
  }
  constexpr int compare(size_type pos1,
                        size_type count1,
                        BasicStringPiece v,
                        size_type pos2,
                        size_type count2) const {
    return substr(pos1, count1).compare(v.substr(pos2, count2));
  }
  constexpr int compare(const CharT* s) const {
    return compare(BasicStringPiece(s));
  }
  constexpr int compare(size_type pos1,
                        size_type count1,
                        const CharT* s) const {
    return substr(pos1, count1).compare(BasicStringPiece(s));
  }
  constexpr int compare(size_type pos1,
                        size_type count1,
                        const CharT* s,
                        size_type count2) const {
    return substr(pos1, count1).compare(BasicStringPiece(s, count2));
  }

  constexpr size_type find(BasicStringPiece v,
                           size_type pos = 0) const noexcept {
    if (is_constant_evaluated()) {
      if (v.size() > size())
        return npos;
      for (size_type p = pos; p <= size() - v.size(); ++p) {
        if (!compare(p, v.size(), v))
          return p;
      }
      return npos;
    }

    return internal::find(*this, v, pos);
  }
  constexpr size_type find(CharT ch, size_type pos = 0) const noexcept {
    if (pos >= size())
      return npos;

    const const_pointer result =
        base::CharTraits<CharT>::find(data() + pos, size() - pos, ch);
    return result ? static_cast<size_type>(result - data()) : npos;
  }
  constexpr size_type find(const CharT* s,
                           size_type pos,
                           size_type count) const {
    return find(BasicStringPiece(s, count), pos);
  }
  constexpr size_type find(const CharT* s, size_type pos = 0) const {
    return find(BasicStringPiece(s), pos);
  }

  constexpr size_type rfind(BasicStringPiece v,
                            size_type pos = npos) const noexcept {
    if (is_constant_evaluated()) {
      if (v.size() > size())
        return npos;
      for (size_type p = std::min(size() - v.size(), pos);; --p) {
        if (!compare(p, v.size(), v))
          return p;
        if (!p)
          break;
      }
      return npos;
    }

    return internal::rfind(*this, v, pos);
  }
  constexpr size_type rfind(CharT c, size_type pos = npos) const noexcept {
    if (empty())
      return npos;

    for (size_t i = std::min(pos, size() - 1);; --i) {
      if (data()[i] == c)
        return i;

      if (i == 0)
        break;
    }
    return npos;
  }
  constexpr size_type rfind(const CharT* s,
                            size_type pos,
                            size_type count) const {
    return rfind(BasicStringPiece(s, count), pos);
  }
  constexpr size_type rfind(const CharT* s, size_type pos = npos) const {
    return rfind(BasicStringPiece(s), pos);
  }

  constexpr size_type find_first_of(BasicStringPiece v,
                                    size_type pos = 0) const noexcept {
    if (is_constant_evaluated()) {
      if (empty() || v.empty())
        return npos;
      for (size_type p = pos; p < size(); ++p) {
        if (v.find(data()[p]) != npos)
          return p;
      }
      return npos;
    }

    return internal::find_first_of(*this, v, pos);
  }
  constexpr size_type find_first_of(CharT c, size_type pos = 0) const noexcept {
    return find(c, pos);
  }
  constexpr size_type find_first_of(const CharT* s,
                                    size_type pos,
                                    size_type count) const {
    return find_first_of(BasicStringPiece(s, count), pos);
  }
  constexpr size_type find_first_of(const CharT* s, size_type pos = 0) const {
    return find_first_of(BasicStringPiece(s), pos);
  }

  constexpr size_type find_last_of(BasicStringPiece v,
                                   size_type pos = npos) const noexcept {
    if (is_constant_evaluated()) {
      if (empty() || v.empty())
        return npos;
      for (size_type p = std::min(pos, size() - 1);; --p) {
        if (v.find(data()[p]) != npos)
          return p;
        if (!p)
          break;
      }
      return npos;
    }

    return internal::find_last_of(*this, v, pos);
  }
  constexpr size_type find_last_of(CharT c,
                                   size_type pos = npos) const noexcept {
    return rfind(c, pos);
  }
  constexpr size_type find_last_of(const CharT* s,
                                   size_type pos,
                                   size_type count) const {
    return find_last_of(BasicStringPiece(s, count), pos);
  }
  constexpr size_type find_last_of(const CharT* s, size_type pos = npos) const {
    return find_last_of(BasicStringPiece(s), pos);
  }

  constexpr size_type find_first_not_of(BasicStringPiece v,
                                        size_type pos = 0) const noexcept {
    if (is_constant_evaluated()) {
      if (empty())
        return npos;
      for (size_type p = pos; p < size(); ++p) {
        if (v.find(data()[p]) == npos)
          return p;
      }
      return npos;
    }

    return internal::find_first_not_of(*this, v, pos);
  }
  constexpr size_type find_first_not_of(CharT c,
                                        size_type pos = 0) const noexcept {
    if (empty())
      return npos;

    for (; pos < size(); ++pos) {
      if (data()[pos] != c)
        return pos;
    }
    return npos;
  }
  constexpr size_type find_first_not_of(const CharT* s,
                                        size_type pos,
                                        size_type count) const {
    return find_first_not_of(BasicStringPiece(s, count), pos);
  }
  constexpr size_type find_first_not_of(const CharT* s,
                                        size_type pos = 0) const {
    return find_first_not_of(BasicStringPiece(s), pos);
  }

  constexpr size_type find_last_not_of(BasicStringPiece v,
                                       size_type pos = npos) const noexcept {
    if (is_constant_evaluated()) {
      if (empty())
        return npos;
      for (size_type p = std::min(pos, size() - 1);; --p) {
        if (v.find(data()[p]) == npos)
          return p;
        if (!p)
          break;
      }
      return npos;
    }

    return internal::find_last_not_of(*this, v, pos);
  }
  constexpr size_type find_last_not_of(CharT c,
                                       size_type pos = npos) const noexcept {
    if (empty())
      return npos;

    for (size_t i = std::min(pos, size() - 1);; --i) {
      if (data()[i] != c)
        return i;
      if (i == 0)
        break;
    }
    return npos;
  }
  constexpr size_type find_last_not_of(const CharT* s,
                                       size_type pos,
                                       size_type count) const {
    return find_last_not_of(BasicStringPiece(s, count), pos);
  }
  constexpr size_type find_last_not_of(const CharT* s,
                                       size_type pos = npos) const {
    return find_last_not_of(BasicStringPiece(s), pos);
  }

  static constexpr size_type npos = size_type(-1);

 protected:
  const_pointer ptr_;
  size_type length_;
};

// static
template <typename CharT, typename Traits>
const typename BasicStringPiece<CharT, Traits>::size_type
    BasicStringPiece<CharT, Traits>::npos;

// MSVC doesn't like complex extern templates and DLLs.
#if !defined(COMPILER_MSVC)
extern template class BASE_EXPORT BasicStringPiece<char>;
extern template class BASE_EXPORT BasicStringPiece<char16_t>;
#endif

template <typename CharT, typename Traits>
constexpr bool operator==(BasicStringPiece<CharT, Traits> lhs,
                          BasicStringPiece<CharT, Traits> rhs) noexcept {
  return lhs.size() == rhs.size() && lhs.compare(rhs) == 0;
}
// Here and below we make use of std::common_type_t to emulate
// std::type_identity (part of C++20). This creates a non-deduced context, so
// that we can compare StringPieces with types that implicitly convert to
// StringPieces. See https://wg21.link/n3766 for details.
// Furthermore, we require dummy template parameters for these overloads to work
// around a name mangling issue on Windows.
template <typename CharT, typename Traits, int = 1>
constexpr bool operator==(
    BasicStringPiece<CharT, Traits> lhs,
    std::common_type_t<BasicStringPiece<CharT, Traits>> rhs) noexcept {
  return lhs.size() == rhs.size() && lhs.compare(rhs) == 0;
}
template <typename CharT, typename Traits, int = 2>
constexpr bool operator==(
    std::common_type_t<BasicStringPiece<CharT, Traits>> lhs,
    BasicStringPiece<CharT, Traits> rhs) noexcept {
  return lhs.size() == rhs.size() && lhs.compare(rhs) == 0;
}

template <typename CharT, typename Traits>
constexpr bool operator!=(BasicStringPiece<CharT, Traits> lhs,
                          BasicStringPiece<CharT, Traits> rhs) noexcept {
  return !(lhs == rhs);
}
template <typename CharT, typename Traits, int = 1>
constexpr bool operator!=(
    BasicStringPiece<CharT, Traits> lhs,
    std::common_type_t<BasicStringPiece<CharT, Traits>> rhs) noexcept {
  return !(lhs == rhs);
}
template <typename CharT, typename Traits, int = 2>
constexpr bool operator!=(
    std::common_type_t<BasicStringPiece<CharT, Traits>> lhs,
    BasicStringPiece<CharT, Traits> rhs) noexcept {
  return !(lhs == rhs);
}

template <typename CharT, typename Traits>
constexpr bool operator<(BasicStringPiece<CharT, Traits> lhs,
                         BasicStringPiece<CharT, Traits> rhs) noexcept {
  return lhs.compare(rhs) < 0;
}
template <typename CharT, typename Traits, int = 1>
constexpr bool operator<(
    BasicStringPiece<CharT, Traits> lhs,
    std::common_type_t<BasicStringPiece<CharT, Traits>> rhs) noexcept {
  return lhs.compare(rhs) < 0;
}

template <typename CharT, typename Traits, int = 2>
constexpr bool operator<(
    std::common_type_t<BasicStringPiece<CharT, Traits>> lhs,
    BasicStringPiece<CharT, Traits> rhs) noexcept {
  return lhs.compare(rhs) < 0;
}

template <typename CharT, typename Traits>
constexpr bool operator>(BasicStringPiece<CharT, Traits> lhs,
                         BasicStringPiece<CharT, Traits> rhs) noexcept {
  return rhs < lhs;
}
template <typename CharT, typename Traits, int = 1>
constexpr bool operator>(
    BasicStringPiece<CharT, Traits> lhs,
    std::common_type_t<BasicStringPiece<CharT, Traits>> rhs) noexcept {
  return rhs < lhs;
}
template <typename CharT, typename Traits, int = 2>
constexpr bool operator>(
    std::common_type_t<BasicStringPiece<CharT, Traits>> lhs,
    BasicStringPiece<CharT, Traits> rhs) noexcept {
  return rhs < lhs;
}

template <typename CharT, typename Traits>
constexpr bool operator<=(BasicStringPiece<CharT, Traits> lhs,
                          BasicStringPiece<CharT, Traits> rhs) noexcept {
  return !(rhs < lhs);
}
template <typename CharT, typename Traits, int = 1>
constexpr bool operator<=(
    BasicStringPiece<CharT, Traits> lhs,
    std::common_type_t<BasicStringPiece<CharT, Traits>> rhs) noexcept {
  return !(rhs < lhs);
}
template <typename CharT, typename Traits, int = 2>
constexpr bool operator<=(
    std::common_type_t<BasicStringPiece<CharT, Traits>> lhs,
    BasicStringPiece<CharT, Traits> rhs) noexcept {
  return !(rhs < lhs);
}

template <typename CharT, typename Traits>
constexpr bool operator>=(BasicStringPiece<CharT, Traits> lhs,
                          BasicStringPiece<CharT, Traits> rhs) noexcept {
  return !(lhs < rhs);
}
template <typename CharT, typename Traits, int = 1>
constexpr bool operator>=(
    BasicStringPiece<CharT, Traits> lhs,
    std::common_type_t<BasicStringPiece<CharT, Traits>> rhs) noexcept {
  return !(lhs < rhs);
}
template <typename CharT, typename Traits, int = 2>
constexpr bool operator>=(
    std::common_type_t<BasicStringPiece<CharT, Traits>> lhs,
    BasicStringPiece<CharT, Traits> rhs) noexcept {
  return !(lhs < rhs);
}

BASE_EXPORT std::ostream& operator<<(std::ostream& o, StringPiece piece);
// Not in the STL: convenience functions to output non-UTF-8 strings to an
// 8-bit-width stream.
BASE_EXPORT std::ostream& operator<<(std::ostream& o, StringPiece16 piece);
BASE_EXPORT std::ostream& operator<<(std::ostream& o, WStringPiece piece);

// Intentionally omitted (since Chromium does not use character literals):
// operator""sv.

// Stand-ins for the STL's std::hash<> specializations.
template <typename StringPieceType>
struct StringPieceHashImpl {
  // This is a custom hash function. We don't use the ones already defined for
  // string and std::u16string directly because it would require the string
  // constructors to be called, which we don't want.
  std::size_t operator()(StringPieceType sp) const {
    std::size_t result = 0;
    for (auto c : sp)
      result = (result * 131) + c;
    return result;
  }
};
using StringPieceHash = StringPieceHashImpl<StringPiece>;
using StringPiece16Hash = StringPieceHashImpl<StringPiece16>;
using WStringPieceHash = StringPieceHashImpl<WStringPiece>;

}  // namespace base

#endif  // BASE_STRINGS_STRING_PIECE_H_
