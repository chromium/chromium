// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/instantmessaging/fake_token_fetcher.h"

FakeTokenFetcher::FakeTokenFetcher()
    : TokenFetcher(/*identity_manager=*/nullptr) {}

FakeTokenFetcher::~FakeTokenFetcher() = default;

void FakeTokenFetcher::GetAccessToken(
    base::OnceCallback<void(const std::string& token)> callback) {
  std::move(callback).Run(token_);
}

void FakeTokenFetcher::SetAccessToken(const std::string& token) {
  token_ = token;
}
