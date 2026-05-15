// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_INTERNAL_IMMUTABLE_STRING_H_
#define BASE_I18N_INTERNAL_IMMUTABLE_STRING_H_

#include <sys/types.h>

#include <array>
#include <string_view>
#include <variant>

#include "base/containers/heap_array.h"
#include "base/i18n/base_i18n_export.h"

namespace base::i18n::internal {

// An immutable string storage that optimizes for memory usage by using a small
// stack-allocated buffer (SSO) and falling back to a heap-allocated buffer for
// larger strings.
class BASE_I18N_EXPORT ImmutableString {
 public:
  // The size limit where we expect to keep things all in the stack.
  static constexpr size_t kSmallBufferSize = 12;

  // Class that stores a small (determined by `kSmallBufferSize`), fixed-size
  // and immutable string. The class is copyable and movable for convenient
  // implementation of `ImmutableString`.
  class BASE_I18N_EXPORT SmallStackString {
   public:
    explicit SmallStackString(std::string_view input);
    ~SmallStackString() = default;
    SmallStackString(const SmallStackString& other) = default;
    SmallStackString& operator=(const SmallStackString& other) = default;
    SmallStackString(SmallStackString&& other) = default;
    SmallStackString& operator=(SmallStackString&& other) = default;

    std::string_view AsString() const;

   private:
    std::array<char, kSmallBufferSize> storage_;
    // We only need one byte for keeping the size of a small string.
    uint8_t size_;
  };

  // This class stores a fixed-size, immutable string that is always stored in
  // the heap. This is basically a wrapper around base::HeapArray into a
  // copyable / movable class for convenience.
  class BASE_I18N_EXPORT HeapString {
   public:
    explicit HeapString(std::string_view input);
    HeapString(const HeapString& other);
    HeapString& operator=(const HeapString& other);
    HeapString(HeapString&&);
    HeapString& operator=(HeapString&&);
    ~HeapString();

    std::string_view AsString() const;

   private:
    base::HeapArray<char> storage_;
  };

  // Constructs an empty string.
  ImmutableString();
  ~ImmutableString();

  // Constructs the string from a string_view.
  explicit ImmutableString(std::string_view input);

  // Copy constructor and assignment operator are required because
  // base::HeapArray is move-only.
  ImmutableString(const ImmutableString& other);
  ImmutableString& operator=(const ImmutableString& other);

  ImmutableString(ImmutableString&&);
  ImmutableString& operator=(ImmutableString&&);

  // Returns the string as a std::string_view.
  std::string_view AsString() const;

 private:
  using StorageVariantType = std::variant<SmallStackString, HeapString>;
  StorageVariantType storage_;
};

}  // namespace base::i18n::internal

#endif  // BASE_I18N_INTERNAL_IMMUTABLE_STRING_H_
