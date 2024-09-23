// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/kcer/kcer_token.h"

#include "ash/components/kcer/chaps/high_level_chaps_client.h"
#include "ash/components/kcer/kcer_nss/kcer_token_impl_nss.h"
#include "ash/components/kcer/kcer_token_impl.h"

namespace kcer::internal {

// static
std::unique_ptr<KcerToken> KcerToken::CreateWithoutNss(
    Token token,
    HighLevelChapsClient* chaps_client) {
  return std::make_unique<KcerTokenImpl>(token, chaps_client);
}

// static
std::unique_ptr<KcerToken> KcerToken::CreateForNss(
    Token token,
    HighLevelChapsClient* chaps_client) {
  return std::make_unique<KcerTokenImplNss>(token, chaps_client);
}

}  // namespace kcer::internal
