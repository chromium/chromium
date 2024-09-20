// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_KCER_TOKEN_KEY_FINDER_H_
#define ASH_COMPONENTS_KCER_TOKEN_KEY_FINDER_H_

#include <optional>

#include "ash/components/kcer/kcer.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"

namespace kcer::internal {

// A helper class for Kcer::FindKeyLocation. Aggregates results about the
// location of a key from several tokens. Exported for unit tests only.
class COMPONENT_EXPORT(KCER) TokenKeyFinder
    : public base::RefCounted<TokenKeyFinder> {
 public:
  // `results_to_receive` is the number of results to collect before returning
  // the end result.
  static scoped_refptr<TokenKeyFinder> Create(
      int results_to_receive,
      base::OnceCallback<void(base::expected<std::optional<Token>, Error>)>
          callback);

  // Use Create() instead.
  TokenKeyFinder(
      base::PassKey<TokenKeyFinder>,
      int results_to_receive,
      base::OnceCallback<void(base::expected<std::optional<Token>, Error>)>
          callback);

  // Returns a callback to collect one result from `token`, the callback
  // will co-own `this` instance.
  base::OnceCallback<void(base::expected<bool, Error>)> GetCallback(
      Token token);

 private:
  friend class base::RefCounted<TokenKeyFinder>;
  ~TokenKeyFinder();

  void HandleOneResult(Token token, base::expected<bool, Error> result);

  // Guardrail variable to ensure that the merger is used correctly.
  int callbacks_to_create_ = 0;
  // Counter for how many results should still be received.
  int results_to_receive_ = 0;
  // Callback for the end result.
  base::OnceCallback<void(base::expected<std::optional<Token>, Error>)>
      callback_;
  // Errors from failed tokens.
  base::flat_map<Token, Error> errors_;
};

}  // namespace kcer::internal

#endif  // ASH_COMPONENTS_KCER_TOKEN_KEY_FINDER_H_
