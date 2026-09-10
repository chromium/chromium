// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_INTERNAL_IMMUTABLE_STRING_H_
#define BASE_I18N_INTERNAL_IMMUTABLE_STRING_H_

#include <sys/types.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <numeric>
#include <string_view>
#include <utility>
#include <variant>

#include "base/component_export.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

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
// There is also a consteval constructor to support string literals. In that
// case, only a std::string_view is kept. Note that this is safe (no dangling
// pointers can happen) as the consteval constructor guarantees that the inner
// std::string_view here is pointing to a string literal constructed at
// compile-time.
class COMPONENT_EXPORT(I18N_INTERNAL) ImmutableString {
 public:
  // The size limit where we expect to keep things all in the stack.
  static constexpr size_t kSmallBufferSize = 14;

  // Class that stores a small (determined by `kSmallBufferSize`), fixed-size
  // and immutable string. The class is copyable and movable for convenient
  // implementation of `ImmutableString`.
  class COMPONENT_EXPORT(I18N_INTERNAL) StackString {
   public:
    inline constexpr StackString();
    inline constexpr explicit StackString(
        base::span<const std::string_view> parts);

    ~StackString() = default;
    StackString(const StackString& other) = default;
    StackString& operator=(const StackString& other) = default;
    StackString(StackString&& other) = default;
    StackString& operator=(StackString&& other) = default;

    inline constexpr std::string_view AsString() const;

   private:
    std::array<char, kSmallBufferSize + 1u> storage_;
    // We only need one byte for keeping the size of a small string.
    uint8_t size_ = 0;
  };

  // Constructs an empty string.
  inline constexpr ImmutableString();
  inline constexpr ~ImmutableString() = default;

  // Constructs the string by joining multiple string_views.
  inline constexpr explicit ImmutableString(
      base::span<const std::string_view> parts);

  // Compile-time constructor for `ImmutableString`, it needs a first argument
  // the ForceConstevalConstructor for the compiler to identify which
  // constructor to use.
  struct ForceConstevalConstructor {};

  // Compile-time constructor for long `ImmutableString` utilizing
  // std::string_view. Marked `consteval` to guarantee it can only be invoked in
  // compile-time contexts, preventing runtime dangling pointer bugs.
  inline consteval ImmutableString(ForceConstevalConstructor,
                                   std::string_view consteval_string);

  inline constexpr ImmutableString(const ImmutableString& other);
  inline constexpr ImmutableString& operator=(const ImmutableString& other);
  inline constexpr ImmutableString(ImmutableString&& other) noexcept;
  inline constexpr ImmutableString& operator=(ImmutableString&& other) noexcept;

  // Returns the string as a std::string_view.
  constexpr std::string_view AsString() const {
    return std::visit(
        absl::Overload{[](const StackString& s) { return s.AsString(); },
                       [](const base::HeapArray<char>& s) {
                         return std::string_view(s.data(), s.size());
                       },
                       [](const std::string_view& s) { return s; }},
        storage_);
  }

 private:
  inline constexpr void Copy(const ImmutableString& other);

  using StorageVariantType =
      std::variant<StackString, base::HeapArray<char>, std::string_view>;
  StorageVariantType storage_;
};

inline constexpr ImmutableString::StackString::StackString() : storage_{} {}

inline constexpr ImmutableString::StackString::StackString(
    base::span<const std::string_view> parts)
    : storage_{}, size_(base::checked_cast<uint8_t>(TotalSize(parts))) {
  CopyParts(parts, base::span<char>(storage_));
  storage_[size_] = '\0';
}

inline constexpr std::string_view ImmutableString::StackString::AsString()
    const {
  return std::string_view(storage_).substr(0u, static_cast<size_t>(size_));
}

inline constexpr ImmutableString::ImmutableString() : storage_(StackString{}) {}

inline constexpr ImmutableString::ImmutableString(
    base::span<const std::string_view> parts) {
  if (TotalSize(parts) <= ImmutableString::kSmallBufferSize) {
    storage_ = StackString(parts);
  } else {
    auto array = base::HeapArray<char>::Uninit(TotalSize(parts));
    CopyParts(parts, array.as_span());
    storage_ = std::move(array);
  }
}

inline consteval ImmutableString::ImmutableString(
    ForceConstevalConstructor,
    std::string_view consteval_string)
    : storage_(consteval_string) {}

inline constexpr void ImmutableString::Copy(const ImmutableString& other) {
  std::visit(
      absl::Overload{[this](const StackString& s) { storage_ = s; },
                     [this](const base::HeapArray<char>& s) {
                       storage_ = base::HeapArray<char>::CopiedFrom(s);
                     },
                     [this](const std::string_view& s) { storage_ = s; }},
      other.storage_);
}

inline constexpr ImmutableString::ImmutableString(
    const ImmutableString& other) {
  Copy(other);
}

inline constexpr ImmutableString& ImmutableString::operator=(
    const ImmutableString& other) {
  if (this != &other) {
    Copy(other);
  }
  return *this;
}

inline constexpr ImmutableString::ImmutableString(
    ImmutableString&& other) noexcept = default;
inline constexpr ImmutableString& ImmutableString::operator=(
    ImmutableString&& other) noexcept = default;

static_assert(sizeof(ImmutableString) <= 24,
              "Keep ImmutableString's stack footprint low.");

static_assert(sizeof(ImmutableString) <= 24,
              "Keep ImmutableString's stack footprint low.");

}  // namespace base::i18n_internal

#endif  // BASE_I18N_INTERNAL_IMMUTABLE_STRING_H_
