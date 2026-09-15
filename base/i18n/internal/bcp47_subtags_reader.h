// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_INTERNAL_BCP47_SUBTAGS_READER_H_
#define BASE_I18N_INTERNAL_BCP47_SUBTAGS_READER_H_

#include <algorithm>
#include <initializer_list>
#include <optional>
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

// Identifies the type of subtags as per the BCP47 standard.
// The order of the types within the enum here is important and we use it
// during parsing (please, do not change it).
enum class SubtagType {
  kLanguage,
  kScript,
  kRegion,
  kVariant,
  kExtensionSingleton,
  kExtensionSubtag,
  kPrivateUseSingleton,
  kPrivateUseSubtag,
};

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
// allocations, that is, the subtags are only parsed when needed.
//
// It provides the following public methods:
//  - HasError(): Returns whether there is an error with the underlying BCP47
//    tag.
//  - IsDone(): Returns whether the reader is done (either completely parsed or
//    encountered an error).
//  - Read(SubtagType type): Returns the current subtag if its type equals
//  `type`
//    and then advances to the next subtag in the underlying BCP47 tag.
//    If the type does not match, returns an empty string without advancing.
//  - ReadSubtags(SubtagType type): Reads and returns all contiguous subtags
//  matching
//    `type`, advancing the reader past them.
//  - Seek(SubtagType type): Advances the reader until the current subtag's type
//  matches `type` or becomes unreachable based on subtag ordering rules.
//
// Handling errors:
//
// If a parsing error is found, `HasError()` and `IsDone()` will always return
// true and `Read()` will have no effect after that (an empty string is
// returned). Also, a false `HasError()` has only significance over what
// `SubtagsReader` has parsed so far, i.e. `HasError()` can return
// false and at some point start returning true if it reaches a parsing error.
// The same applies to `Read()` as it returning a non-empty string does not mean
// anything about the underlying `tag` as a whole, only that it was able to
// parse the current tag (and that so far no parsing errors were found).
//
// If you want to access the correctness of the tag `SubtagsReader` is acting
// upon, you must read it until `IsDone()` is true and verify that `HasError()`
// is false.
class SubtagsReader {
 public:
  // Constructs a SubtagsReader from a raw BCP47 string view.
  // Splits off the first subtag (which must be a valid primary language subtag)
  // and saves the rest as `remaining_`.
  constexpr explicit SubtagsReader(std::string_view tag)
      : front_(tag.substr(0, tag.find('-'))),
        remaining_(front_.size() == tag.size() ? std::string_view()
                                               : tag.substr(front_.size() + 1)),
        // Checks if front_ is a language subtag and the remaining does not
        // end with a "-".
        has_error_(remaining_.ends_with("-") || !IsLanguageSubtag(front_)) {}

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
  constexpr std::string_view Read(SubtagType type) {
    if (type_ != type) {
      return std::string_view();
    }
    std::string_view read_front = front_;
    Advance();
    return read_front;
  }

  // Reads and returns all contiguous subtags of the given `type` starting
  // from the current position, advancing the reader past them.
  // Returns an empty vector if the current subtag does not match `type`.
  constexpr std::vector<std::string_view> ReadSubtags(SubtagType type) {
    std::vector<std::string_view> result;
    std::string_view read_subtag;
    while (!(read_subtag = Read(type)).empty()) {
      result.push_back(read_subtag);
    }
    return result;
  }

  // Advances the reader until the current subtag's type matches `type` or is
  // no longer reachable (meaning a subtag of `type` cannot follow the current
  // subtag type according to BCP47 subtag ordering rules).
  // Returns a reference to this reader to allow method chaining.
  constexpr SubtagsReader& Seek(SubtagType type) {
    while (type_ && type_ != type && !IsDone() && IsReachable(*type_, type)) {
      Advance();
    }
    return *this;
  }

  // Returns whether there is an error with the underlying BCP47 tag.
  constexpr bool HasError() const { return has_error_; }
  // Returns whether the reader is done. This is true if the underlying tag has
  // been read completely or an error was found.
  constexpr bool IsDone() const { return has_error_ || !type_.has_value(); }

 private:
  static constexpr bool IsReachable(SubtagType lhs, SubtagType rhs) {
    // Extension subtag to extension singleton is the only valid type-loop.
    return lhs < rhs || (lhs == SubtagType::kExtensionSubtag &&
                         rhs == SubtagType::kExtensionSingleton);
  }
  // Advances the current subtag to the next one.
  constexpr void Advance() {
    if (IsDone()) {
      return;
    }
    size_t next = remaining_.find('-');
    front_ = remaining_.substr(0, next);
    remaining_.remove_prefix(next == std::string_view::npos ? remaining_.size()
                                                            : next + 1);
    SubtagType current_type = *type_;
    type_ = GetNextSubtagType(front_, current_type);
    // This means that a next subtag could not be found, this could be either
    // because the whole input has been parsed or an error was found.
    if (!type_.has_value()) {
      // If a singleton was seen, there must be a least a subtag following it.
      if (current_type == SubtagType::kExtensionSingleton ||
          current_type == SubtagType::kPrivateUseSingleton) {
        has_error_ = true;
        return;
      }
      // That is the "--" case.
      if (!remaining_.empty()) {
        has_error_ = true;
        return;
      }
    }
  }
  // Returns whether `subtag` is of type `type`.
  static constexpr bool IsSubtagType(SubtagType type, std::string_view subtag) {
    switch (type) {
      case SubtagType::kLanguage:
        return IsLanguageSubtag(subtag);
      case SubtagType::kScript:
        return IsScriptSubtag(subtag);
      case SubtagType::kRegion:
        return IsRegionSubtag(subtag);
      case SubtagType::kVariant:
        return IsVariantSubtag(subtag);
      case SubtagType::kExtensionSingleton:
        return IsExtensionSingleton(subtag);
      case SubtagType::kExtensionSubtag:
        return IsExtensionSubtag(subtag);
      case SubtagType::kPrivateUseSingleton:
        return IsPrivateUseSingleton(subtag);
      case SubtagType::kPrivateUseSubtag:
        return IsPrivateUseSubtag(subtag);
    }
  }

  // Finds the first type in `next_types` such that `subtag` satisfies
  // `IsSubtagType(next_type, subtag)`. If no type is found,
  // std::nullopt is returned. Note: std::initializer_list is used to
  // avoid heap allocations.
  static constexpr std::optional<SubtagType> FindNextSubtagType(
      std::string_view subtag,
      std::initializer_list<SubtagType> next_types) {
    for (SubtagType next_type : next_types) {
      if (IsSubtagType(next_type, subtag)) {
        return next_type;
      }
    }
    return std::nullopt;
  }

  // Returns the next type given a `current_type` and a `front_`. The current
  // type is assumed to be `type_` and that the `front_` and `remaining_` have
  // already been advanceed to the next subtag and `type_` is yet to be
  // determined.
  static constexpr std::optional<SubtagType> GetNextSubtagType(
      std::string_view front,
      SubtagType current_type) {
    if (front.empty()) {
      return std::nullopt;
    }
    // Each switch-case statement here determines what are the types of subtags
    // that can follow the `type_`. For example, for SubtagType::kLanguage,
    // anything can follow it besides extension subtags or another language
    // subtag.
    switch (current_type) {
      case SubtagType::kLanguage:
        return FindNextSubtagType(
            front, {SubtagType::kScript, SubtagType::kRegion,
                    SubtagType::kVariant, SubtagType::kPrivateUseSingleton,
                    SubtagType::kExtensionSingleton});
      case SubtagType::kScript:
        return FindNextSubtagType(front,
                                  {SubtagType::kRegion, SubtagType::kVariant,
                                   SubtagType::kPrivateUseSingleton,
                                   SubtagType::kExtensionSingleton});
      case SubtagType::kRegion:
        return FindNextSubtagType(
            front, {SubtagType::kVariant, SubtagType::kPrivateUseSingleton,
                    SubtagType::kExtensionSingleton});
      case SubtagType::kVariant:
        return FindNextSubtagType(
            front, {SubtagType::kVariant, SubtagType::kPrivateUseSingleton,
                    SubtagType::kExtensionSingleton});
      case SubtagType::kExtensionSingleton:
        return FindNextSubtagType(front, {SubtagType::kExtensionSubtag});
      case SubtagType::kExtensionSubtag:
        return FindNextSubtagType(front, {SubtagType::kExtensionSubtag,
                                          SubtagType::kExtensionSingleton,
                                          SubtagType::kPrivateUseSingleton});
      case SubtagType::kPrivateUseSingleton:
        return FindNextSubtagType(front, {SubtagType::kPrivateUseSubtag});
      case SubtagType::kPrivateUseSubtag:
        return FindNextSubtagType(front, {SubtagType::kPrivateUseSubtag});
    }
  }

  std::string_view front_;
  std::string_view remaining_;
  // The first subtag is always the language while we do not enable
  // private-use-only tags.
  // A std::nullopt `type_` could mean error or that there is not more subtags
  // to be read.
  // TODO(crbug.com/537806159): support private-use-only  language
  // tags.
  std::optional<SubtagType> type_ = SubtagType::kLanguage;
  bool has_error_ = false;
};

}  // namespace base::i18n_internal

#endif  // BASE_I18N_INTERNAL_BCP47_SUBTAGS_READER_H_
