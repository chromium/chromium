// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_AVATAR_ICON_UTIL_H_
#define CHROME_BROWSER_PROFILES_PROFILE_AVATAR_ICON_UTIL_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <unordered_set>

#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"

namespace base {
class FilePath;
class ListValue;
}

namespace gfx {
class Image;
}

class ProfileAttributesEntry;
class SkBitmap;

namespace profiles {

#if defined(OS_WIN)
// The avatar badge size needs to be half of the shortcut icon size because
// the Windows taskbar icon is 32x32 and the avatar icon overlay is 16x16. So to
// get the shortcut avatar badge and the avatar icon overlay to match up, we
// need to preserve those ratios when creating the shortcut icon.
const int kShortcutIconSizeWin = 48;
const int kProfileAvatarBadgeSizeWin = kShortcutIconSizeWin / 2;
#endif  // OS_WIN

// Avatar access.
extern const char kGAIAPictureFileName[];
extern const char kHighResAvatarFolderName[];

// Avatar formatting.
extern const int kAvatarIconSize;
extern const SkColor kAvatarTutorialBackgroundColor;
extern const SkColor kAvatarTutorialContentTextColor;
extern const SkColor kAvatarBubbleAccountsBackgroundColor;
extern const SkColor kAvatarBubbleGaiaBackgroundColor;
extern const SkColor kUserManagerBackgroundColor;

// Avatar shape.
enum AvatarShape {
  SHAPE_CIRCLE,  // Only available for desktop platforms
  SHAPE_SQUARE,
};

// Returns the default guest avatar.
gfx::ImageSkia GetGuestAvatar(int size = 256);

// Returns a version of |image| of a specific size. Note that no checks are
// done on the width/height so make sure they're reasonable values; in the
// range of 16-256 is probably best.
gfx::Image GetSizedAvatarIcon(const gfx::Image& image,
                              bool is_rectangle,
                              int width,
                              int height,
                              AvatarShape shape);

gfx::Image GetSizedAvatarIcon(const gfx::Image& image,
                              bool is_rectangle,
                              int width,
                              int height);

// Returns a version of |image| suitable for use in WebUI.
gfx::Image GetAvatarIconForWebUI(const gfx::Image& image,
                                 bool is_rectangle);

// Returns a version of |image| suitable for use in title bars. The returned
// image is scaled to fit |dst_width| and |dst_height|.
gfx::Image GetAvatarIconForTitleBar(const gfx::Image& image,
                                    bool is_rectangle,
                                    int dst_width,
                                    int dst_height);

#if defined(OS_MACOSX)
// Returns the image for the profile at |profile_path| that is suitable for use
// in the macOS menu bar.
gfx::Image GetAvatarIconForNSMenu(const base::FilePath& profile_path);
#endif

// Returns a bitmap with a couple of columns shaved off so it is more square,
// so that when resized to a square aspect ratio it looks pretty.
SkBitmap GetAvatarIconAsSquare(const SkBitmap& source_bitmap, int scale_factor);

// Gets the number of default avatar icons that exist.
size_t GetDefaultAvatarIconCount();

// Gets the number of generic avatar icons that exist.
size_t GetGenericAvatarIconCount();

// Gets the index for the (grey silhouette) avatar used as a placeholder.
size_t GetPlaceholderAvatarIndex();

// Gets the start index of the modern profile avatar icons.
size_t GetModernAvatarIconStartIndex();

// Returns whether |icon_index| corresponds to one of the modern profile avatar
// icons.
bool IsModernAvatarIconIndex(size_t icon_index);

// Gets the resource ID of the placeholder avatar icon.
int GetPlaceholderAvatarIconResourceID();

// Returns a URL for the placeholder avatar icon.
std::string GetPlaceholderAvatarIconUrl();

// Gets the resource ID of the default avatar icon at |index|.
int GetDefaultAvatarIconResourceIDAtIndex(size_t index);

// Gets the resource filename of the default avatar icon at |index|.
const char* GetDefaultAvatarIconFileNameAtIndex(size_t index);

// Gets the resource ID of the default avatar label at |index|.
int GetDefaultAvatarLabelResourceIDAtIndex(size_t index);

// Gets the full path of the high res avatar icon at |index|.
base::FilePath GetPathOfHighResAvatarAtIndex(size_t index);

// Returns a URL for the default avatar icon with specified index.
std::string GetDefaultAvatarIconUrl(size_t index);

// Checks if |index| is a valid avatar icon index
bool IsDefaultAvatarIconIndex(size_t index);

// Checks if the given URL points to one of the default avatar icons. If it
// is, returns true and its index through |icon_index|. If not, returns false.
bool IsDefaultAvatarIconUrl(const std::string& icon_url, size_t *icon_index);

// Returns a list of dictionaries containing the default profile avatar icons as
// well as avatar labels used for accessibility purposes. The list is ordered
// according to the avatars' default order. If |selected_avatar_idx| is one of
// the available indices, the corresponding avatar is marked as selected.
std::unique_ptr<base::ListValue> GetDefaultProfileAvatarIconsAndLabels(
    size_t selected_avatar_idx = SIZE_MAX);

// This method tries to find a random avatar index that is not in
// |used_icon_indices|. If there is no such index, a random index is returned.
size_t GetRandomAvatarIconIndex(
    const std::unordered_set<size_t>& used_icon_indices);

#if defined(OS_WIN)
// Get the 1x and 2x avatar images for a ProfileAttributesEntry.
void GetWinAvatarImages(ProfileAttributesEntry* entry,
                        SkBitmap* avatar_image_1x,
                        SkBitmap* avatar_image_2x);

// Badges |app_icon_bitmap| with |avatar_bitmap| at the bottom right corner and
// returns the resulting SkBitmap.
SkBitmap GetBadgedWinIconBitmapForAvatar(const SkBitmap& app_icon_bitmap,
                                         const SkBitmap& avatar_bitmap,
                                         int scale_factor);
#endif  // OS_WIN

}  // namespace profiles

#endif  // CHROME_BROWSER_PROFILES_PROFILE_AVATAR_ICON_UTIL_H_
