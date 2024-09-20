// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/kcer/token_key_finder.h"

#include <optional>

#include "ash/components/kcer/kcer.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"

namespace kcer::internal {

// static
scoped_refptr<TokenKeyFinder> TokenKeyFinder::Create(
    int results_to_receive,
    base::OnceCallback<void(base::expected<std::optional<Token>, Error>)>
        callback) {
  return base::MakeRefCounted<TokenKeyFinder>(
      base::PassKey<TokenKeyFinder>(), results_to_receive, std::move(callback));
}

TokenKeyFinder::TokenKeyFinder(
    base::PassKey<TokenKeyFinder>,
    int results_to_receive,
    base::OnceCallback<void(base::expected<std::optional<Token>, Error>)>
        callback)
    : callbacks_to_create_(results_to_receive),
      results_to_receive_(results_to_receive),
      callback_(std::move(callback)) {
  CHECK_GT(results_to_receive_, 0);
}

TokenKeyFinder::~TokenKeyFinder() {
  // If `callbacks_to_create_` is positive, then not enough callbacks were
  // created and the result was never returned. If negative, then the result
  // was returned before receiving all sub-results.
  CHECK_EQ(callbacks_to_create_, 0);
}

base::OnceCallback<void(base::expected<bool, Error>)>
TokenKeyFinder::GetCallback(Token token) {
  this->callbacks_to_create_--;
  CHECK_GE(this->callbacks_to_create_, 0);
  return base::BindOnce(&TokenKeyFinder::HandleOneResult,
                        base::RetainedRef(this), token);
}

void TokenKeyFinder::HandleOneResult(Token token,
                                     base::expected<bool, Error> result) {
  if (!callback_) {
    // The key was already found earlier, ignore this result.
    return;
  }

  if (result.has_value() && (result.value() == true)) {
    return std::move(callback_).Run(token);
  }

  if (!result.has_value()) {
    CHECK(!base::Contains(errors_, token));
    errors_[token] = result.error();
  }

  if (--results_to_receive_ > 0) {
    // Key not found so far, but more results will come. Wait for them.
    return;
  }
  // else: All results were received, the key was not found.

  if (!errors_.empty()) {
    // Multiple different errors could be present, return just one of them for
    // simplicity.
    return std::move(callback_).Run(base::unexpected(errors_.begin()->second));
  }
  return std::move(callback_).Run(/*token=*/std::nullopt);
}

}  // namespace kcer::internal
