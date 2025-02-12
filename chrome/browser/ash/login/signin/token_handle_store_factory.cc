// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/token_handle_store_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/login/signin/token_handle_util.h"

namespace ash {

TokenHandleStoreFactory::TokenHandleStoreFactory() = default;

TokenHandleStoreFactory::~TokenHandleStoreFactory() = default;

// static
TokenHandleStoreFactory* TokenHandleStoreFactory::Get() {
  static base::NoDestructor<TokenHandleStoreFactory> instance;
  return instance.get();
}

TokenHandleStore* TokenHandleStoreFactory::GetTokenHandleStore() {
  if (token_handle_store_ == nullptr) {
    // TODO(b/383733245): switch based on feature flag state.
    token_handle_store_ = std::make_unique<TokenHandleUtil>();
  }

  return token_handle_store_.get();
}

}  // namespace ash
