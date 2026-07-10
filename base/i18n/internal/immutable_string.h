// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_INTERNAL_IMMUTABLE_STRING_H_
#define BASE_I18N_INTERNAL_IMMUTABLE_STRING_H_

#include <sys/types.h>

#include <algorithm>
#include <array>
#include <numeric>
#include <string_view>
#include <variant>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/i18n/base_i18n_export.h"
#include "base/numerics/safe_conversions.h"

namespace base::i18n_internal {

constexpr size_t TotalSize(base::span<const std::string_view> parts) {
  return std::ranges::fold_left(
      parts, size_t{0},
      [](size_t total, std::string_view part) { return total + part.size(); });
}

constexpr void CopyParts(base::span<const std::string_view> parts,
                         base::span<char> dest) {
  for (const std::string_view& part : parts) {
    base::span(dest).first(part.size()).copy_from_nonoverlapping(part);
    dest = dest.subspan(part.size());
  }
}

// An immutable string storage that optimizes for memory usage by using a small
// stack-allocated buffer (SSO) and falling back to a heap-allocated buffer for
// larger strings.
class BASE_I18N_EXPORT ImmutableString {
 public:
  // The size limit where we expect to keep things all in the stack.
  static constexpr size_t kSmallBufferSize = 14;

  // Class that stores a small (determined by `kSmallBufferSize`), fixed-size
  // and immutable string. The class is copyable and movable for convenient
  // implementation of `ImmutableString`.
  class BASE_I18N_EXPORT StackString {
   public:
    constexpr StackString() : storage_{} {}
    explicit constexpr StackString(base::span<const std::string_view> parts)
        : storage_{}, size_(base::checked_cast<uint8_t>(TotalSize(parts))) {
      CopyParts(parts, base::span<char>(storage_));
      storage_[size_] = '\0';
    }

    ~StackString() = default;
    StackString(const StackString& other) = default;
    StackString& operator=(const StackString& other) = default;
    StackString(StackString&& other) = default;
    StackString& operator=(StackString&& other) = default;

    constexpr std::string_view AsString() const {
      return std::string_view(storage_.data(), static_cast<size_t>(size_));
    }

   private:
    std::array<char, kSmallBufferSize + 1u> storage_;
    // We only need one byte for keeping the size of a small string.
    uint8_t size_ = 0;
  };

  // This class stores a fixed-size, immutable string that is always stored in
  // the heap. This is basically a wrapper around base::HeapArray into a
  // copyable / movable class for convenience.
  class BASE_I18N_EXPORT HeapString {
   public:
    explicit HeapString(base::span<const std::string_view> parts);
    HeapString(const HeapString& other);
    HeapString& operator=(const HeapString& other);
    constexpr HeapString(HeapString&& other) noexcept;
    HeapString& operator=(HeapString&&);
    inline constexpr ~HeapString() = default;

    std::string_view AsString() const;

   private:
    base::HeapArray<char> storage_;
  };

  // Constructs an empty string.
  // Adding a void template due to compiler complaining about the constructor
  // being defined in the header, which is necessary for compile-time functions.
  template <typename = void>
  inline constexpr ImmutableString() : storage_(StackString{}) {}

  inline constexpr ~ImmutableString() = default;

  // Constructs the string by joining multiple string_views.
  explicit ImmutableString(base::span<const std::string_view> parts);

  // Compile-time constructor for `ImmutableString`, it needs a first argument
  // the ForceStackString for the compiler to identify which constructor
  // to use. Note: compile-time construction only supports the small-string
  // case as base::HeapArray does not offer constexpr constructors.
  struct ForceStackString {};
  template <typename = void>
  constexpr explicit ImmutableString(ForceStackString,
                                     base::span<const std::string_view> parts)
      : storage_(StorageVariantType(StackString(parts))) {}

  constexpr ImmutableString(const ImmutableString& other);
  constexpr ImmutableString& operator=(const ImmutableString& other);
  constexpr ImmutableString(ImmutableString&& other) noexcept;
  constexpr ImmutableString& operator=(ImmutableString&&) noexcept;

  // Returns the string as a std::string_view.
  constexpr std::string_view AsString() const {
    if (const auto* ss = std::get_if<StackString>(&storage_)) {
      return ss->AsString();
    }
    return std::get<HeapString>(storage_).AsString();
  }

 private:
  using StorageVariantType = std::variant<StackString, HeapString>;
  StorageVariantType storage_;
};

constexpr ImmutableString::HeapString::HeapString(HeapString&& other) noexcept =
    default;
constexpr ImmutableString::ImmutableString(ImmutableString&& other) noexcept =
    default;
inline constexpr ImmutableString& ImmutableString::operator=(
    ImmutableString&&) noexcept = default;
constexpr ImmutableString::ImmutableString(const ImmutableString& other) =
    default;
inline constexpr ImmutableString& ImmutableString::operator=(
    const ImmutableString& other) = default;

}  // namespace base::i18n_internal

#endif  // BASE_I18N_INTERNAL_IMMUTABLE_STRING_H_
