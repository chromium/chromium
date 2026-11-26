// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DINO_DINO_BOOKMARK_IPH_CONTROLLER_H_
#define CHROME_BROWSER_DINO_DINO_BOOKMARK_IPH_CONTROLLER_H_

class BrowserWindowInterface;
class Profile;

namespace dino {

inline constexpr char kDinoUrl[] = "chrome://dino/";

// Saves `chrome://dino/` to the profile's bookmarks bar (preferring account
// bookmarks when available) when `kDinoDelight` is enabled.
// Returns true if the bookmark was added.
bool SaveDinoBookmarkFromPromo(Profile* profile);

// If `kDinoDelight` is enabled and `kIPHDelightDinoBookmarkFeature` is active
// in `browser`, marks the IPH feature as used (closing the promo bubble) and
// saves `chrome://dino/` to the bookmarks bar. Returns true if the bookmark
// was added so the caller can show the standard Bookmark Added bubble for
// `chrome://dino/`.
bool MaybeBookmarkDinoFromIph(BrowserWindowInterface* browser);

}  // namespace dino

#endif  // CHROME_BROWSER_DINO_DINO_BOOKMARK_IPH_CONTROLLER_H_
