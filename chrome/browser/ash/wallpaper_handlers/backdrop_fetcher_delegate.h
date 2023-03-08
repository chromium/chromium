// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_BACKDROP_FETCHER_DELEGATE_H_
#define CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_BACKDROP_FETCHER_DELEGATE_H_

#include <memory>
#include <string>

namespace wallpaper_handlers {

class BackdropCollectionInfoFetcher;
class BackdropImageInfoFetcher;

// Delegate class for creating backdrop fetchers. Abstract class to allow
// mocking out in test.
class BackdropFetcherDelegate {
 public:
  virtual ~BackdropFetcherDelegate() = default;

  virtual std::unique_ptr<BackdropCollectionInfoFetcher>
  CreateBackdropCollectionInfoFetcher() const = 0;

  virtual std::unique_ptr<BackdropImageInfoFetcher>
  CreateBackdropImageInfoFetcher(const std::string& collection_id) const = 0;
};

class BackdropFetcherDelegateImpl : public BackdropFetcherDelegate {
 public:
  BackdropFetcherDelegateImpl();

  BackdropFetcherDelegateImpl(const BackdropFetcherDelegateImpl&) = delete;
  BackdropFetcherDelegateImpl& operator=(const BackdropFetcherDelegateImpl&) =
      delete;

  ~BackdropFetcherDelegateImpl() override;

  // BackdropFetcherDelegate:
  std::unique_ptr<BackdropCollectionInfoFetcher>
  CreateBackdropCollectionInfoFetcher() const override;

  std::unique_ptr<BackdropImageInfoFetcher> CreateBackdropImageInfoFetcher(
      const std::string& collection_id) const override;
};

}  // namespace wallpaper_handlers

#endif  // CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_BACKDROP_FETCHER_DELEGATE_H_
