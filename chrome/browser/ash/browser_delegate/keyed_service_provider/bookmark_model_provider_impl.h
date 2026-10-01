// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_BOOKMARK_MODEL_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_BOOKMARK_MODEL_PROVIDER_IMPL_H_

#include "chromeos/ash/components/bookmarks/bookmark_model_provider.h"

class AccountId;

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace ash {

// //chrome-side implementation of BookmarkModelProvider. Wraps
// //chrome/browser/bookmarks's Profile-keyed BookmarkModelFactory so ChromeOS
// callers can reach the model through the chromeos-side interface.
class BookmarkModelProviderImpl : public BookmarkModelProvider {
 public:
  BookmarkModelProviderImpl();
  BookmarkModelProviderImpl(const BookmarkModelProviderImpl&) = delete;
  BookmarkModelProviderImpl& operator=(const BookmarkModelProviderImpl&) =
      delete;
  ~BookmarkModelProviderImpl() override;

  // BookmarkModelProvider:
  bookmarks::BookmarkModel* Find(const AccountId& account_id) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_BOOKMARK_MODEL_PROVIDER_IMPL_H_
