// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_COMMON_FAST_PAIR_FAST_PAIR_HTTP_RESULT_H_
#define ASH_QUICK_PAIR_COMMON_FAST_PAIR_FAST_PAIR_HTTP_RESULT_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

namespace ash {
namespace quick_pair {

// This class is used to represent server errors (both network and HTTP errors)
// we encounter in the repository component.
class COMPONENT_EXPORT(QUICK_PAIR_COMMON) FastPairHttpResult {
 public:
  FastPairHttpResult(const int net_error,
                     const network::mojom::URLResponseHead* head);
  FastPairHttpResult(const FastPairHttpResult&) = delete;
  FastPairHttpResult& operator=(const FastPairHttpResult&) = delete;
  FastPairHttpResult& operator=(FastPairHttpResult&&) = delete;
  ~FastPairHttpResult();

  std::optional<int> net_error() const { return net_error_; }
  std::optional<int> http_response_error() const {
    return http_response_error_;
  }

  bool IsSuccess() const;
  std::string ToString() const;

 private:
  enum class Type { kSuccess, kNetworkFailure, kHttpFailure } type_;

  // Only set if the code is an error, i.e., not set on success.
  std::optional<int> net_error_;
  std::optional<int> http_response_error_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_COMMON_FAST_PAIR_FAST_PAIR_HTTP_RESULT_H_
