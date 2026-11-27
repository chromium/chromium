// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DINO_DINO_BOOKMARK_IPH_CONTROLLER_H_
#define CHROME_BROWSER_DINO_DINO_BOOKMARK_IPH_CONTROLLER_H_

class BrowserWindowInterface;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace dino {

inline constexpr char kDinoUrl[] = "chrome://dino/";

// Returns true if `web_contents` satisfies the arm-neutral browser-side
// eligibility conditions for the offline Dino game-over bookmark IPH
// (enterprise promotions policy enabled, visible offline error page, non-OTR
// profile, bookmarks editable, and `chrome://dino/` not already bookmarked).
// Intentionally does not check `kDinoDelight` or the treatment-only show
// counter so eligibility can be evaluated identically across experiment arms.
bool CanShowDinoBookmarkPrompt(content::WebContents* web_contents);

// Checks `kDinoDelight`, `CanShowDinoBookmarkPrompt()`, and the per-profile
// `kDinoBookmarkPromptMaxTimesShownCounter` cap, and if all pass, requests User
// Education to show `kIPHDelightDinoBookmarkFeature`. Increments
// `kDinoBookmarkPromptMaxTimesShownCounter` when the promo is actually shown.
void MaybeTriggerDinoBookmarkPrompt(content::WebContents* web_contents);

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
