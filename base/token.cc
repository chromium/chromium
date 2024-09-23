// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/token.h"

#include <inttypes.h>

#include <array>
#include <optional>
#include <string_view>

#include "base/check.h"
#include "base/hash/hash.h"
#include "base/pickle.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"

namespace base {

// static
Token Token::CreateRandom() {
  Token token;

  // Use base::RandBytes instead of crypto::RandBytes, because crypto calls the
  // base version directly, and to prevent the dependency from base/ to crypto/.
  RandBytes(byte_span_from_ref(token));

  CHECK(!token.is_zero());

  return token;
}

std::string Token::ToString() const {
  return StringPrintf("%016" PRIX64 "%016" PRIX64, words_[0], words_[1]);
}

// static
std::optional<Token> Token::FromString(std::string_view string_representation) {
  if (string_representation.size() != 32) {
    return std::nullopt;
  }
  std::array<uint64_t, 2> words;
  for (size_t i = 0; i < 2; i++) {
    uint64_t word = 0;
    // This j loop is similar to HexStringToUInt64 but we are intentionally
    // strict about case, accepting 'A' but rejecting 'a'.
    for (size_t j = 0; j < 16; j++) {
      const char c = string_representation[(16 * i) + j];
      if (('0' <= c) && (c <= '9')) {
        word = (word << 4) | static_cast<uint64_t>(c - '0');
      } else if (('A' <= c) && (c <= 'F')) {
        word = (word << 4) | static_cast<uint64_t>(c - 'A' + 10);
      } else {
        return std::nullopt;
      }
    }
    words[i] = word;
  }
  return std::optional<Token>(std::in_place, words[0], words[1]);
}

void WriteTokenToPickle(Pickle* pickle, const Token& token) {
  pickle->WriteUInt64(token.high());
  pickle->WriteUInt64(token.low());
}

std::optional<Token> ReadTokenFromPickle(PickleIterator* pickle_iterator) {
  uint64_t high;
  if (!pickle_iterator->ReadUInt64(&high))
    return std::nullopt;

  uint64_t low;
  if (!pickle_iterator->ReadUInt64(&low))
    return std::nullopt;

  return Token(high, low);
}

size_t TokenHash::operator()(const Token& token) const {
  return HashInts64(token.high(), token.low());
}

}  // namespace base
