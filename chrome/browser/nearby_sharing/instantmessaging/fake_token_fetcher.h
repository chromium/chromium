// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_INSTANTMESSAGING_FAKE_TOKEN_FETCHER_H_
#define CHROME_BROWSER_NEARBY_SHARING_INSTANTMESSAGING_FAKE_TOKEN_FETCHER_H_

#include <string>

#include "chrome/browser/nearby_sharing/instantmessaging/token_fetcher.h"

class FakeTokenFetcher : public TokenFetcher {
 public:
  FakeTokenFetcher();
  ~FakeTokenFetcher() override;

  void GetAccessToken(
      base::OnceCallback<void(const std::string& token)> callback) override;
  void SetAccessToken(const std::string& token);

 private:
  std::string token_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_INSTANTMESSAGING_FAKE_TOKEN_FETCHER_H_
