// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BITMAP_FETCHER_BITMAP_FETCHER_DELEGATE_H_
#define CHROME_BROWSER_BITMAP_FETCHER_BITMAP_FETCHER_DELEGATE_H_

#include "url/gurl.h"

class SkBitmap;

// A delegate interface for users of BitmapFetcher.
class BitmapFetcherDelegate {
 public:
  BitmapFetcherDelegate() {}

  BitmapFetcherDelegate(const BitmapFetcherDelegate&) = delete;
  BitmapFetcherDelegate& operator=(const BitmapFetcherDelegate&) = delete;

  // This will be called when the bitmap has been requested, whether or not the
  // request succeeds.  |url| is the URL that was originally fetched so we can
  // match up the bitmap with a specific request.  |bitmap| may be NULL if the
  // image fails to be downloaded or decoded.
  virtual void OnFetchComplete(const GURL& url, const SkBitmap* bitmap) = 0;

 protected:
  virtual ~BitmapFetcherDelegate() {}
};

#endif  // CHROME_BROWSER_BITMAP_FETCHER_BITMAP_FETCHER_DELEGATE_H_
