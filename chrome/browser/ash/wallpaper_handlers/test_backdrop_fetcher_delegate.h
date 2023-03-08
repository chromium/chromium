// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_TEST_BACKDROP_FETCHER_DELEGATE_H_
#define CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_TEST_BACKDROP_FETCHER_DELEGATE_H_

#include <memory>
#include <string>

#include "chrome/browser/ash/wallpaper_handlers/backdrop_fetcher_delegate.h"

namespace wallpaper_handlers {

class BackdropCollectionInfoFetcher;
class BackdropImageInfoFetcher;

class TestBackdropFetcherDelegate : public BackdropFetcherDelegate {
 public:
  TestBackdropFetcherDelegate();

  TestBackdropFetcherDelegate(const TestBackdropFetcherDelegate&) = delete;
  TestBackdropFetcherDelegate& operator=(const TestBackdropFetcherDelegate&) =
      delete;

  ~TestBackdropFetcherDelegate() override;

  // BackdropFetcherDelegate:
  std::unique_ptr<BackdropCollectionInfoFetcher>
  CreateBackdropCollectionInfoFetcher() const override;
  std::unique_ptr<BackdropImageInfoFetcher> CreateBackdropImageInfoFetcher(
      const std::string& collection_id) const override;
};

}  // namespace wallpaper_handlers
#endif  // CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_TEST_BACKDROP_FETCHER_DELEGATE_H_
