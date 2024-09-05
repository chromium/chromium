// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_KCER_TOKEN_RESULTS_MERGER_H_
#define ASH_COMPONENTS_KCER_TOKEN_RESULTS_MERGER_H_

#include <type_traits>
#include <vector>

#include "ash/components/kcer/kcer.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"

namespace kcer::internal {

// This type is used for unit tests.
struct MoveOnlyType;

// A helper class for Kcer methods that work with several tokens in parallel.
// Collects and aggregates results from each token before returning the end
// result.
template <typename T>
class TokenResultsMerger : public base::RefCounted<TokenResultsMerger<T>> {
 public:
  static_assert(std::is_same<T, scoped_refptr<const Cert>>::value ||
                std::is_same<T, PublicKey>::value ||
                std::is_same<T, MoveOnlyType>::value);

  // `results_to_receive` is the amount of results to collect before
  // aggregating and returning them.
  static scoped_refptr<TokenResultsMerger> Create(
      int results_to_receive,
      base::OnceCallback<void(std::vector<T>, base::flat_map<Token, Error>)>
          callback);

  // Use Create() instead.
  TokenResultsMerger(
      base::PassKey<TokenResultsMerger<T>>,
      int results_to_receive,
      base::OnceCallback<void(std::vector<T>, base::flat_map<Token, Error>)>
          callback)
      : callbacks_to_create_(results_to_receive),
        results_to_receive_(results_to_receive),
        callback_(std::move(callback)) {
    CHECK_GT(results_to_receive_, 0);
  }

  // Returns a callback to collect one result from `token`, the callback
  // will co-own `this` instance.
  base::OnceCallback<void(base::expected<std::vector<T>, Error>)> GetCallback(
      Token token);

 private:
  friend class base::RefCounted<TokenResultsMerger<T>>;

  ~TokenResultsMerger() {
    // If `callbacks_to_create_` is positive, then not enough callbacks were
    // created and the result was never returned. If negative, then the result
    // was returned before receiving all sub-results.
    CHECK_EQ(callbacks_to_create_, 0);
  }

 private:
  void HandleOneResult(Token token,
                       base::expected<std::vector<T>, Error> result);

  // Guardrail variable to ensure that the merger is used correctly.
  int callbacks_to_create_ = 0;
  // Counter for how many results should still be received.
  int results_to_receive_ = 0;
  // Callback for the end result.
  base::OnceCallback<void(std::vector<T>, base::flat_map<Token, Error>)>
      callback_;
  // Objects from succeeded tokens.
  std::vector<T> good_results_;
  // Errors from failed tokens.
  base::flat_map<Token, Error> errors_;
};

// static
template <typename T>
scoped_refptr<TokenResultsMerger<T>> TokenResultsMerger<T>::Create(
    int results_to_receive,
    base::OnceCallback<void(std::vector<T>, base::flat_map<Token, Error>)>
        callback) {
  return base::MakeRefCounted<TokenResultsMerger<T>>(
      base::PassKey<TokenResultsMerger<T>>(), results_to_receive,
      std::move(callback));
}

template <typename T>
base::OnceCallback<void(base::expected<std::vector<T>, Error>)>
TokenResultsMerger<T>::GetCallback(Token token) {
  this->callbacks_to_create_--;
  CHECK_GE(this->callbacks_to_create_, 0);
  return base::BindOnce(&TokenResultsMerger<T>::HandleOneResult,
                        base::RetainedRef(this), token);
}

template <typename T>
void TokenResultsMerger<T>::HandleOneResult(
    Token token,
    base::expected<std::vector<T>, Error> result) {
  if (result.has_value()) {
    good_results_.reserve(good_results_.size() + result.value().size());
    std::move(result.value().begin(), result.value().end(),
              std::back_inserter(good_results_));
  } else {
    CHECK(!base::Contains(errors_, token));
    errors_[token] = result.error();
  }
  --results_to_receive_;

  if (results_to_receive_ == 0) {
    std::move(callback_).Run(std::move(good_results_), std::move(errors_));
  }
}

}  // namespace kcer::internal

#endif  // ASH_COMPONENTS_KCER_TOKEN_RESULTS_MERGER_H_
