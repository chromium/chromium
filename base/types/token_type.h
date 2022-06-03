// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TYPES_TOKEN_TYPE_H_
#define BASE_TYPES_TOKEN_TYPE_H_

#include <type_traits>

#include "base/types/strong_alias.h"
#include "base/unguessable_token.h"

namespace base {

// A specialization of StrongAlias for UnguessableToken. Unlike
// UnguessableToken, a TokenType<...> does not default to null and does not
// expose the concept of null tokens. If you need to indicate a null token,
// please use absl::optional<TokenType<...>>.
template <typename TypeMarker>
class TokenType : public StrongAlias<TypeMarker, UnguessableToken> {
 private:
  using Super = StrongAlias<TypeMarker, UnguessableToken>;

 public:
  TokenType() : Super(UnguessableToken::Create()) {}
  // The parameter |unused| is here to prevent multiple definitions of a
  // single argument constructor. This is only needed during the migration to
  // strongly typed frame tokens.
  explicit TokenType(const UnguessableToken& token) : Super(token) {}
  TokenType(const TokenType& token) : Super(token.value()) {}
  TokenType(TokenType&& token) noexcept : Super(token.value()) {}
  TokenType& operator=(const TokenType& token) = default;
  TokenType& operator=(TokenType&& token) noexcept = default;

  // This object allows default assignment operators for compatibility with
  // STL containers.

  // Hash functor for use in unordered containers.
  struct Hasher {
    using argument_type = TokenType;
    using result_type = size_t;
    result_type operator()(const argument_type& token) const {
      return UnguessableTokenHash()(token.value());
    }
  };

  // Mimic the UnguessableToken API for ease and familiarity of use.
  std::string ToString() const { return this->value().ToString(); }
};

}  // namespace base

#endif  // BASE_TYPES_TOKEN_TYPE_H_
