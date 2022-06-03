// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_PROFILES_PROFILE_AVATAR_DOWNLOADER_H_
#define CHROME_BROWSER_PROFILES_PROFILE_AVATAR_DOWNLOADER_H_

#include <stddef.h>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"

namespace gfx {
class Image;
}

class ProfileAvatarDownloader : public BitmapFetcherDelegate {
 public:
  using FetchCompleteCallback = base::OnceCallback<
      void(gfx::Image, const std::string&, const base::FilePath&)>;

  ProfileAvatarDownloader(size_t icon_index, FetchCompleteCallback callback);
  ~ProfileAvatarDownloader() override;

  void Start();

  // BitmapFetcherDelegate:
  void OnFetchComplete(const GURL& url, const SkBitmap* bitmap) override;

 private:
  // Downloads the avatar image from a url.
  std::unique_ptr<BitmapFetcher> fetcher_;

  // Index of the avatar being downloaded.
  size_t icon_index_;

  FetchCompleteCallback callback_;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_AVATAR_DOWNLOADER_H_
