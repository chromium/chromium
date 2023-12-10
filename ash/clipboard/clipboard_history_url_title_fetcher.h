// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_HISTORY_URL_TITLE_FETCHER_H_
#define ASH_CLIPBOARD_CLIPBOARD_HISTORY_URL_TITLE_FETCHER_H_

#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"

class GURL;

namespace ash {

// A fetcher implemented in the browser that queries the primary user profile's
// browsing history for URLs. Clipboard history can only display page titles for
// URLs that the profile has already visited.
class ASH_EXPORT ClipboardHistoryUrlTitleFetcher {
 public:
  virtual ~ClipboardHistoryUrlTitleFetcher();

  // Returns the singleton fetcher instance.
  static ClipboardHistoryUrlTitleFetcher* Get();

  // Queries the primary user profile's browsing history for `url`. `callback`
  // is run asynchronously with the title of the page `url` points to if the
  // profile has visited the `url`; otherwise, it is run with an absent result.
  using OnHistoryQueryCompleteCallback =
      base::OnceCallback<void(std::optional<std::u16string>)>;
  virtual void QueryHistory(const GURL& url,
                            OnHistoryQueryCompleteCallback callback) = 0;

 protected:
  ClipboardHistoryUrlTitleFetcher();
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_URL_TITLE_FETCHER_H_
