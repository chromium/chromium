// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_INTERNAL_BCP47_SUBTAGS_READER_H_
#define BASE_I18N_INTERNAL_BCP47_SUBTAGS_READER_H_

#include <algorithm>
#include <initializer_list>
#include <string_view>

#include "base/notreached.h"
#include "base/strings/string_util.h"

namespace base::i18n_internal {

constexpr bool VerifyAsciiAlphanumeric(std::string_view str) {
  return std::ranges::all_of(
      str, [](char c) { return base::IsAsciiAlphaNumeric(c); });
}

constexpr bool VerifyAsciiNumeric(std::string_view str) {
  return std::ranges::all_of(str, [](char c) { return base::IsAsciiDigit(c); });
}

constexpr bool VerifyAsciiAlpha(std::string_view str) {
  return std::ranges::all_of(str, [](char c) { return base::IsAsciiAlpha(c); });
}

// Primary language subtag: 2-3 alpha characters.
// RFC 5646 Section 2.2.1.
//  language      = 2*3ALPHA
//                  ["-" extlang]
//                  / 4ALPHA
//                  / 5*8ALPHA
// Note: Extended language subtags (extlang) are not supported.
// Note: 4ALPHA language subtags are not supported.
// Note: 5*8ALPHA language subtags are not supported.
constexpr bool IsLanguageSubtag(std::string_view subtag) {
  return subtag.size() >= 2 && subtag.size() <= 3 && VerifyAsciiAlpha(subtag);
}

// Script subtag: 4 alpha characters.
// RFC 5646 Section 2.2.3.
//  script        = 4ALPHA
constexpr bool IsScriptSubtag(std::string_view subtag) {
  return subtag.size() == 4 && VerifyAsciiAlpha(subtag);
}

// Region subtag: 2 alpha characters or 3 digits.
// RFC 5646 Section 2.2.4.
//  region        = 2ALPHA
//                / 3DIGIT
constexpr bool IsRegionSubtag(std::string_view subtag) {
  return (subtag.size() == 2 && VerifyAsciiAlpha(subtag)) ||
         (subtag.size() == 3 && VerifyAsciiNumeric(subtag));
}

// Variant subtag: 5-8 alphanumeric characters, or 4 characters starting with a
// digit. RFC 5646 Section 2.2.5.
//  variant       = 5*8alphanum
//                 / (DIGIT 3alphanum)
constexpr bool IsVariantSubtag(std::string_view subtag) {
  if (subtag.size() >= 5 && subtag.size() <= 8) {
    return VerifyAsciiAlphanumeric(subtag);
  }
  if (subtag.size() == 4) {
    return base::IsAsciiDigit(subtag[0]) && VerifyAsciiAlphanumeric(subtag);
  }
  return false;
}

// Singleton subtag: Single alphanumerics; "x" reserved for private use.
//  singleton     = DIGIT               ; 0 - 9
//                / %x41-57             ; A - W
//                / %x59-5A             ; Y - Z
//                / %x61-77             ; a - w
//                / %x79-7A             ; y - z
constexpr bool IsExtensionSingleton(std::string_view subtag) {
  return subtag.size() == 1 && subtag != "x" && subtag != "X" &&
         VerifyAsciiAlphanumeric(subtag);
}

// extension     = singleton 1*("-" (2*8alphanum))
constexpr bool IsExtensionSubtag(std::string_view subtag) {
  return subtag.size() >= 2 && subtag.size() <= 8 &&
         VerifyAsciiAlphanumeric(subtag);
}

// privateuse    = "x" 1*("-" (1*8alphanum))
constexpr bool IsPrivateUseSingleton(std::string_view subtag) {
  return subtag == "x" || subtag == "X";
}

// privateuse    = "x" 1*("-" (1*8alphanum))
constexpr bool IsPrivateUseSubtag(std::string_view subtag) {
  return subtag.size() >= 1 && subtag.size() <= 8 &&
         VerifyAsciiAlphanumeric(subtag);
}

// This class constructs per-demand a reader over a BCP47 tag without any heap
// allocations, that is the subtags are only parsed when needed.
//
// It provides the following public methods:
//  - HasError(): Returns whether there is an error with the underlying BCP47
//  tag.
//  - IsDone(): Returns whether the reader is done (either completely parsed or
//    encountered an error).
//  - Read(Type type): Returns the current subtag if its type equals `type`
//    and then advances to the next subtag in the underlying BCP47 tag.
//    If the type does not match, returns an empty string without advancing.
class SubtagsReader {
 public:
  // Identifies the type of subtags as per the BCP47 standard.
  enum class Type {
    kLanguage,
    kScript,
    kRegion,
    kVariant,
    kExtensionSingleton,
    kExtensionSubtag,
    kPrivateUseSingleton,
    kPrivateUseSubtag,
    kEmpty,
    kError,
  };

  // Constructs a SubtagsReader from a raw BCP47 string view.
  // Splits off the first subtag (which must be a valid primary language subtag)
  // and saves the rest as `remaining_`.
  constexpr explicit SubtagsReader(std::string_view tag)
      : front_(tag.substr(0, tag.find('-'))),
        remaining_(front_.size() == tag.size() ? std::string_view()
                                               : tag.substr(front_.size() + 1)),
        // Checks if front_ is a language subtag and the remaining does not
        // end with a "-".
        type_(remaining_.ends_with("-") || !IsLanguageSubtag(front_)
                  ? Type::kError
                  : Type::kLanguage) {}

  // The Read method will return the current subtag if its type equals `type`
  // and then advance to the next subtag in the underlying BCP47 tag. If the
  // current subtag's type is not the same, an empty string is returned.
  //
  // Examples:
  //
  //  [Initial State]
  //  "en-Latn-US"
  //   ^
  //   front_ = "en" (type_ = kLanguage)
  //
  //  [Scenario A: Match]
  //  Call: Read(kLanguage) -> Matches!
  //  1. Save front_ ("en") to return.
  //  2. Advance() to next subtag:
  //     "en-Latn-US"
  //         ^
  //         front_ = "Latn" (type_ = kScript)
  //  3. Return "en".
  //
  //  [Scenario B: Mismatch]
  //  Call: Read(kRegion) -> type_ (kScript) != kRegion
  //  1. Do NOT advance (cursor stays on "Latn").
  //  2. Return "" (empty string).
  constexpr std::string_view Read(Type type) {
    if (type_ != type) {
      return std::string_view();
    }
    std::string_view read_front = front_;
    Advance();
    return read_front;
  }

  // Returns whether there is an error with the underlying BCP47 tag.
  constexpr bool HasError() const { return type_ == Type::kError; }
  // Returns whether the reader is done. This is true if the underlying tag has
  // been read completely or an error was found.
  constexpr bool IsDone() const {
    return type_ == Type::kError || type_ == Type::kEmpty;
  }

 private:
  // Advances the current subtag to the next one.
  constexpr void Advance() {
    size_t next = remaining_.find('-');
    front_ = remaining_.substr(0, next);
    remaining_.remove_prefix(next == std::string_view::npos ? remaining_.size()
                                                            : next + 1);
    type_ = GetNextSubtagType();
  }
  // Returns whether `subtag` is of type `type`.
  static constexpr bool IsSubtagType(Type type, std::string_view subtag) {
    switch (type) {
      case Type::kLanguage:
        return IsLanguageSubtag(subtag);
      case Type::kScript:
        return IsScriptSubtag(subtag);
      case Type::kRegion:
        return IsRegionSubtag(subtag);
      case Type::kVariant:
        return IsVariantSubtag(subtag);
      case Type::kExtensionSingleton:
        return IsExtensionSingleton(subtag);
      case Type::kExtensionSubtag:
        return IsExtensionSubtag(subtag);
      case Type::kPrivateUseSingleton:
        return IsPrivateUseSingleton(subtag);
      case Type::kPrivateUseSubtag:
        return IsPrivateUseSubtag(subtag);
      case Type::kEmpty:
      case Type::kError:
        return false;
    }
  }

  // Finds the first type in `next_types` such that `subtag` satisfies
  // `IsSubtagType(next_type, subtag)`. If no type is found, Type::kError is
  // returned.
  // Note: std::initializer_list is used to avoid heap allocations.
  static constexpr Type FindNextType(std::string_view subtag,
                                     std::initializer_list<Type> next_types) {
    for (Type next_type : next_types) {
      if (IsSubtagType(next_type, subtag)) {
        return next_type;
      }
    }
    return Type::kError;
  }

  // Returns the next type given a `current_type` and a `front_`. The current
  // type is assumed to be `type_` and that the `front_` and `remaining_` have
  // already been Advanceed to the next subtag and `type_` is yet to be
  // determined.
  constexpr Type GetNextSubtagType() const {
    if (front_.empty()) {
      // If a singleton was seen, there must be a least a subtag following it.
      // If the `front_` is empty, and the remaining is not, it means we got an
      // empty subtag in the middle of the tag ("--" case).
      if (!remaining_.empty() || type_ == Type::kExtensionSingleton ||
          type_ == Type::kPrivateUseSingleton) {
        return Type::kError;
      }
      return Type::kEmpty;
    }
    // Each switch-case statement here determines what are the types of subtags
    // that can follow the `type_`. For example, for Type::kLanguage,
    // anything can follow it besides extension subtags or another language
    // subtag.
    switch (type_) {
      case Type::kLanguage:
        return FindNextType(
            front_, {Type::kScript, Type::kRegion, Type::kVariant,
                     Type::kPrivateUseSingleton, Type::kExtensionSingleton});
      case Type::kScript:
        return FindNextType(
            front_, {Type::kRegion, Type::kVariant, Type::kPrivateUseSingleton,
                     Type::kExtensionSingleton});
      case Type::kRegion:
        return FindNextType(front_, {Type::kVariant, Type::kPrivateUseSingleton,
                                     Type::kExtensionSingleton});
      case Type::kVariant:
        return FindNextType(front_, {Type::kVariant, Type::kPrivateUseSingleton,
                                     Type::kExtensionSingleton});
      case Type::kExtensionSingleton:
        return FindNextType(front_, {Type::kExtensionSubtag});
      case Type::kExtensionSubtag:
        return FindNextType(front_,
                            {Type::kExtensionSubtag, Type::kExtensionSingleton,
                             Type::kPrivateUseSingleton});
      case Type::kPrivateUseSingleton:
        return FindNextType(front_, {Type::kPrivateUseSubtag});
      case Type::kPrivateUseSubtag:
        return FindNextType(front_, {Type::kPrivateUseSubtag});
      case Type::kEmpty:
        return Type::kEmpty;
      case Type::kError:
        return Type::kError;
    }
  }

  std::string_view front_;
  std::string_view remaining_;
  // The first subtag is always the language while we do not enable
  // private-use-only tags.
  // T0DO(crbug.com/537806159): support private-use-only  language tags.
  Type type_ = Type::kLanguage;
};

}  // namespace base::i18n_internal

#endif  // BASE_I18N_INTERNAL_BCP47_SUBTAGS_READER_H_
